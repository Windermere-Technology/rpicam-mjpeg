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

		// Yoink'd: https://stackoverflow.com/a/28667150
		auto in = [](int min, int max, std::string opt_name) {
			return [opt_name, min, max](int value) {
				if (value < min || value > max) {
					throw validation_error(
						validation_error::invalid_option_value,
						opt_name,
						std::to_string(value)
					);
				}
			};
		};

		// clang-format off
		options_.add_options()
			("preview-output", value<std::string>(&previewOptions.output),
				"Set the preview output file name")
			("preview-width", value<unsigned int>(&previewOptions.width)->default_value(0)
				->notifier(in(128, 1024, "preview-width")),
				"Set the output preview width (0 = use default value, min = 128, max = 1024)")
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
			// Break nopreview flag; the preview will not work in rpicam-mjpeg!
			("nopreview,n", value<bool>(&nopreview)->default_value(true)->implicit_value(true),
			"	**DO NOT USE** The preview window does not work for rpicam-mjpeg")
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

		// Disable the preview window; it won't work.
		nopreview = true;

		// Check if --output is used and throw an error if it's provided
		if (!output.empty())
		{
			throw std::runtime_error("The --output option is deprecated. Use --video-output, --preview-output, or --still-output instead.");
		}

		// Ensure at least one of --still-output, --video-output, or --preview-output is specified
		if (stillOptions.output.empty() && previewOptions.output.empty() && videoOptions.output.empty())
		{
			throw std::runtime_error("At least one of --still-output, --video-output, or --preview-output should be provided.");
		}
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
