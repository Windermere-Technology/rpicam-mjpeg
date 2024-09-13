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
			("video-output", value<std::string>(&videoOptions.output),
				"Set the video output file name")
			("still-output", value<std::string>(&stillOptions.output),
				"Set the still output file name")
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
