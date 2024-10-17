/* SPDX-License-Identifier: BSD-2-Clause */
/*
* Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
* Copyright (C) 2024, Dylan Lom
* TODO: If people want to stick their names here they can!
*
* rpicam_jpeg.cpp - minimal libcamera jpeg capture app.
*/
#include <cassert>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <string>
#include <csignal> 
#include <atomic>
#include <system_error>
#include <regex>
#include <filesystem>
#include <fstream>

//for mapping fifo
#include <map>
#include <vector>
#include <functional>
#include <iostream>
#include <variant>
//

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "core/mjpeg_options.hpp"
#include "core/rpicam_app.hpp"
#include "image/image.hpp"
#include <libcamera/control_ids.h>

// video recording
#include "core/rpicam_encoder.hpp"
#include "encoder/encoder.hpp"
#include "output/file_output.hpp"
//check camera resolution
#include "cameraResolutionChecker.hpp"

//check camera resolution
#include "cameraResolutionChecker.hpp"

// motion detection
#include "post_processing_stages/motion_detect_stage.cpp"

using namespace std::placeholders;
using libcamera::Stream;

std::atomic<bool> stopRecording(false); // Global flag to indicate when to stop recording(Ctrl C)

void signal_handler(int signal) // signal handler
{
	if (signal == SIGINT)
	{
		stopRecording = true; // Set the flag to true when SIGINT is caught
	}
}

