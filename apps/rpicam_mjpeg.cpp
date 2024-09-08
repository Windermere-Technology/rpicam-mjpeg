/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
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
};

static void preview_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info, libcamera::ControlList const &metadata,
			   std::string const &filename, std::string const &cam_model, MjpegOptions const *options, libcamera::Size outputSize)
{
    // TODO: Review if RaspiMJPEG allows arbitrary resolutions (which this supports).
    // If it doesn't, and we get multi-streams back from the camera working we can do the resize on the camera hardware directly.
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
        std::stringstream buffer;

        // The part before the extension (if one exists)
        size_t period_index = filename.rfind(".");
        if (period_index == std::string::npos) period_index = filename.length();
        buffer << filename.substr(0, period_index);

        // The date/timestamp
        std::time_t t = std::time(nullptr);
        // As per silvanmelchior/userland/.../RaspiMJPEG.c:714... Is this really the best C++ could do?
        buffer << std::put_time(std::localtime(&t), "%Y%m%d%H%M%S");

        // The extension
        buffer << filename.substr(period_index, filename.length());
        output_filename = buffer.str();
    }

    jpeg_save(mem, info, metadata, output_filename, cam_model, options, outputSize.width, outputSize.height);
    LOG(1, "Saved still capture: " + output_filename);
};

static void video_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info, libcamera::ControlList const &metadata,
			   std::string const &filename, std::string const &cam_model, MjpegOptions const *options, libcamera::Size outputSize)
{
    assert(false && "TODO: Implement video_save");
};

// The main even loop for the application.
static void event_loop(RPiCamMjpegApp &app)
{
	MjpegOptions const *options = app.GetOptions();
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
			LOG(2, "Viewfinder image received");

            if (preview_active) {
                preview_save(mem, info, completed_request->metadata, options->output,
                           app.CameraModel(), options, libcamera::Size(100, 100));
            }

            if (still_active) {
                still_save(mem, info, completed_request->metadata, options->output,
                           app.CameraModel(), options, libcamera::Size(info.width, info.height));
            }

            if (video_active) {
                video_save(mem, info, completed_request->metadata, options->output,
                           app.CameraModel(), options, libcamera::Size(info.width, info.height));
            }
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
