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

#include "core/mjpeg_options.hpp"
#include "core/rpicam_app.hpp"
#include "image/image.hpp"
#include <libcamera/control_ids.h>

// video recording
#include "core/rpicam_encoder.hpp"
#include "encoder/encoder.hpp"
#include "output/file_output.hpp"

using namespace std::placeholders;
using libcamera::Stream;

class RPiCamMjpegApp : public RPiCamApp
{
public:
	RPiCamMjpegApp() : RPiCamApp(std::make_unique<MjpegOptions>()) {}

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
		h264Encoder->SetInputDoneCallback([](void *buffer) { LOG(1, "Input buffer done."); });

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

        if (h264FileOutput) {
            LOG(1, "Cleaning up file output...");
            h264FileOutput.reset();  // Free the file output resources
        }
    }

    // FIXME: This name is terrible!
    // TODO: It'd be nice to integrate this will app.Wait(), but that probably requires a decent refactor *~*
    std::string GetFifoCommand()
    {
        static std::string fifo_path = GetOptions()->fifo;
        static std::ifstream fifo { fifo_path };

        if (fifo_path == "") return "";

        std::string command;
        std::getline(fifo, command);
        // Reset EOF flag, so we can read in the future.
        if (fifo.eof()) fifo.clear();
        return command;
    }
};

static void preview_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info, libcamera::ControlList const &metadata,
                         std::string const &filename, std::string const &cam_model, StillOptions const *options, libcamera::Size outputSize, bool multiStream)
{
    std::string output_filename = filename;

    // Append "_preview.jpg" if multi-stream is enabled
    if (multiStream) {
        output_filename += "_preview.jpg";
    }

    jpeg_save(mem, info, metadata, output_filename, cam_model, options, outputSize.width, outputSize.height);
    LOG(1, "Saved preview image: " + output_filename);
}

static void still_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info,
					   libcamera::ControlList const &metadata, std::string const &filename,
					   std::string const &cam_model, StillOptions const *options, libcamera::Size outputSize)
{
	// Save a still every 3 seconds.
	static std::time_t last_run_at = 0;
	const std::time_t seconds_per_frame = 3;
	std::time_t now = std::time(nullptr);

	if (now - last_run_at < seconds_per_frame)
		return;
	last_run_at = now;

	// Add the datetime to the filename.
	std::string output_filename;
	{
		// The part before the extension (if one exists)
		size_t period_index = filename.rfind(".");
		if (period_index == std::string::npos)
			period_index = filename.length();
		std::string name = filename.substr(0, period_index);

		// The date/timestamp
		struct tm *tm = localtime(&now);
		size_t time_buff_size = 4 + 2 + 2 + 2 + 2 + 2 + 1; // strftime(NULL, 0, "%Y%m%d%H%M%S", tm);
		char *time_buff = (char *)calloc(time_buff_size, sizeof(*time_buff));
		assert(time_buff);
		// As per silvanmelchior/userland/.../RaspiMJPEG.c:714... surely there is a better way?
		strftime(time_buff, time_buff_size, "%Y%m%d%H%M%S", tm);
		std::string timestamp(time_buff);
		free(time_buff);

		// FIXME: This is the "better way" to print the timestamp, but when I create buffer we start getting "libav: cannot open input device" in *video_save*??
		// std::stringstream buffer;
		// buffer << std::put_time(std::localtime(&t), "%Y%m%d%H%M%S");

		// The extension
		std::string extension = filename.substr(period_index, filename.length());
		output_filename = name + timestamp + extension;
	}

	jpeg_save(mem, info, metadata, output_filename, cam_model, options, outputSize.width, outputSize.height);
	LOG(1, "Saved still capture: " + output_filename);
};