class RPiCamMjpegApp : public RPiCamApp
{
public:
	std::map<std::string, std::function<void(const std::vector<std::string>&, std::chrono::time_point<std::chrono::steady_clock>&, int&)>> commands;
	RPiCamMjpegApp() : RPiCamApp(std::make_unique<MjpegOptions>()) {
		commands["im"] = std::bind(&RPiCamMjpegApp::im_handle, this);
		commands["ca"] = std::bind(&RPiCamMjpegApp::ca_handle, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		commands["pv"] = std::bind(&RPiCamMjpegApp::pv_handle, this, std::placeholders::_1);
		commands["ro"] = std::bind(&RPiCamMjpegApp::ro_handle, this, std::placeholders::_1);
		commands["fl"] = std::bind(&RPiCamMjpegApp::fl_handle, this, std::placeholders::_1);
		commands["sc"] = std::bind(&RPiCamMjpegApp::set_counts, this);
		commands["md"] = std::bind(&RPiCamMjpegApp::md_handle, this, std::placeholders::_1);
		commands["wb"] = std::bind(&RPiCamMjpegApp::wb_handle, this, std::placeholders::_1);
		commands["mm"] = std::bind(&RPiCamMjpegApp::mm_handle, this, std::placeholders::_1);
		commands["ec"] = std::bind(&RPiCamMjpegApp::ec_handle, this, std::placeholders::_1);
		commands["ag"] = std::bind(&RPiCamMjpegApp::ag_handle, this, std::placeholders::_1);
		commands["is"] = std::bind(&RPiCamMjpegApp::is_handle, this, std::placeholders::_1);
		commands["px"] = std::bind(&RPiCamMjpegApp::px_handle, this, std::placeholders::_1); // video resolution
		commands["co"] = std::bind(&RPiCamMjpegApp::co_handle, this, std::placeholders::_1);
		commands["br"] = std::bind(&RPiCamMjpegApp::br_handle, this, std::placeholders::_1);
		commands["sa"] = std::bind(&RPiCamMjpegApp::sa_handle, this, std::placeholders::_1);
		commands["qu"] = std::bind(&RPiCamMjpegApp::qu_handle, this, std::placeholders::_1);
		commands["bi"] = std::bind(&RPiCamMjpegApp::bi_handle, this, std::placeholders::_1);
		commands["sh"] = std::bind(&RPiCamMjpegApp::sh_handle, this, std::placeholders::_1);

	}

	~RPiCamMjpegApp() { cleanup(); }

	MjpegOptions *GetOptions() const { return static_cast<MjpegOptions *>(options_.get()); }

	// Declare Encoder and FileOutput as member variables
	std::unique_ptr<Encoder> h264Encoder;
	std::unique_ptr<FileOutput> h264FileOutput;
	std::unique_ptr<MotionDetectStage> motionDetectStage;

	bool preview_active;
	bool still_active;
	bool video_active;
	bool motion_active;
	bool firstTime = true; 	// helper var for motion detect
	// TODO: Remove this variable altogether... eventually
	bool multi_active;
	bool fifo_active() const { return !GetOptions()->fifo.empty(); }
	std::optional<std::string> error = std::nullopt;

	int image_count = 0; // still and timelapse
	int video_count = 0;

	// Get the application "status": https://github.com/roberttidey/userland/blob/e2b8cd0c80902d6aeb4f157c3cf1b1f61446b061/host_applications/linux/apps/raspicam/README_RaspiMJPEG.md
	std::string status()
	{
		if (error)
			return std::string("Error: ") + *error;

		// NOTE: Considering that RaspiMJPEG would interrupt the video recording to
		// take a still image, we are saying that the status is "image" whenever still
		// is active, even though we might also be recording a video.
		if (still_active)
			return "image"; // saving still
		if (motion_active && video_active)
			return "md_video"; // motion detection and video recording
		if (video_active)
			return "video"; // recording
		if (motion_active)
			return "md_ready"; // motion detection
		if (preview_active)
			return "ready"; // preview only
		return "halted"; // nothing
	}

	// Report the application status to --status-output file.
	void write_status()
	{
		std::string status_output = GetOptions()->status_output;
		if (status_output.empty())
			return;
		std::ofstream stream(status_output);
		stream << status();
	}


	void Configure(MjpegOptions *options)
	{
		if (multi_active)
		{
			// Call the multi-stream configuration function
			ConfigureMultiStream(options->stillOptions, options->videoOptions, options->previewOptions, 0);
		}
		else if (video_active)
		{
			ConfigureVideo();
		}
		else if (preview_active || still_active || motion_active)
		{
			ConfigureViewfinder();
		}
	}

	// Function to initialize the encoder and file output
	void initialize_encoder(VideoOptions &videoOptions, const StreamInfo &info)
	{
		if (!h264Encoder)
		{
			LOG(1, "Initializing encoder...");
			h264Encoder = std::unique_ptr<Encoder>(
				Encoder::Create(&videoOptions, info)); // Properly wrap the raw pointer into unique_ptr
			if (!h264Encoder)
			{
				LOG_ERROR("Failed to create encoder.");
				return;
			}
		}

		if (!h264FileOutput)
		{
			LOG(1, "Initializing FileOutput...");
			h264FileOutput = std::make_unique<FileOutput>(&videoOptions); // Pass the VideoOptions object
		}

		// Set encoder callbacks (if not already set)
		h264Encoder->SetInputDoneCallback([](void *buffer) { 
            // LOG(1, "Input buffer done."); 
        });

		h264Encoder->SetOutputReadyCallback(
			[this](void *data, size_t size, int64_t timestamp, bool keyframe)
			{
				LOG(1, "Output ready: size = " << size << ", timestamp = " << timestamp);
				h264FileOutput->OutputReady(data, size, timestamp, keyframe);
			});
	}

	void initialize_motion_detect_stage()
	{
		if (motionDetectStage != nullptr)
			return;

		MjpegOptions *options = GetOptions();
		(void)options;

		motionDetectStage = std::make_unique<MotionDetectStage>(this);
		// Create an instance of MotionDetectStage
		motionDetectStage->UseViewfinder(true);

		using namespace boost::property_tree;
		ptree motion_detect_parameters;

		motion_detect_parameters.push_back(ptree::value_type("roi_x", "0.1"));
		motion_detect_parameters.push_back(ptree::value_type("roi_y", "0.1"));
		motion_detect_parameters.push_back(ptree::value_type("roi_width", "0.8"));
		motion_detect_parameters.push_back(ptree::value_type("roi_height", "0.8"));
		motion_detect_parameters.push_back(ptree::value_type("difference_m", "0.1"));
		motion_detect_parameters.push_back(ptree::value_type("difference_c", "10"));
		motion_detect_parameters.push_back(ptree::value_type("region_threshold", "0.005"));
		motion_detect_parameters.push_back(ptree::value_type("frame_period", "3"));
		motion_detect_parameters.push_back(ptree::value_type("hskip", "1"));
		motion_detect_parameters.push_back(ptree::value_type("vskip", "1"));
		motion_detect_parameters.push_back(ptree::value_type("verbose", "0"));

		motionDetectStage->Read(motion_detect_parameters);
		motionDetectStage->Configure();
	}

	void cleanup_motion_detect_stage() { motionDetectStage.reset(); }

	void cleanup()
	{
		if (h264Encoder)
		{
			LOG(1, "Cleaning up encoder...");
			h264Encoder.reset(); // This will call the destructor of the encoder and release its resources
		}

		if (h264FileOutput)
		{
			LOG(1, "Cleaning up file output...");
			h264FileOutput.reset(); // Free the file output resources
			auto options = GetOptions();
			// NOTE: videoOptions.output contains the generate file name (make_name).
			thumbnail_save(options->videoOptions.output, 'v');
			options->videoOptions.output = "";
			video_count++;
		}
	}


	// FIXME: This name is terrible!
	// TODO: It'd be nice to integrate this will app.Wait(), but that probably requires a decent refactor *~*
	std::string get_fifo_command()
	{
		static std::string fifo_path = GetOptions()->fifo;
		static int fd = -1;

		if (fifo_path == "")
			return "";

		// NOTE: On the first read the FIFO will be blocking if we using normal
		// C++ I/O (ifstream); so instead we create the FD ourselves so we can set
		// the O_NONBLOCK flag :)
		if (fd == -1) {
			fd = open(fifo_path.c_str(), O_RDONLY | O_NONBLOCK);
			if (fd < 0) throw std::system_error(errno, std::generic_category(), fifo_path);
		}

		// FIXED: buffered reader minimizes number of function calls to read
		std::string command = "";
		char buffer[32];
		int bytes_read;
		while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
			if (buffer[bytes_read - 1] == '\n') {
				command.append(buffer, bytes_read - 1);
				break;
			}
			command.append(buffer, bytes_read);
		}

		return command;
	}

