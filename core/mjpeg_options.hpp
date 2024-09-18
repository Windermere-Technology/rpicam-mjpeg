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
	std::string fifo;

	virtual void Print() const override
	{
		Options::Print();
		stillOptions.Print();
		previewOptions.Print();
		videoOptions.Print();
	}

	StillOptions stillOptions{};
	StillOptions previewOptions{};
	VideoOptions videoOptions{};

};
