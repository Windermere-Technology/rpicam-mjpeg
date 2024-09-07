/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * rpicam_jpeg.cpp - minimal libcamera jpeg capture app.
 */

#include <chrono>
#include <string>

#include "core/rpicam_app.hpp"
#include "core/still_options.hpp"

#include "image/image.hpp"

#include <libcamera/control_ids.h>

using namespace std::placeholders;
using libcamera::Stream;

class RPiCamMjpegApp : public RPiCamApp
{
public:
	RPiCamMjpegApp()
		: RPiCamApp(std::make_unique<StillOptions>())
	{
	}

	StillOptions *GetOptions() const
	{
		return static_cast<StillOptions *>(options_.get());
	}
};

static void mjpeg_save(std::vector<libcamera::Span<uint8_t>> const &mem, StreamInfo const &info, libcamera::ControlList const &metadata,
			   std::string const &filename, std::string const &cam_model, StillOptions const *options, libcamera::Size outputSize)
{
    // TODO: Review if RaspiMJPEG allows arbitrary resolutions (which this supports).
    // If it doesn't, and we get multi-streams back from the camera working we can do the resize on the camera hardware directly.
    jpeg_save(mem, info, metadata, filename, cam_model, options, outputSize.width, outputSize.height);
};

// The main even loop for the application.

static void event_loop(RPiCamMjpegApp &app)
{
	StillOptions const *options = app.GetOptions();
	app.OpenCamera();
	app.ConfigureViewfinder();
	app.StartCamera();

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
			mjpeg_save(mem, info, completed_request->metadata, options->output, app.CameraModel(), options, libcamera::Size(100, 100));

			LOG(1, "Viewfinder image received");
		}
	}
}

int main(int argc, char *argv[])
{
	try
	{
		RPiCamMjpegApp app;
		StillOptions *options = app.GetOptions();
		if (options->Parse(argc, argv))
		{
			if (options->verbose >= 2)
				options->Print();
			if (options->output.empty())
				throw std::runtime_error("output file name required");

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