	void ro_handle(std::vector<std::string> args)
	{
		using namespace libcamera;

		if (args.size() > 1)
			throw std::runtime_error("expected at most 1 argument to `ro` command");

		// Default (no arguments) is 0 degrees.
		int rotation = args.size() == 0 ? 0 : std::stoi(args[0]) % 360;

		if (rotation != 0 && rotation != 180)
		{
			// https://github.com/raspberrypi/rpicam-apps/issues/505
			throw std::runtime_error("transforms requiring transpose not supported");
		}

		bool ok;
		Transform rot = transformFromRotation(rotation, &ok);
		if (!ok)
			throw std::runtime_error("unsupported rotation value: " + args[0]);

		auto options = GetOptions();
		options->SetRotation(rot);

		// FIXME: Can we avoid resetting everything?
		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}

	void fl_handle(std::vector<std::string> args)
	{
		using namespace libcamera;
		if (args.size() > 1)
			throw std::runtime_error("expected at most 1 argument to `fl` command");

		// Default 0.
		int value = args.size() == 0 ? 0 : std::stoi(args[0]);
		// Set horisontal flip(hflip) and vertical flip(vflip). 0={hflip=0,vflip=0}, 1={hflip=1,vflip=0}, 2={hflip=0,vflip=1}, 3={hflip=1,vflip=1}, default: 0
		bool hflip = value & 1;
		bool vflip = value & 2;

		auto options = GetOptions();

		Transform flip = Transform::Identity;
		if (hflip)
			flip = Transform::HFlip * flip;
		if (vflip)
			flip = Transform::VFlip * flip;
		options->SetFlip(flip);

		// FIXME: Can we avoid resetting everything?
		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}

	void im_handle(){
		still_active = true;
	}

	void ca_handle(std::vector<std::string> args, std::chrono::time_point<std::chrono::steady_clock>& start_time, int& duration_limit_seconds){
		if (args.size() < 1 || args[0] != "1")
			{ // ca 0, or some invalid command.
				if (video_active)  // finish up with the current recording.
					cleanup();
				video_active = false;

			}
		else
		{
			video_active = true;
			start_time = std::chrono::steady_clock::now();
			if (args.size() >= 2) {
				duration_limit_seconds = stoi(args[1]);
			} else {
				// FIXME: Magic number :)
				duration_limit_seconds = -1; // Indefinite
			}
		}
		
	}

	void pv_handle(std::vector<std::string> args)
	{
		// pv QQ WWW DD - set preview Quality, Width and Divider
		if (args.size() < 3)
			throw std::runtime_error("Expected at least three arguments to `pv` command");
		

		auto options = GetOptions();
		options->previewOptions.quality = stoi(args[0]);
		options->previewOptions.width = stoi(args[1]);
		// TODO: Use the divider to set the frame rate somehow

		StopCamera();
		Teardown();
		Configure(options);
		preview_active = true;
		StartCamera();
	}

	void md_handle(std::vector<std::string> args){
		if (args.size() < 1 || args[0] != "1")
		{ 
			motion_active = false;
		}
		else
		{
			motion_active = true;	
			firstTime = true;
			auto options = GetOptions();

			// FIXME: dont use the motion_detect.json anymore? 
			options->post_process_file = "assets/motion_detect.json";

			StopCamera();
			Teardown();
			Configure(options);
			StartCamera();
		}
	}

	void wb_handle(std::vector<std::string> args)
	{
		using namespace libcamera;

		if (args.size() != 1)
			throw std::runtime_error("expected exactly one argument to `wb` command");

		std::string awb = args[0];
		auto options = GetOptions();
		try
		{
			options->SetAwb(awb);
		}
		catch (const std::exception &e)
		{
			// We got some AWB value which libcamera does not support; ignore the command.
			LOG(1, e.what());
			return;
		}

		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}
	
