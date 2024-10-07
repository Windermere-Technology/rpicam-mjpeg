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
			("preview-width", value<unsigned int>(&previewOptions.width)->default_value(512)
				->notifier(in(128, 1024, "preview-width")),
				"Set the output preview width (min = 128, max = 1024)")
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
			("status-output", value<std::string>(&status_output)->default_value("/dev/shm/mjpeg/status_mjpeg.txt"),
				"Set the status output file name")
			;
		// clang-format on
	}

	virtual bool Parse(int argc, char *argv[]) override
	{
		using namespace libcamera;

		// We need to intersect all the unrecognized options, since it is only those that
		// *nothing* recognized that are actually unrecognized by our program.
		std::vector<std::string> unrecognized_tmp, unrecognized;
		auto unrecognized_intersect = [&unrecognized_tmp, &unrecognized]()
		{
			std::vector<std::string> collector;
			std::sort(unrecognized_tmp.begin(), unrecognized_tmp.end());
			if (unrecognized.size() == 0)
			{
				unrecognized = unrecognized_tmp;
				return;
			}

			std::set_intersection(unrecognized_tmp.cbegin(), unrecognized_tmp.cend(), unrecognized.cbegin(),
								  unrecognized.cend(), std::back_inserter(collector));

			unrecognized = collector;
		};

		if (stillOptions.Parse(argc, argv, &unrecognized_tmp) == false)
			return false;
		unrecognized_intersect();

		if (previewOptions.Parse(argc, argv, &unrecognized_tmp) == false)
			return false;
		unrecognized_intersect();

		if (videoOptions.Parse(argc, argv, &unrecognized_tmp) == false)
			return false;
		unrecognized_intersect();

		// NOTE: This will override the *Options.output members :)
		if (Options::Parse(argc, argv, &unrecognized_tmp) == false)
			return false;
		unrecognized_intersect();

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

		// Error if any unrecognised flags were provided
		if (unrecognized.size())
		{
			throw boost::program_options::unknown_option(unrecognized[0]);
		}

		// Save the actual rotation/flip applied by the settings, as we need this later.
		bool ok;
		SetRotation(transformFromRotation(rotation(), &ok));
		assert(ok && "This should have failed already if it was going to.");
		Transform flip = Transform::Identity;
		if (vflip())
			flip = flip * Transform::VFlip;
		if (hflip())
			flip = flip * Transform::HFlip;
		SetFlip(flip);

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
	std::string status_output;

	virtual void Print() const override
	{
		Options::Print();
		stillOptions.Print();
		previewOptions.Print();
		videoOptions.Print();
		std::cerr << "    fifo: " << fifo << std::endl;
		std::cerr << "    status-output: " << status_output << std::endl;
	}

	StillOptions stillOptions {};
	StillOptions previewOptions {};
	VideoOptions videoOptions {};

	/* We need to track the current rotation/flip independantly, but the
	 * design of libcamera::Transform doesn't allow us to distinguish between
	 * rot180 and (hflip * vflip), for example. So we use these wrappers :)
	 * https://libcamera.org/api-html/namespacelibcamera.html#a371b6d17d531b85c035c4e889b116571
	*/
	libcamera::Transform rot() const { return rot_; }

	void SetRotation(libcamera::Transform value)
	{
		rot_ = value;
		updateTransform();
	};

	void SetFlip(libcamera::Transform value)
	{
		flip_ = value;
		updateTransform();
	}

	void SetAwb(std::string new_awb)
	{
		// NOTE: This will throw if we got an unhandled value.
		auto new_awb_index = Options::AwbLookup(new_awb);
		awb = new_awb;
		awb_index = new_awb_index;
		stillOptions.awb = new_awb;
		stillOptions.awb_index = new_awb_index;
		previewOptions.awb = new_awb;
		previewOptions.awb_index = new_awb_index;
		videoOptions.awb = new_awb;
		videoOptions.awb_index = new_awb_index;
	}
private:
	libcamera::Transform rot_;
	libcamera::Transform flip_;

	void updateTransform()
	{
		using namespace libcamera;
		// Recacluate the transform.
		transform = Transform::Identity * flip_ * rot_;
		// Apply the new transform to all our sub-9options.
		stillOptions.transform = transform;
		videoOptions.transform = transform;
		previewOptions.transform = transform;
	}
};
