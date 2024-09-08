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
Encoder* h264Encoder = nullptr;
std::unique_ptr<FileOutput> h264FileOutput;  // Use unique_ptr for managing the file output

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
};

// helper fucntions 
// Function to initialize the encoder and file output

void initialize_encoder(VideoOptions& videoOptions, const StreamInfo& info) {
    if (!h264Encoder) {
        LOG(1, "Initializing encoder...");
        h264Encoder = Encoder::Create(&videoOptions, info);  // Pass VideoOptions and StreamInfo
        if (!h264Encoder) {
            LOG_ERROR("Failed to create encoder.");
            return;
        }
    }

    if (!h264FileOutput) {
        LOG(1, "Initializing FileOutput...");
        h264FileOutput = std::make_unique<FileOutput>(&videoOptions);  // Pass the VideoOptions object
    }

}



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
static void video_save(const std::vector<libcamera::Span<uint8_t>>& mem, const StreamInfo& info, 
                       const libcamera::ControlList& metadata, const std::string& filename, 
                       const std::string& cam_model, const MjpegOptions* options, libcamera::Size outputSize, 
                       const CompletedRequestPtr& completed_request, Stream* stream) 
{
    VideoOptions videoOptions;
    videoOptions.codec = "h264";  // Use H.264 codec for encoding
    videoOptions.output = options->output;  // Output file path from MjpegOptions

    // Initialize encoder and file output if not already initialized
    initialize_encoder(videoOptions, info);

    // Get buffer to process
    auto buffer = completed_request->buffers[stream];
    int fd = buffer->planes()[0].fd.get();  // File descriptor of buffer
    auto ts = metadata.get(libcamera::controls::SensorTimestamp);
    int64_t timestamp_us = ts ? *ts : buffer->metadata().timestamp / 1000;  // Convert ns to us if needed

    // Encode the buffer using the H.264 encoder
    LOG(1, "Encoding buffer of size " << mem[0].size() << " at timestamp " << timestamp_us);
    h264Encoder->EncodeBuffer(fd, mem[0].size(), mem[0].data(), info, timestamp_us);

    //  Manually invoke OutputReady - Write encoded data to file using FileOutput
    h264FileOutput->OutputReady(mem[0].data(), mem[0].size(), timestamp_us, 0);
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

            if (video_active) {
     			video_save(mem, info, completed_request->metadata, options->output,
                           app.CameraModel(), options, libcamera::Size(100, 100),completed_request, stream);
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
	}
	catch (std::exception const &e)
	{
		LOG_ERROR("ERROR: *** " << e.what() << " ***");
		return -1;
	}
	return 0;
}