	void px_handle(std::vector<std::string> args)
	{
		if (args.size() < 7) // Expecting at least 7 arguments
			throw std::runtime_error("Expected 7 arguments to `px` command: width height video_fps preview_fps image_width image_height frame_divider");

		// Parse the arguments
		int videoWidth = std::stoi(args[0]);     // Video width
		int videoHeight = std::stoi(args[1]);    // Video height
		int videoFps = std::stoi(args[2]);       // Video FPS
		int previewFps = std::stoi(args[3]);     // Preview FPS
		int imageWidth = std::stoi(args[4]);     // Image (still capture) width
		int imageHeight = std::stoi(args[5]);    // Image (still capture) height
		int frameDivider = std::stoi(args[6]);   // Frame divider

		// Set the video and image resolution in the options
		auto options = GetOptions();
		options->videoOptions.width = videoWidth;
		options->videoOptions.height = videoHeight;
		options->videoOptions.fps = videoFps;    // Set video FPS
		
		options->frameDivider = frameDivider;    // Optional: Handle this as needed

		// Log the parsed values for debugging
		LOG(1, "px command received: video=" << videoWidth << "x" << videoHeight 
			<< ", video FPS=" << videoFps << ", preview FPS=" << previewFps 
			<< ", image=" << imageWidth << "x" << imageHeight 
			<< ", frame divider=" << frameDivider);

		// Reconfigure the camera with the new resolution
		StopCamera();    // Stop the camera
		Teardown();      // Clean up the current configuration
		Configure(options); // Apply the new configuration with the updated resolution
		StartCamera();   // Restart the camera with the new settings
	}

	void mm_handle(std::vector<std::string> args){
		if (args.size() != 1)
			throw std::runtime_error("Expected only one argument for `mm` command");
		// accepts: centre, spot, average, matrix, custom
		auto options = GetOptions();
		auto new_mm_index = Options::MMLookup(args[0]);
		options->metering = args[0];
		options->metering_index = new_mm_index;
		options->videoOptions.metering = args[0];
		options->videoOptions.metering_index = new_mm_index;
		options->stillOptions.metering = args[0];
		options->stillOptions.metering_index = new_mm_index;
		options->previewOptions.metering = args[0];
		options->previewOptions.metering_index = new_mm_index;
		//options->videoOptions.Print();

		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}
  
	void co_handle(std::vector<std::string> args)
	{
		if (args.size() != 1)
			throw std::runtime_error("expected at most 1 argument to `co` command");

		float contrast = std::stof(args[0]);  // Use float for contrast

		float normalized_contrast;

		if (contrast < 0.0f) {
			// If contrast is less than 0, map it to the range [0, 1]
			normalized_contrast = (contrast + 100.0f) * (1.0f / 100.0f);
		} else if (contrast == 0.0f) {
			// If contrast is 0, set it to 1
			normalized_contrast = 1.0f;
		} else {
			// If contrast is greater than 0, map it to the range [1.0f, 15.99f]
			normalized_contrast = 1 + (contrast * 14.99f) / 100.0f;
		}

		auto options = GetOptions();
		options->contrast = std::clamp(normalized_contrast, 0.0f, 15.99f);
		LOG(1, "Contrast updated to: " << options->contrast);  // Log the updated contrast value

		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}

	void br_handle(std::vector<std::string> args)
	{
		if (args.size() != 1)
			throw std::runtime_error("expected exactly 1 argument to `br` command");

		float brightness = std::stof(args[0]);  // Use float for brightness

		// Clamp brightness to the valid range [0, 100]
		brightness = std::max(0.0f, std::min(brightness, 100.0f));

		// Convert brightness to the range [-1.0f, 1.0f]
		float normalized_brightness = (brightness / 50.0f) - 1.0f;

		auto options = GetOptions();
		options->brightness = normalized_brightness;

		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}


	void ec_handle(std::vector<std::string> args){
		if (args.size() != 1)
			throw std::runtime_error("Expected only one argument for `ec` command");
		
		auto options = GetOptions();
		float ev_comp = -1;
		try{
			ev_comp = stof(args[0]);
			ev_comp = std::max(-10.0f, std::min(ev_comp, 10.0f));
		} catch (const std::invalid_argument &e) {
			std::cerr << "Invalid argument: The provided value is not a valid number." << std::endl;
			return;
		} 

		options->ev = ev_comp;

		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}

	void ag_handle(std::vector<std::string> args){
		if (args.size() != 2)
			throw std::runtime_error("Expected only two arguments for `ag` command");
		
		auto options = GetOptions();
		float ag_red = -1;
		float ag_blue = -1;
		try{
			ag_red = stof(args[0])/100;
			ag_blue = stof(args[1])/100;
			if (ag_red < 0 || ag_blue < 0){
				throw std::invalid_argument("Negative values are not allowed.");
			}
			// options requires the sum of red and blue gain to be 2.0 although not doing it doesn't create issues
			//float epsilon = 0.00001f;
			//if ((ag_red + ag_blue - 2.0f) > epsilon) {
			//	throw std::invalid_argument("The sum of red gain and blue gain must be 2.0");
			//}
				
		} catch (const std::invalid_argument &e) {
			std::cerr << "Invalid argument: One of the values is not a valid positive number." << std::endl;
			return;
		} 
		std::string ag_br =  std::to_string(ag_red) + "," + std::to_string(ag_blue);
		options->awbgains = ag_br;
		options->awb_gain_r = ag_red;
		options->awb_gain_b = ag_blue;

		//options->videoOptions.Print();
		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}

