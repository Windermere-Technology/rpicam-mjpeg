/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * rpicam_jpeg.cpp - minimal libcamera jpeg capture app.
 */

#include <chrono>
#include <string>
#include <cassert>

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
    assert(false && "TODO: Implement still_save");
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
                           app.CameraModel(), options, libcamera::Size(100, 100));
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
