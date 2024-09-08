#include <chrono>
#include <string>
#include <cassert>
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

// Declare global encoder and file output
// Encoder* h264Encoder = nullptr;
// std::unique_ptr<FileOutput> h264FileOutput;  // Use unique_ptr for managing the file output

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
			 // Pass VideoOptions and StreamInfo
			h264Encoder.reset(Encoder::Create(&videoOptions, info));  // Wrap the raw pointer into unique_ptr

			if (!h264Encoder) {
				LOG_ERROR("Failed to create encoder. h264Encoder is still nullptr.");
				return;
			} else {
				LOG(1, "Encoder successfully created.");
			}
		}

		if (!h264FileOutput) {
			LOG(1, "Initializing FileOutput...");
			h264FileOutput = std::make_unique<FileOutput>(&videoOptions);  // Pass the VideoOptions object

			// Log the FileOutput address to confirm initialization
			LOG(1, "FileOutput address: " << h264FileOutput.get());

			if (!h264FileOutput) {
				LOG_ERROR("FileOutput is not initialized, aborting...");
				return;
			}

			// // Bind the encoder's callback to the FileOutput's OutputReady method
			h264Encoder->SetOutputReadyCallback(std::bind(&FileOutput::OutputReady, h264FileOutput.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		}
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

};


static void preview_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info, libcamera::ControlList const &metadata,
			   std::string const &filename, std::string const &cam_model, MjpegOptions const *options, libcamera::Size outputSize)
{
    jpeg_save(mem, info, metadata, filename, cam_model, options, outputSize.width, outputSize.height);
};

static void still_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info, libcamera::ControlList const &metadata,
			   std::string const &filename, std::string const &cam_model, MjpegOptions const *options, libcamera::Size outputSize)
{
    assert(false && "TODO: Implement still_save");
};

// video_save function with global encoder and file output
static void video_save(RPiCamMjpegApp& app, const std::vector<libcamera::Span<uint8_t>>& mem, const StreamInfo& info, 
                       const libcamera::ControlList& metadata, const std::string& filename, 
                       const std::string& cam_model, const MjpegOptions* options, libcamera::Size outputSize, 
                       const CompletedRequestPtr& completed_request, Stream* stream) 
{
    VideoOptions videoOptions;
    videoOptions.codec = "h264";  // Use H.264 codec for encoding
    videoOptions.output = options->output;  // Output file path from MjpegOptions

    // Use the app instance to call initialize_encoder
    app.initialize_encoder(videoOptions, info);

    // Check if the encoder and file output were successfully initialized
    if (!app.h264Encoder) {
        LOG_ERROR("h264Encoder is not initialized. Aborting!");
        return;
    }

    if (!app.h264FileOutput) {
        LOG_ERROR("FileOutput is not initialized. Aborting!");
        return;
    }

    // Get buffer to process
    auto buffer = completed_request->buffers[stream];
    int fd = buffer->planes()[0].fd.get();  // File descriptor of buffer
    auto ts = metadata.get(libcamera::controls::SensorTimestamp);
    int64_t timestamp_us = ts ? *ts : buffer->metadata().timestamp / 1000;

    // Ensure buffer is valid before encoding
    if (mem.empty() || mem[0].size() == 0) {
        LOG_ERROR("Buffer is empty, cannot proceed.");
        return;
    }

    // Encode the buffer using the H.264 encoder
    LOG(1, "Encoding buffer of size " << mem[0].size() << " at timestamp " << timestamp_us);
    app.h264Encoder->EncodeBuffer(fd, mem[0].size(), mem[0].data(), info, timestamp_us);
	

    // Manually invoke OutputReady to write encoded data to file using FileOutput
    LOG(1, "Writing encoded data to file...");
    // Add debug checks for OutputReady
    try {
        app.h264FileOutput->OutputReady(mem[0].data(), mem[0].size(), timestamp_us, 0);
        LOG(1, "OutputReady successfully wrote data to file.");
    } catch (const std::exception &e) {
        LOG_ERROR("OutputReady failed: " << e.what());
    } catch (...) {
        LOG_ERROR("OutputReady encountered an unknown error.");
    }
}

// The main event loop for the application.
static void event_loop(RPiCamMjpegApp &app)
{
	MjpegOptions const *options = app.GetOptions();
	 // Properly declare and initialize MjpegOptions
    MjpegOptions* mjpegOptions = static_cast<MjpegOptions*>(app.GetOptions());
	// Create a new VideoOptions object
    VideoOptions videoOptions;

	 // Copy the fields that are common between MjpegOptions and VideoOptions
    videoOptions.output = mjpegOptions->output;  // Assuming output exists in both
    videoOptions.quality = mjpegOptions->quality;  // Copy MJPEG quality
    videoOptions.keypress = mjpegOptions->keypress;  // Copy keypress option
    videoOptions.signal = mjpegOptions->signal;  // Copy signal option

    // Set the codec (default to "mjpeg" if necessary)
    videoOptions.codec = "mjpeg";  // MJPEG is the codec being used

	app.OpenCamera();
	app.ConfigureViewfinder();
	app.StartCamera();
	bool preview_active = options->stream == "preview";
	bool still_active = options->stream == "still";
	bool video_active = options->stream == "video";

	for (;;)
	{
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

		// In viewfinder mode, simply run until the timeout. When that happens, switch to
		// capture mode.
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
     			still_save(mem, info, completed_request->metadata, options->output,
                           app.CameraModel(), options, libcamera::Size(100, 100));
            }

			// Call video_save and pass the app instance
			if (video_active) {
				try {
					video_save(app, mem, info, completed_request->metadata, options->output,
						app.CameraModel(), options, libcamera::Size(100, 100), completed_request, stream);
				} catch (const std::exception &e) {
					LOG_ERROR("Exception caught in event loop: " << e.what());
				}
			}

			LOG(1, "Viewfinder image received");

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
            // This is janky... but we probably won't keep this
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