	void is_handle(std::vector<std::string> args){
		if (args.size() != 1)
			throw std::runtime_error("Expected only one argument for `is` command");
		
		auto options = GetOptions();
		float new_gain = -1;
		try{
			new_gain = stof(args[0]);
			new_gain = std::max(100.0f, std::min(new_gain, 2000.0f));
		} catch (const std::invalid_argument &e) {
			std::cerr << "Invalid argument: The provided value is not a valid number." << std::endl;
			return;
		} 
		//according to the Raspicam-app github issue #349 iso/100 = gain
		options->gain = new_gain/100;

		//options->videoOptions.Print();
		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}
	
	void sa_handle(std::vector<std::string> args)
	{
		if (args.size() != 1)
			throw std::runtime_error("expected at most 1 argument to `sa` command");

		float saturation = std::stof(args[0]);  // Use float for contrast

		float normalized_saturation;

		if (saturation < 0.0f) {
			// If saturation is less than 0, map it to the range [0, 1]
			normalized_saturation = (saturation + 100.0f) * (1.0f / 100.0f);
		} else if (saturation == 0.0f) {
			// If saturation is 0, set it to 1
			normalized_saturation = 1.0f;
		} else {
			// If saturation is greater than 0, map it to the range [1.0f, 15.99f]
			normalized_saturation = 1 + (saturation * 14.99f) / 100.0f;
		}

		auto options = GetOptions();
		options->saturation = std::clamp(normalized_saturation, 0.0f, 15.99f);

		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}

	void ss_handle(std::vector<std::string> args)
	{
		if (args.size() != 1)
			throw std::runtime_error("expected exactly 1 argument to `ss` command");

		int shutter_speed = std::stoi(args[0]);  
		if (shutter_speed < 0)
			shutter_speed = 0;  // keep shutter speed to a positive value

		auto options = GetOptions();

		// Convert the shutter speed to a string and pass it to the set method
		options->shutter.set(std::to_string(shutter_speed));

		LOG(1, "Shutter speed updated to: " << shutter_speed << " microseconds");

		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}

	void qu_handle(std::vector<std::string> args)
	{
		if (args.size() != 1)
			throw std::runtime_error("expected exactly 1 argument to `qu` command");
	
		float quality = std::stof(args[0]);  // Use float for quality
	
		// Clamp quality to the valid range [0, 100]
		quality = std::max(0.0f, std::min(quality, 100.0f));
	
		float normalized_quality;
	
		if (quality <= 10.0f) {
			// Map quality from [0, 10] to [60, 85]
			normalized_quality = 60.0f + (quality * 2.5f);
		} else {
			// Map quality from [10, 100] to [85, 100]
			normalized_quality = 85.0f + ((quality - 10.0f) * (15.0f / 90.0f));
		}
	
		auto options = GetOptions();
		options->stillOptions.quality = std::clamp(normalized_quality, 60.0f, 100.0f);
	
		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}

	void bi_handle(std::vector<std::string> args)
	{
		if (args.size() != 1)
			throw std::runtime_error("expected exactly 1 argument to `bi` command");
	
		int bitrate = std::stoi(args[0]);  // Use int for bitrate
	
		// Ensure bitrate is non-negative
		if (bitrate < 0)
			bitrate = 0;
	
		// Clamp bitrate to the valid range [0, 25000000]
		bitrate = std::min(bitrate, 25000000);
	
		auto options = GetOptions();
		options->videoOptions.bitrate.set(std::to_string(bitrate) + "bps");
	
		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}	

	void sh_handle(std::vector<std::string> args)
	{
		if (args.size() != 1)
			throw std::runtime_error("expected at most 1 argument to `sh` command");

		float sharpness = std::stof(args[0]);  // Use float for contrast

		float normalized_sharpness;

		if (sharpness < 0.0f) {
			// If sharpness is less than 0, map it to the range [0, 1]
			normalized_sharpness = (sharpness + 100.0f) * (1.0f / 100.0f);
		} else if (sharpness == 0.0f) {
			// If sharpness is 0, set it to 1
			normalized_sharpness = 1.0f;
		} else {
			// If sharpness is greater than 0, map it to the range [1.0f, 15.99f]
			normalized_sharpness = 1 + (sharpness * 14.99f) / 100.0f;
		}

		auto options = GetOptions();
		options->sharpness = std::clamp(normalized_sharpness, 0.0f, 15.99f);

		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}

