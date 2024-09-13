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

		if (h264FileOutput)
		{
			LOG(1, "Cleaning up file output...");
			h264FileOutput.reset(); // Free the file output resources
		}
	}
};

static void preview_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info, libcamera::ControlList const &metadata,
                         std::string const &filename, std::string const &cam_model, MjpegOptions const *options, libcamera::Size outputSize, bool multiStream)
{
    std::string output_filename = filename;

    // Append "_preview.jpg" if multi-stream is enabled, otherwise just append ".jpg"
    if (multiStream) {
        output_filename += "_preview.jpg";
    } else {
        output_filename += ".jpg";
    }

    jpeg_save(mem, info, metadata, output_filename, cam_model, options, outputSize.width, outputSize.height);
    LOG(1, "Saved preview image: " + output_filename);
}

static void still_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info,
					   libcamera::ControlList const &metadata, std::string const &filename,
					   std::string const &cam_model, MjpegOptions const *options, libcamera::Size outputSize)
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
					   const std::string &cam_model, const MjpegOptions *options, libcamera::Size outputSize,
					   const CompletedRequestPtr &completed_request, Stream *stream)
{
	VideoOptions videoOptions;
	videoOptions.codec = "h264"; // Use H.264 codec for encoding
	videoOptions.framerate = 15; // frames!!!!!
	videoOptions.output = options->output; // Output file path from MjpegOptions

	// Use the app instance to call initialize_encoder
	app.initialize_encoder(videoOptions, info);

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

// motion vector extraction function from standalone motion vector app
void extractMotionVectors(const std::string& videoFile, const std::string& outputFile) {
    // Construct the FFmpeg command to extract motion vectors
    std::string ffmpegCommand = "ffmpeg -flags2 +export_mvs -i " + videoFile + 
                                " -vf codecview=mv=pf+bf+bb " + outputFile + " -y";

    // Run the FFmpeg command
    std::cout << "Executing FFmpeg command to extract motion vectors..." << std::endl;
    system(ffmpegCommand.c_str());
    std::cout << "Motion vectors extracted to " << outputFile << std::endl;
}


// motion_vector function
static void motion_vector(RPiCamMjpegApp &app, const std::vector<libcamera::Span<uint8_t>> &mem, const StreamInfo &info,
					   const libcamera::ControlList &metadata, const std::string &filename,
					   const std::string &cam_model, const MjpegOptions *options, libcamera::Size outputSize,
					   const CompletedRequestPtr &completed_request, Stream *stream)
{
	VideoOptions videoOptions;
	videoOptions.codec = "h264"; // Use H.264 codec for encoding
	videoOptions.framerate = 15; // frames!!!!!
	videoOptions.output = options->output; // Output file path from MjpegOptions

	// Use the app instance to call initialize_encoder
	app.initialize_encoder(videoOptions, info);

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


	std::string outputFile = std::string(options->output);  // Assuming it's a C-style string

	// Extract motion vectors using FFmpeg
	extractMotionVectors(outputFile, "/tmp/motion_vectors_output.mp4");
}

// The main event loop for the application.
static void event_loop(RPiCamMjpegApp &app)
{
    MjpegOptions const *options = app.GetOptions();
    MjpegOptions* mjpegOptions = static_cast<MjpegOptions*>(app.GetOptions());

    VideoOptions videoOptions;
    videoOptions.output = mjpegOptions->output; // Assuming output exists in both
    videoOptions.quality = mjpegOptions->quality; // Copy MJPEG quality
    videoOptions.keypress = mjpegOptions->keypress; // Copy keypress option
    videoOptions.signal = mjpegOptions->signal; // Copy signal option

    // Set the codec (default to "mjpeg" if necessary)
    videoOptions.codec = "mjpeg";  // MJPEG is the codec being used

    app.OpenCamera();

    bool preview_active = options->stream == "preview";
    bool still_active = options->stream == "still";
    bool video_active = options->stream == "video";
    bool multi_active = options->stream == "multi";

	bool motion_active = options->stream == "motion";

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
	
	else if (motion_active)
    {
        app.ConfigureVideo();
        app.StartCamera();
    }

    else if (preview_active || still_active)
    {
        app.ConfigureViewfinder();
        app.StartCamera();
    }

    // If video recording is active or multi-stream, set up a 5-second limit
    const int duration_limit_seconds = 5;
    auto start_time = std::chrono::steady_clock::now();

    for (;;)
    {
        if (video_active || multi_active) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

            if (elapsed_time >= duration_limit_seconds) {
                LOG(1, "5-second video recording limit reached. Stopping.");
                app.cleanup();
                break;
            }
        }

		if (motion_active || multi_active) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

            if (elapsed_time >= duration_limit_seconds) {
                LOG(1, "5-second video recording limit reached. Stopping.");
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

            if (preview_active || multi_active) 
            {
                // Save the preview image
                preview_save(viewfinder_mem, viewfinder_info, completed_request->metadata, options->output,
                            app.CameraModel(), options, libcamera::Size(100, 100), multi_active);  // Adjust size as needed
                LOG(2, "Viewfinder (Preview) image saved");
            }
            else if (still_active) {
                still_save(viewfinder_mem, viewfinder_info, completed_request->metadata, options->output,
                            app.CameraModel(), options, libcamera::Size(viewfinder_info.width, viewfinder_info.height));
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
                video_save(app, video_mem, video_info, completed_request->metadata, options->output,
                           app.CameraModel(), options, libcamera::Size(video_info.width, video_info.height),
                           completed_request, video_stream);
                LOG(2, "Video recorded and saved");
            }

			if (motion_active) {
                LOG(1, "working...");
				motion_vector(app, video_mem, video_info, completed_request->metadata, options->output,
                           app.CameraModel(), options, libcamera::Size(video_info.width, video_info.height),
                           completed_request, video_stream);
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
            if (options->output.empty())
                throw std::runtime_error("output file name required");
            if (options->stream.empty())
                throw std::runtime_error("stream type required");
            if (options->stream != "preview" && options->stream != "still" && options->stream != "video" && options->stream != "multi" && options->stream != "motion") {
                throw std::runtime_error("stream type must be one of: preview, still, video, motion");
            }
            if (options->stream == "multi"){
                std::cout << "==== Starting multistream ====" << std::endl;
            }

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