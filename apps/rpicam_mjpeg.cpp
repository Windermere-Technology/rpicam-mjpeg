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
#include <ctime>
#include <iomanip>
#include <string>
#include <csignal> 
#include <atomic>
#include <system_error>

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
		commands["wb"] = std::bind(&RPiCamMjpegApp::wb_handle, this, std::placeholders::_1);
	}

	~RPiCamMjpegApp() { cleanup(); }

	MjpegOptions *GetOptions() const { return static_cast<MjpegOptions *>(options_.get()); }

	// Declare Encoder and FileOutput as member variables
	std::unique_ptr<Encoder> h264Encoder;
	std::unique_ptr<FileOutput> h264FileOutput;

	bool preview_active;
	bool still_active;
	bool video_active;
	// TODO: Remove this variable altogether... eventually
	bool multi_active;
	bool fifo_active() const { return !GetOptions()->fifo.empty(); }
	std::optional<std::string> error = std::nullopt;

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
		if (video_active)
			return "video"; // recording
		if (preview_active)
			return "ready"; // preview only
		return "halted"; // nothing
	}

	// Report the application status to --status-output file.
	void WriteStatus()
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
		else if (preview_active || still_active)
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
		}
	}


	// FIXME: This name is terrible!
	// TODO: It'd be nice to integrate this will app.Wait(), but that probably requires a decent refactor *~*
	std::string GetFifoCommand()
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
			throw std::runtime_error("Expected three arguments to `pv` command");
		

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

	void wb_handle(std::vector<std::string> args)
	{
		using namespace libcamera;

		if (args.size() != 1)
			throw std::runtime_error("expected exactly one argument to `wb` command");

		std::string awb = args[0];
		// NOTE: This will throw if we got an unhandled value.
		auto options = GetOptions();
		options->SetAwb(awb);

		StopCamera();
		Teardown();
		Configure(options);
		StartCamera();
	}
};

static void preview_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info,
						 libcamera::ControlList const &metadata, std::string const &filename,
						 std::string const &cam_model, StillOptions const *options)
{
	jpeg_save(mem, info, metadata, filename, cam_model, options, options->width, options->height);
}

static void still_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info,
					   libcamera::ControlList const &metadata, std::string const &filename,
					   std::string const &cam_model, StillOptions const *options, libcamera::Size outputSize)
{
	// Add the datetime to the filename.
	std::string output_filename;
	{
		// The part before the extension (if one exists)
		size_t period_index = filename.rfind(".");
		if (period_index == std::string::npos)
			period_index = filename.length();
		std::string name = filename.substr(0, period_index);
		std::time_t now = std::time(nullptr);
		// The date/timestamp
		std::stringstream buffer;
        buffer << std::put_time(std::localtime(&now), "%Y%m%d%H%M%S"); // Use &now instead of &t
        std::string timestamp = buffer.str(); // Get the string from the buffer

		// Extension
        std::string extension = filename.substr(period_index, filename.length());
        output_filename = name + timestamp + extension;
		
	}

	jpeg_save(mem, info, metadata, output_filename, cam_model, options, outputSize.width, outputSize.height);
	LOG(1, "Saved still capture: " + output_filename);
};

// video_save function using app to manage encoder and file output
static void video_save(RPiCamMjpegApp &app, const std::vector<libcamera::Span<uint8_t>> &mem, const StreamInfo &info,
					   const libcamera::ControlList &metadata, const std::string &filename,
					   const std::string &cam_model, VideoOptions &options,
					   const CompletedRequestPtr &completed_request, Stream *stream)
{   

	// Use the app instance to call initialize_encoder
	app.initialize_encoder(options, info);

	// Check if the encoder and file output were successfully initialized
	if (!app.h264Encoder)
	{
		LOG_ERROR("Failed to initialize encoder.");
		return;
	}

	if (!app.h264FileOutput)
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
	app.h264Encoder->EncodeBuffer(fd, mem[0].size(), mem[0].data(), info, timestamp_us);
}

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
	app.video_active = !options->videoOptions.output.empty();
	app.multi_active = ((int)app.preview_active + (int)app.still_active + (int)app.video_active) > 1;

	app.OpenCamera();
	app.Configure(options);
	app.StartCamera();

	// If accepting external commands, wait for them before running video/still captures.
	if (app.fifo_active()) {
		app.video_active = false;
		app.still_active = false;
	}

	// -1 indicates indefinte recording (until `ca 0` recv'd.)
	int duration_limit_seconds = options->fifo.empty() ? 5 : -1;
	auto start_time = std::chrono::steady_clock::now();

	while (app.video_active || app.preview_active || app.still_active || app.fifo_active())
	{
		// Check if there are any commands over the FIFO.
		std::string fifo_command = app.GetFifoCommand();
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

		app.WriteStatus();

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
				still_save(viewfinder_mem, viewfinder_info, completed_request->metadata, options->stillOptions.output,
						   app.CameraModel(), &options->stillOptions, libcamera::Size(3200, 2400));
				
				LOG(2, "Still image saved");
				app.still_active = false;
			}
			else if (app.preview_active || app.multi_active)
			{
				// Save preview if not in still mode
				StillOptions opts = options->previewOptions;
				// If opts.width == 0, we should use "the default"
				opts.width = (opts.width >= 128 && opts.width <= 1024) ? opts.width : 512;

				// Copied from RaspiMJPEG ;)
				unsigned int height = (unsigned long int) opts.width*viewfinder_info.height/viewfinder_info.width;
				height -= height%16;
				opts.height = height;

				preview_save(viewfinder_mem, viewfinder_info, completed_request->metadata, opts.output,
							 app.CameraModel(), &opts);
				LOG(2, "Viewfinder (Preview) image saved");
			}
		}

		// Process the VideoRecording stream
		if (app.VideoStream())
		{
			Stream *video_stream = app.VideoStream();
			StreamInfo video_info = app.GetStreamInfo(video_stream);
			BufferReadSync r(&app, completed_request->buffers[video_stream]);
			const std::vector<libcamera::Span<uint8_t>> video_mem = r.Get();

			if (app.video_active)
			{
				video_save(app, video_mem, video_info, completed_request->metadata, options->videoOptions.output,
						   app.CameraModel(), options->videoOptions, completed_request, video_stream);
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
         // Initialize the resolution checker and print available video modes - for future use
        CameraResolutionChecker checker;
        std::pair<int, int> highestResolution = checker.getHighestVideoResolution();
        std::cout << "Highest video resolution: " << highestResolution.first << "x" << highestResolution.second << std::endl;

		RPiCamMjpegApp app;
		try
		{
			MjpegOptions *options = app.GetOptions();

			if (options->Parse(argc, argv))
			{
				if (options->verbose >= 2)
					options->Print();
				if (options->previewOptions.output.empty() && options->stillOptions.output.empty() &&
					options->videoOptions.output.empty())
					throw std::runtime_error(
						"At least one of --preview-output, --still-output or --video-output should be provided.");

				event_loop(app);
			}
		}
		catch (std::exception const &e)
		{
			app.error = e.what();
			app.WriteStatus();
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