	void preview_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info,
					libcamera::ControlList const &metadata)
	{
		StillOptions *options = &GetOptions()->previewOptions;
		std::string const filename = options->output;
		std::string const &cam_model = CameraModel();

		// If opts.width == 0, we should use "the default"
		options->width = (options->width >= 128 && options->width <= 1024) ? options->width : 512;

		// Copied from RaspiMJPEG ;)
		unsigned int height = (unsigned long int)options->width * info.height / info.width;
		height -= height % 16;
		options->height = height;

		jpeg_save(mem, info, metadata, filename, cam_model, options, options->width, options->height);
	}

	void still_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info,
						libcamera::ControlList const &metadata, libcamera::Size outputSize)
	{
		StillOptions const *options = &GetOptions()->stillOptions;
		std::string const filename = make_name(options->output);
		std::string const &cam_model = CameraModel();
		jpeg_save(mem, info, metadata, filename, cam_model, options, outputSize.width, outputSize.height);
		LOG(1, "Saved still capture: " + filename);
		thumbnail_save(filename, 'i');
		image_count++;
	};

	// video_save function using app to manage encoder and file output
	void video_save(const std::vector<libcamera::Span<uint8_t>> &mem, const StreamInfo &info,
				const libcamera::ControlList &metadata, const CompletedRequestPtr &completed_request,
				Stream *stream)
	{
		MjpegOptions *options = GetOptions();

		// FIXME: This is a big ol' hack, since the Encoder family takes output file name from VideoOptions
		// - We need to retain the original output name for future make_name calls.
		// - We need to retain the result of make_name for future thumbnail_save calls.
		if (options->videoOptions.output.empty())
			options->videoOptions.output = make_name(options->video_output);

		// Use the app instance to call initialize_encoder
		initialize_encoder(options->videoOptions, info);

		// Check if the encoder and file output were successfully initialized
		if (!h264Encoder)
		{
			LOG_ERROR("Failed to initialize encoder.");
			return;
		}

		if (!h264FileOutput)
		{
			LOG_ERROR("Failed to initialize file output.");
			return;
		}

		// Get buffer to process
		auto buffer = completed_request->buffers[stream];
		int fd = buffer->planes()[0].fd.get(); // File descriptor of buffer
		auto ts = metadata.get(libcamera::controls::SensorTimestamp);
		int64_t timestamp_us = ts ? (*ts / 1000) : (buffer->metadata().timestamp / 1000);

		// Ensure buffer is valid before encoding
		if (mem.empty() || mem[0].size() == 0)
		{
			LOG_ERROR("Buffer is empty, cannot proceed.");
			return;
		}

		// Encode the buffer using the H.264 encoder
		//LOG(1, "Encoding buffer of size " << mem[0].size() << " at timestamp " << timestamp_us);
		h264Encoder->EncodeBuffer(fd, mem[0].size(), mem[0].data(), info, timestamp_us);
	}

	// motion detect function
	bool detected_ = false;
	bool detected = false;
	void motion_detect(CompletedRequestPtr &completed_request)
	{
		initialize_motion_detect_stage();
		assert(motionDetectStage != nullptr);

		motionDetectStage->Process(completed_request);
		
		completed_request->post_process_metadata.Get("motion_detect.result", detected);
		std::string msg = detected ? "1" : "0";
		static std::ofstream scheduler {GetOptions()->motion_output};
		
		if (detected_ != detected) 
		{
			scheduler << msg << std::endl;
		}

		detected_ = detected;
	}

	void set_counts()
	{
		MjpegOptions *options = GetOptions();

		auto get_highest_by_type = [](std::string filename, std::string types) {
			auto base = filename.substr(0, filename.rfind("/"));

			std::vector<std::string> filenames;
			for (auto const& dir_entry : std::filesystem::directory_iterator(base)) {
				if (!dir_entry.is_regular_file()) continue;
				filenames.push_back(dir_entry.path());
			}

			// Extract max <count> from Vec of "<name>.<type><count>.th.jpg"
			int max_count = 0;
			std::regex thumbnail_regex(".*.(t|i|v)([0-9]+).th.jpg");
			std::smatch thumbnail_match;
			for (auto &filename : filenames) {
				// File didn't match thumbnail format
				if (!std::regex_search(filename, thumbnail_match, thumbnail_regex))
					continue;
				std::string type = thumbnail_match[1];

				// File didn't match required types
				if (types.find(type) == std::string::npos)
					continue;

				int count = std::stoi(thumbnail_match[2]);
				if (count > max_count) max_count = count;
			}
			return max_count;
		};

		if (!options->stillOptions.output.empty()) {
			std::string example_filename = make_name(options->stillOptions.output);
			image_count = get_highest_by_type(example_filename, "it") + 1;
		}

		if (!options->video_output.empty()) {
			std::string example_filename = make_name(options->video_output);
			video_count = get_highest_by_type(example_filename, "v") + 1;
		}
	}

	void thumbnail_save(std::string filename, char type)
	{
		assert((type == 'v' || type == 'i' || type == 't') && "Type must be one of v, i, t.");

		MjpegOptions const *options = GetOptions();
		if (options->media_path.empty()) return;
		if (options->thumb_gen.empty()) return;
		if (options->previewOptions.output.empty()) return;

		// Thumbnail generation for this type is disabled.
		if (options->thumb_gen.find(type) == std::string::npos)
			return;

		// Only generate thumbnails for files saved at the media path.
		if (filename.rfind(options->media_path, 0) == std::string::npos)
			return;

		int count = type == 'v' ? video_count : image_count;
		// TODO: We are supposed to replace subdirectories relative to media_path with options->subdir_char.
		// - ie. /var/www/media/my/sub/directory/img.jpg should generate thumbnail /var/www/media/my@sub@directory@img.jpg.i1.th.jpg

		std::stringstream buffer;
		buffer << filename << "." << type << count << ".th.jpg";

		// Use the current preview as the thumbnail.
		std::string preview_filename = options->previewOptions.output;
		std::string thumbnail_filename = buffer.str();
		std::ifstream preview(preview_filename, std::ios::binary);
		std::ofstream thumbnail(thumbnail_filename, std::ios::binary);
		thumbnail << preview.rdbuf();

		LOG(2, "Saved thumbnail to " << thumbnail_filename);
	}

	std::string make_name(const std::string format, const bool is_filename = true)
	{
		auto options = GetOptions();
		time_t tt = time(nullptr);
		struct tm *ptm = localtime(&tt);
		std::stringstream buffer;

		// Filenames are assumed to be relative to media_path if not absolute.
		if (is_filename && format.find('/') != 0 && !options->media_path.empty())
			buffer << options->media_path << "/";

		// Handle the format string
		size_t pos, previous_pos = 0;

		while ((pos = format.find('%', previous_pos)) != std::string::npos) {
			// copy leading non format stuff.
			buffer << format.substr(previous_pos, pos - previous_pos);

			// Edge case: The string terminates in a %. 🙃
			if (pos == format.size()) {
				buffer << '%';
				break;
			}

			switch (format[pos + 1])
			{
			case '%': // literal %
			case 'Y': // 4 dig. year
			case 'y': // 2 dig. year
				buffer << std::put_time(ptm, format.substr(pos, 2).c_str());
				break;
			case 'M': // 2 dig. month
				buffer << std::put_time(ptm, "%m");
				break;
			case 'D': // day of month
				buffer << std::put_time(ptm, "%d");
				break;
			case 'h': // 24 hour
				buffer << std::put_time(ptm, "%H");
				break;
			case 'm': // minute
				buffer << std::put_time(ptm, "%M");
				break;
			case 's': // second
				buffer << std::put_time(ptm, "%S");
				break;
			// TODO: We should support count_format config option for v, i, t
			case 'v': // video #
				buffer << std::to_string(video_count);
				break;
			case 'i': // image #
			case 'l': // timelapse #
				// FIXME: roberttidey RaspiMJPEG actually does use a lapse_cnt here...
				buffer << std::to_string(image_count);
				break;
			default: // Fallback for unrecognized / unsupported
				LOG(1, "Unsupported f-string: " << format.substr(pos, 2));
				buffer << format.substr(pos, 2);
			}

			previous_pos = pos + 2; // advance past whatever we just substitued
		}

		// copy trailing non-format stuff.
		if (previous_pos != format.size())
			buffer << format.substr(previous_pos);

		return buffer.str();
	}
};

