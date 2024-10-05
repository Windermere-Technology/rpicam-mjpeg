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
	void im_handle(std::vector<std::string> tokens, bool& still_active, bool& video_active, bool& preview_active){
		still_active = true;
	}
	void ca_handle(std::vector<std::string> tokens, bool& still_active, bool& video_active, bool& preview_active){
		if (tokens.size() < 2 || tokens[1] != "1")
			{ // ca 0, or some invalid command.
				if (video_active)  // finish up with the current recording.
					cleanup();
				video_active = false;

			}
		else
		{
			video_active = true;
			// start_time = std::chrono::steady_clock::now();
			// if (tokens.size() >= 3) {
			// 	duration_limit_seconds = stoi(tokens[2]);
			// } else {
			// 	// FIXME: Magic number :)
			// 	duration_limit_seconds = -1; // Indefinite
			// }
		}
		
	}
	void pv_handle(std::vector<std::string> tokens, bool& still_active, bool& video_active, bool& preview_active){
		if (tokens.size() < 3)
		{
			std::cout << "Invalid command" << std::endl;
		}
		else
		{
			preview_active = true;
			MjpegOptions *options = GetOptions();
			options->previewOptions.width = stoi(tokens[1]);
			options->previewOptions.height = stoi(tokens[2]);
		}
		
	}
	std::map<std::string, std::function<void(const std::vector<std::string>&, bool&, bool&, bool&)>> commands;
	RPiCamMjpegApp() : RPiCamApp(std::make_unique<MjpegOptions>()) {
		commands["im"] = std::bind(&RPiCamMjpegApp::im_handle, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
		commands["ca"] = std::bind(&RPiCamMjpegApp::ca_handle, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
		commands["pv"] = std::bind(&RPiCamMjpegApp::pv_handle, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
	}

	MjpegOptions *GetOptions() const { return static_cast<MjpegOptions *>(options_.get()); }

	// Declare Encoder and FileOutput as member variables
	std::unique_ptr<Encoder> h264Encoder;
	std::unique_ptr<FileOutput> h264FileOutput;


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

		// FIXME: This is inefficient, obviously...
		std::string command = "";
		char c = '\0';
		while (read(fd, &c, 1) > 0) {
			if (c == '\n') break;
			command += c;
		}

		return command;
	}
};

static void preview_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info,
						 libcamera::ControlList const &metadata, std::string const &filename,
						 std::string const &cam_model, StillOptions const *options)
{
	jpeg_save(mem, info, metadata, filename, cam_model, options, options->width, options->height);
	LOG(1, "Saved preview image: " + filename);
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

	app.OpenCamera();

	bool preview_active = !options->previewOptions.output.empty();
	bool still_active = !options->stillOptions.output.empty();
	bool video_active = !options->videoOptions.output.empty();
	// TODO: Remove this variable altogether... eventually
	bool multi_active = ((int)preview_active + (int)still_active + (int)video_active) > 1;


    if (multi_active)
    {
        // Call the multi-stream configuration function
        app.ConfigureMultiStream(
            options->stillOptions,
            options->videoOptions,
            options->previewOptions,
            0
        );
        app.StartCamera();
    }
    else if (video_active)
    {
        app.ConfigureVideo();
        app.StartCamera();
    }
    else if (preview_active || still_active)
    {
        app.ConfigureViewfinder();
        app.StartCamera();
    }

	// If accepting external commands, wait for them before running video/still captures.
	if (!options->fifo.empty()) {
		video_active = false;
		still_active = false;
	}

	// -1 indicates indefinte recording (until `ca 0` recv'd.)
	int duration_limit_seconds = options->fifo.empty() ? 5 : -1;
	auto start_time = std::chrono::steady_clock::now();

	while (video_active || preview_active || still_active || !options->fifo.empty())
	{
		// Check if there are any commands over the FIFO.
		std::string fifo_command = app.GetFifoCommand();
		if (!fifo_command.empty())
		{			
			LOG(1, "Got command from FIFO: " + fifo_command);

			// Split the fifo_command by space 
			std::vector<std::string> tokens = tokenizer(fifo_command, " ");
			auto it = app.commands.find(tokens[0]);
			LOG(1, "Got command from FIFO: " + tokens[0]);
			if (it != app.commands.end())
			{
				app.commands[tokens[0]](tokens, still_active, video_active, preview_active); //Call associated command handler
			}
			else
			{
				std::cout << "I don't know what is: " << tokens[0]<< std::endl;
			}
			// if (tokens[0] == "im") still_active = true; // Take a picture :)
			// else if (tokens[0] == "ca")
			// {
			// 	if (tokens.size() < 2 || tokens[1] != "1")
			// 	{ // ca 0, or some invalid command.
			// 		if (video_active)  // finish up with the current recording.
			// 			app.cleanup();
			// 		video_active = false;

			// 	}
			// 	else
			// 	{
			// 		video_active = true;
			// 		start_time = std::chrono::steady_clock::now();
			// 		if (tokens.size() >= 3) {
			// 			duration_limit_seconds = stoi(tokens[2]);
			// 		} else {
			// 			// FIXME: Magic number :)
			// 			duration_limit_seconds = -1; // Indefinite
			// 		}
			// 	}
			// }
			// else if (tokens[0] == "pv")
			// {
			// 	//OG: pv QQ WWW DD - set preview Quality, Width and Divider
			// 	//p05: pv Hight Width, may need to be consistent with OG
			// 	if (tokens.size() < 3)
			// 	{
			// 		std::cout << "Invalid command" << std::endl;
			// 	}
			// 	else
			// 	{
			// 		preview_active = true;
			// 		options->previewOptions.width = stoi(tokens[1]);
			// 		options->previewOptions.height = stoi(tokens[2]);
			// 	}
			// }
		}

		// If video is active and a duration is set, check the elapsed time
		if (video_active && duration_limit_seconds >= 0)
		{
			auto current_time = std::chrono::steady_clock::now();
			auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

			if (stopRecording)
			{
				LOG(1, "SIGINT caught. Stopping recording.");
				app.cleanup();
				video_active = false;  // Ensure video_active is set to false
				break; 
			}

			if (elapsed_time >= duration_limit_seconds)
			{
				std::cout << "time limit: " << duration_limit_seconds << " seconds is reached. stop." << std::endl;
				app.cleanup();
				video_active = false;
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

			if (still_active)
			{
				// Save still image instead of preview when still_active is set
				still_save(viewfinder_mem, viewfinder_info, completed_request->metadata, options->stillOptions.output,
						   app.CameraModel(), &options->stillOptions, libcamera::Size(3200, 2400));
				
				LOG(2, "Still image saved");
				still_active = false;
			}
			else if (preview_active || multi_active)
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

			if (video_active)
			{
				video_save(app, video_mem, video_info, completed_request->metadata, options->videoOptions.output,
						   app.CameraModel(), options->videoOptions, completed_request, video_stream);
				LOG(2, "Video recorded and saved");
			}
		}

		LOG(2, "Request processing completed");
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
		// Call cleanup after the event loop
		app.cleanup();
	}
	catch (std::exception const &e)
	{
		LOG_ERROR("ERROR: *** " << e.what() << " ***");
		return -1;
	}
	return 0;
}