// video_save function using app to manage encoder and file output
static void video_save(RPiCamMjpegApp &app, const std::vector<libcamera::Span<uint8_t>> &mem, const StreamInfo &info,
					   const libcamera::ControlList &metadata, const std::string &filename,
					   const std::string &cam_model, VideoOptions &options, libcamera::Size outputSize,
					   const CompletedRequestPtr &completed_request, Stream *stream)
{
	// TODO: This probably shouldn't be setting these?
	options.codec = "h264"; // Use H.264 codec for encoding
	options.framerate = 15; // frames!!!!!

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
	LOG(1, "Encoding buffer of size " << mem[0].size() << " at timestamp " << timestamp_us);
	app.h264Encoder->EncodeBuffer(fd, mem[0].size(), mem[0].data(), info, timestamp_us);
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
        app.ConfigureMultiStream(0); // Flags can be passed as needed
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

    // If video recording is active, set up a 5-second limit
    int duration_limit_seconds = 5;
    auto start_time = std::chrono::steady_clock::now();

    for (;;)
    {
        // Check if there are any commands over the FIFO.
        std::string fifo_command = app.GetFifoCommand();
        if (fifo_command != "") {
            LOG(2, "Got command from FIFO: " + fifo_command);

			//split the fifo_commamd by space
			std::string delimiter = " ";
			size_t pos = 0;
			std::string token;
			std::vector<std::string> tokens; // Vector to store tokens

			while ((pos = fifo_command.find(delimiter)) != std::string::npos) {
				token = fifo_command.substr(0, pos);
				tokens.push_back(token); 
				fifo_command.erase(0, pos + delimiter.length());
			}

			// Add the last token
			tokens.push_back(fifo_command);
						
            if (tokens[0] == "im") {
                still_active = true; // Take a picture :)
			} else if (tokens[0] == "ca") {
				if (tokens.size() < 2 || tokens[1] != "1") { // ca 0, or some invalid command.
					still_active = false; //TODO: may need to break the current recording
				} else {
					//print get in true
					still_active = true;
					if (tokens.size() >= 3) duration_limit_seconds = stoi(tokens[2]);
				} 
			} else if (tokens[0] == "pv") { 
				//OG: pv QQ WWW DD - set preview Quality, Width and Divider
				//p05: pv Hight Width, may need to be consistent with OG
				if (tokens.size() < 3) {
					std::cout << "Invalid command" << std::endl;
				} else {
					std::cout << "Preview command: " << tokens[1] << " " << tokens[2] << std::endl; //delete
					preview_active = true;
					options->previewOptions.width = stoi(tokens[1]);
					options->previewOptions.height = stoi(tokens[2]);
				}
			}
        }

        // If video is active, check the elapsed time and limit to 5 seconds
        if (video_active) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

            if (elapsed_time >= duration_limit_seconds) {				
				std::cout << "time limit: " << duration_limit_seconds << " seconds is reached. stop." << std::endl;
                
                // Clean up encoder and file output
                app.cleanup();
                break;
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

            if (still_active) {
                // Save still image instead of preview when still_active is set
                still_save(viewfinder_mem, viewfinder_info, completed_request->metadata, options->stillOptions.output,
                           app.CameraModel(), &options->stillOptions, libcamera::Size(3200, 2400));
                LOG(2, "Still image saved");
            }
            else if (preview_active || multi_active) {
                // Save preview if not in still mode
				StillOptions const opts = options->previewOptions;
				// If opts.width == 0, we should use "the default"
				auto width = (opts.width >= 128 && opts.width <= 1024) ? opts.width : 512;
				auto height = opts.height ? opts.height : viewfinder_info.height;
                preview_save(viewfinder_mem, viewfinder_info, completed_request->metadata,
                             opts.output, app.CameraModel(), &opts,
							 libcamera::Size(width, height), multi_active);
                LOG(2, "Viewfinder (Preview) image saved");
            }
            else if (still_active) {
                still_save(viewfinder_mem, viewfinder_info, completed_request->metadata, options->output,
                            app.CameraModel(), &options->stillOptions, libcamera::Size(viewfinder_info.width, viewfinder_info.height));
                LOG(2, "Still image saved");
            }
        }

        // Process the VideoRecording stream
        if (app.VideoStream())
        {
            Stream *video_stream = app.VideoStream();
            StreamInfo video_info = app.GetStreamInfo(video_stream);
            BufferReadSync r(&app, completed_request->buffers[video_stream]);
            const std::vector<libcamera::Span<uint8_t>> video_mem = r.Get();

            if (video_active || multi_active) {
                video_save(app, video_mem, video_info, completed_request->metadata, options->videoOptions.output,
                           app.CameraModel(), options->videoOptions, libcamera::Size(video_info.width, video_info.height),
                           completed_request, video_stream);
                LOG(2, "Video recorded and saved");
            }
        }

        LOG(2, "Request processing completed");
    }
}

int main(int argc, char *argv[])
{
    try
    {
        RPiCamMjpegApp app;
        MjpegOptions *options = app.GetOptions();

        if (options->Parse(argc, argv))
        {
            if (options->verbose >= 2)
                options->Print();
			if (
				options->previewOptions.output.empty()
				&& options->stillOptions.output.empty()
				&& options->videoOptions.output.empty()
			)
				throw std::runtime_error("At least one of --preview-output, --still-output or --video-output should be provided.");

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
