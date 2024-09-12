/* SPDX-License-Identifier: BSD-2-Clause */
/*
* Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
* Copyright (C) 2024, Dylan Lom
* TODO: If people want to stick their names here they can!
*
* rpicam_jpeg.cpp - minimal libcamera jpeg capture app.
*/
#include <chrono>
#include <string>
#include <cassert>
#include <ctime>
#include <iomanip>

#include "core/rpicam_app.hpp"
#include "core/mjpeg_options.hpp"
#include "image/image.hpp"
#include <libcamera/control_ids.h>

// video recording 
#include "core/rpicam_encoder.hpp"
#include "output/file_output.hpp"
#include "encoder/encoder.hpp"  

using namespace std::placeholders;
using libcamera::Stream;

class RPiCamMjpegApp : public RPiCamApp
{
public:
    RPiCamMjpegApp()
        : RPiCamApp(std::make_unique<MjpegOptions>())
    {
    }

    MjpegOptions *GetOptions() const
    {
        return static_cast<MjpegOptions *>(options_.get());
    }

    // Declare Encoder and FileOutput as member variables
    std::unique_ptr<Encoder> h264Encoder;
    std::unique_ptr<FileOutput> h264FileOutput;

    // Function to initialize the encoder and file output
    void initialize_encoder(VideoOptions& videoOptions, const StreamInfo& info) {
        if (!h264Encoder) {
            LOG(1, "Initializing encoder...");
            h264Encoder = std::unique_ptr<Encoder>(Encoder::Create(&videoOptions, info));  // Properly wrap the raw pointer into unique_ptr
            if (!h264Encoder) {
                LOG_ERROR("Failed to create encoder.");
                return;
            }
        }

        if (!h264FileOutput) {
            LOG(1, "Initializing FileOutput...");
            h264FileOutput = std::make_unique<FileOutput>(&videoOptions);  // Pass the VideoOptions object
        }

        // Set encoder callbacks (if not already set)
        h264Encoder->SetInputDoneCallback([](void *buffer) {
            LOG(1, "Input buffer done.");
        });

        h264Encoder->SetOutputReadyCallback([this](void *data, size_t size, int64_t timestamp, bool keyframe) {
            LOG(1, "Output ready: size = " << size << ", timestamp = " << timestamp);
            h264FileOutput->OutputReady(data, size, timestamp, keyframe);
        });
    }

    void cleanup() {
        if (h264Encoder) {
            LOG(1, "Cleaning up encoder...");
            h264Encoder.reset();  // This will call the destructor of the encoder and release its resources
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
                         std::string const &filename, std::string const &cam_model, MjpegOptions const *options, libcamera::Size outputSize)
{
    jpeg_save(mem, info, metadata, filename, cam_model, options, outputSize.width, outputSize.height);
};

static void still_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info, libcamera::ControlList const &metadata,
                       std::string const &filename, std::string const &cam_model, MjpegOptions const *options, libcamera::Size outputSize)
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
         if (period_index == std::string::npos) period_index = filename.length();
         std::string name = filename.substr(0, period_index);

         // The date/timestamp
         struct tm *tm = localtime(&now);
         size_t time_buff_size = 4 + 2 + 2 + 2 + 2 + 2 + 1; // strftime(NULL, 0, "%Y%m%d%H%M%S", tm);
         char *time_buff = (char*)calloc(time_buff_size, sizeof(*time_buff));
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
static void video_save(RPiCamMjpegApp& app, const std::vector<libcamera::Span<uint8_t>>& mem, const StreamInfo& info, 
                       const libcamera::ControlList& metadata, const std::string& filename, 
                       const std::string& cam_model, const MjpegOptions* options, libcamera::Size outputSize, 
                       const CompletedRequestPtr& completed_request, Stream* stream) 
{
    VideoOptions videoOptions;
    videoOptions.codec = "h264";  // Use H.264 codec for encoding
    videoOptions.framerate = 15;  // frames!!!!!
    videoOptions.output = options->output;  // Output file path from MjpegOptions

    // Use the app instance to call initialize_encoder
    app.initialize_encoder(videoOptions, info);

    // Check if the encoder and file output were successfully initialized
    if (!app.h264Encoder) {
        LOG_ERROR("Failed to initialize encoder.");
        return;
    }

    if (!app.h264FileOutput) {
        LOG_ERROR("Failed to initialize file output.");
        return;
    }

    // Get buffer to process
    auto buffer = completed_request->buffers[stream];
    int fd = buffer->planes()[0].fd.get();  // File descriptor of buffer
    auto ts = metadata.get(libcamera::controls::SensorTimestamp);
    int64_t timestamp_us = ts ? (*ts / 1000) : (buffer->metadata().timestamp / 1000);

    // Ensure buffer is valid before encoding
    if (mem.empty() || mem[0].size() == 0) {
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
    MjpegOptions const *options = app.GetOptions();

    VideoOptions videoOptions;
    videoOptions.output = options->output; // Assuming output exists in both
    videoOptions.quality = options->quality; // Copy MJPEG quality
    videoOptions.keypress = options->keypress; // Copy keypress option
    videoOptions.signal = options->signal; // Copy signal option
    
	// Set the codec (default to "mjpeg" if necessary)
	videoOptions.codec = "mjpeg";  // MJPEG is the codec being used

    app.OpenCamera();
    app.ConfigureViewfinder();
    app.StartCamera();

    bool preview_active = options->stream == "preview";
    bool still_active = options->stream == "still";
    bool video_active = options->stream == "video";

    // If video recording is active, set up a 5-second limit
    const int duration_limit_seconds = 5;
    auto start_time = std::chrono::steady_clock::now();

    for (;;)
    {
        // Check if there are any commands over the FIFO.
        std::string fifo_command = app.GetFifoCommand();
        if (fifo_command != "") {
            LOG(2, "Got command from FIFO: " + fifo_command);
            if (fifo_command == "im")
                still_active = true; // Take a picture :)
        }

        // If video is active, check the elapsed time and limit to 5 seconds
        if (video_active) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

            if (elapsed_time >= duration_limit_seconds) {
                LOG(1, "5-second video recording limit reached. Stopping.");
                
                // Clean up encoder and file output
                app.cleanup();
                break;  // Break out of the loop
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

        if (app.ViewfinderStream())
        {
            Stream *stream = app.ViewfinderStream();
            StreamInfo info = app.GetStreamInfo(stream);
            CompletedRequestPtr &completed_request = std::get<CompletedRequestPtr>(msg.payload);
            BufferReadSync r(&app, completed_request->buffers[stream]);
            const std::vector<libcamera::Span<uint8_t>> mem = r.Get();

            if (preview_active) {
                preview_save(mem, info, completed_request->metadata, options->output,
                             app.CameraModel(), options, libcamera::Size(100, 100));
            }

            if (still_active) {
                // TODO: This will ignore subsequent "im" commands in a 3s period.
                // - We need some kind of mechanism to reset the timer inside of still_save.
                still_save(mem, info, completed_request->metadata, options->output,
                           app.CameraModel(), options, libcamera::Size(info.width, info.height));
                // If still_active was set due to an "im" command, it should not keep running.
                still_active = options->stream == "still";
            }

            if (video_active) {
                video_save(app, mem, info, completed_request->metadata, options->output,
                           app.CameraModel(), options, libcamera::Size(100, 100), completed_request, stream);
            }

            LOG(2, "Viewfinder image received");
        }
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
            if (options->stream != "preview" && options->stream != "still" && options->stream != "video") {
                throw std::runtime_error("stream type must be one of: preview, still, video");
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
