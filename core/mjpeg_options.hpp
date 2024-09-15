/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * still_options.hpp - still capture program options
 */

#pragma once

#include <cstdio>

#include "options.hpp"
#include "still_options.hpp"
#include "video_options.hpp"

struct MjpegOptions : public Options
{
	MjpegOptions() : Options()
	{
		using namespace boost::program_options;
		// clang-format off
		options_.add_options()
			("preview-output", value<std::string>(&previewOptions.output),
				"Set the preview output file name")
			("preview-width", value<unsigned int>(&previewOptions.width)->default_value(0),
				"Set the output preview width (0 = use default value)")
			("preview-height", value<unsigned int>(&previewOptions.height)->default_value(0),
				"Set the output preview height (0 = use default value)")
			("video-output", value<std::string>(&videoOptions.output),
				"Set the video output file name")
			("video-width", value<unsigned int>(&videoOptions.width)->default_value(0),
				"Set the output video width (0 = use default value)")
			("video-height", value<unsigned int>(&videoOptions.height)->default_value(0),
				"Set the output video height (0 = use default value)")
			("still-output", value<std::string>(&stillOptions.output),
				"Set the still output file name")
			("still-width", value<unsigned int>(&stillOptions.width)->default_value(0),
				"Set the output still width (0 = use default value)")
			("still-height", value<unsigned int>(&stillOptions.height)->default_value(0),
				"Set the output still height (0 = use default value)")
			("stream", value<std::string>(&stream), "Select the output stream type (preview, still or video)")
			("fifo", value<std::string>(&fifo), "The path to the commands FIFO")
			;
		// clang-format on
	}
	virtual bool Parse(int argc, char *argv[]) override
	{
		// TODO: Modifications won't propogate down at this time.
		if (stillOptions.Parse(argc, argv) == false)
			return false;

		if (previewOptions.Parse(argc, argv) == false)
			return false;

		if (videoOptions.Parse(argc, argv) == false)
			return false;

		// NOTE: This will override the *Options.output members :)
		if (Options::Parse(argc, argv) == false)
			return false;

		output = "/dev/null";

		return true;
	}

	virtual void SetApp(RPiCamApp *app) override
	{
		// I hope I'm not breaking any assumptions by doing this...
		stillOptions.SetApp(app);
		previewOptions.SetApp(app);
		videoOptions.SetApp(app);
		Options::SetApp(app);
	}
	std::string stream;
	std::string fifo;

	virtual void Print() const override
	{
		Options::Print();
		stillOptions.Print();
		previewOptions.Print();
		videoOptions.Print();
		std::cerr << "    encoding: " << encoding << std::endl;
		std::cerr << "    quality: " << quality << std::endl;
		std::cerr << "    raw: " << raw << std::endl;
		std::cerr << "    restart: " << restart << std::endl;
		std::cerr << "    timelapse: " << timelapse.get() << "ms" << std::endl;
		std::cerr << "    framestart: " << framestart << std::endl;
		std::cerr << "    datetime: " << datetime << std::endl;
		std::cerr << "    timestamp: " << timestamp << std::endl;
		std::cerr << "    keypress: " << keypress << std::endl;
		std::cerr << "    signal: " << signal << std::endl;
		std::cerr << "    thumbnail width: " << thumb_width << std::endl;
		std::cerr << "    thumbnail height: " << thumb_height << std::endl;
		std::cerr << "    thumbnail quality: " << thumb_quality << std::endl;
		std::cerr << "    latest: " << latest << std::endl;
		std::cerr << "    immediate " << immediate << std::endl;
		std::cerr << "    AF on capture: " << af_on_capture << std::endl;
		std::cerr << "    Zero shutter lag: " << zsl << std::endl;
		std::cerr << "    Stream: " << stream << std::endl;
		std::cerr << "    FIFO: " << fifo << std::endl;
		for (auto &s : exif)
			std::cerr << "    EXIF: " << s << std::endl;
	}

	StillOptions stillOptions{};
	StillOptions previewOptions{};
	VideoOptions videoOptions{};

};