// Function to tokenize the FIFO command
std::vector<std::string> tokenizer(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    std::string token;
    std::string s = str; // Make a copy of the input string to modify

    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        tokens.push_back(token);
        s.erase(0, pos + delimiter.length());
    }

    // Add the last token
    tokens.push_back(s);

    return tokens;
}
	


// The main event loop for the application.
static void event_loop(RPiCamMjpegApp &app)
{
	MjpegOptions *options = app.GetOptions();

	// FIXME: app should probably know how to set these...
	app.preview_active = !options->previewOptions.output.empty();
	app.still_active = !options->stillOptions.output.empty();
	app.video_active = !options->video_output.empty();
	app.motion_active = !options->motion_output.empty();
	app.multi_active = ((int)app.preview_active + (int)app.still_active + (int)app.video_active) > 1;

	app.OpenCamera();
	app.Configure(options);
	app.StartCamera();

	// If accepting external commands, wait for them before running video/still captures.
	if (app.fifo_active()) {
		app.video_active = false;
		app.still_active = false;
		app.motion_active = false;
	}

	// -1 indicates indefinte recording (until `ca 0` recv'd.)
	int duration_limit_seconds = options->fifo.empty() ? 10 : -1;
	auto start_time = std::chrono::steady_clock::now();

	app.set_counts();
	LOG(2, "image_count: " << app.image_count << ", video_count: " << app.video_count);

	while (app.video_active || app.preview_active || app.still_active || app.motion_active || app.fifo_active())
	{
		// Check if there are any commands over the FIFO.
		std::string fifo_command = app.get_fifo_command();
		if (!fifo_command.empty())
		{			
			LOG(1, "Got command from FIFO: " + fifo_command);

			// Split the fifo_command by space
			std::vector<std::string> tokens = tokenizer(fifo_command, " ");
			std::vector<std::string> arguments = std::vector<std::string>(tokens.begin() + 1, tokens.end());
			// check for existing command
			auto it = app.commands.find(tokens[0]);
			if (it != app.commands.end())
			{
				app.commands[tokens[0]](arguments, start_time, duration_limit_seconds); //Call associated command handler
			}
			else
			{
				std::cout << "Invalid command: " << tokens[0] << std::endl;
			}

		}

		app.write_status();

		// If video is active and a duration is set, check the elapsed time
		if (app.video_active && duration_limit_seconds >= 0)
		{
			auto current_time = std::chrono::steady_clock::now();
			auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

			if (stopRecording)
			{
				LOG(1, "SIGINT caught. Stopping recording.");
				app.cleanup();
				app.video_active = false;  // Ensure video_active is set to false
				break; 
			}

			if (elapsed_time >= duration_limit_seconds)
			{
				std::cout << "time limit: " << duration_limit_seconds << " seconds is reached. stop." << std::endl;
				app.cleanup();
				app.video_active = false;
			}
		}

		// Exit FIFO loop if SIGINT (Ctrl+C) is caught
		if (stopRecording)
		{
			LOG(1, "SIGINT caught. Exiting FIFO loop.");
			break;  // Exit the FIFO loop when SIGINT is received
		}


		RPiCamApp::Msg msg = app.Wait();
		if (msg.type == RPiCamApp::MsgType::Timeout)
		{
			LOG_ERROR("ERROR: Device timeout detected, attempting a restart!!!");
			app.StopCamera();
			app.StartCamera();
			continue;
		}
		if (msg.type == RPiCamApp::MsgType::Quit)
			return;
		else if (msg.type != RPiCamApp::MsgType::RequestComplete)
			throw std::runtime_error("unrecognised message!");

		CompletedRequestPtr &completed_request = std::get<CompletedRequestPtr>(msg.payload);

		// Process the Viewfinder (Preview) stream
		if (app.ViewfinderStream())
		{
			Stream *viewfinder_stream = app.ViewfinderStream();
			StreamInfo viewfinder_info = app.GetStreamInfo(viewfinder_stream);
			BufferReadSync r(&app, completed_request->buffers[viewfinder_stream]);
			const std::vector<libcamera::Span<uint8_t>> viewfinder_mem = r.Get();

			if (app.still_active)
			{
				// Save still image instead of preview when still_active is set
				app.still_save(viewfinder_mem, viewfinder_info, completed_request->metadata,
                            libcamera::Size(3200, 2400));

				LOG(2, "Still image saved");
				app.still_active = false;
			}
			else if (app.preview_active || app.multi_active)
			{
				// Save preview if not in still mode
				app.preview_save(viewfinder_mem, viewfinder_info, completed_request->metadata);
				LOG(2, "Viewfinder (Preview) image saved");
			}
			if (app.motion_active)
			{
				app.motion_detect(completed_request);
			}
		}

		// Process the VideoRecording stream
		if (app.VideoStream())
		{
			// LOG(1, "here");
			Stream *video_stream = app.VideoStream();
			StreamInfo video_info = app.GetStreamInfo(video_stream);
			BufferReadSync r(&app, completed_request->buffers[video_stream]);
			const std::vector<libcamera::Span<uint8_t>> video_mem = r.Get();
			if (app.video_active)
			{
				app.video_save(video_mem, video_info, completed_request->metadata, completed_request, video_stream);
				LOG(2, "Video recorded and saved");
			}
		}
		LOG(2, "Request processing completed, current status: " + app.status());
	}
}



int main(int argc, char *argv[])
{	
	std::signal(SIGINT, signal_handler); // deal with SIGINT
	try
	{
        //  // Initialize the resolution checker and print available video modes - for future use
        // CameraResolutionChecker checker;
        // std::pair<int, int> highestResolution = checker.getHighestVideoResolution();
        // std::cout << "Highest video resolution: " << highestResolution.first << "x" << highestResolution.second << std::endl;

		RPiCamMjpegApp app;
		try
		{
			MjpegOptions *options = app.GetOptions();

			if (options->Parse(argc, argv))
			{
				if (options->verbose >= 2)
					options->Print();
				if (options->previewOptions.output.empty() && options->stillOptions.output.empty() &&
					options->video_output.empty() && options->motion_output.empty())
					throw std::runtime_error(
						"At least one of --preview-output, --still-output, --video-output, or --motion-output should be provided.");

				event_loop(app);
			}
		}
		catch (std::exception const &e)
		{
			app.error = e.what();
			app.write_status();
			throw;
		}
	}
	catch (std::exception const &e)
	{
		LOG_ERROR("ERROR: *** " << e.what() << " ***");
		return -1;
	}
	return 0;
}
