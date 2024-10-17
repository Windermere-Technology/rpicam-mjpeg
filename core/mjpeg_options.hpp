/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * still_options.hpp - still capture program options
 */

#pragma once

#include <cstdio>
#include <algorithm>

#include "options.hpp"
#include "still_options.hpp"
#include "video_options.hpp"

struct MjpegOptions : public Options
{
	std:: string motion_output;

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
			("preview_path", value<std::string>(&previewOptions.output),
				"Set the preview output file name")
			("preview_width", value<unsigned int>(&previewOptions.width)->default_value(512)
				->notifier(in(128, 1024, "width")),
				"Set the output preview width (min = 128, max = 1024)")
			("video_path", value<std::string>(&videoOptions.output),
				"Set the video output file name")
			("video_width", value<unsigned int>(&videoOptions.width)->default_value(0),
				"Set the output video width (0 = use default value)")
			("video_height", value<unsigned int>(&videoOptions.height)->default_value(0),
				"Set the output video height (0 = use default value)")
			("image_path", value<std::string>(&stillOptions.output),
				"Set the still output file name")
			("image_width", value<unsigned int>(&stillOptions.width)->default_value(0),
				"Set the output still width (0 = use default value)")
			("image_height", value<unsigned int>(&stillOptions.height)->default_value(0),
				"Set the output still height (0 = use default value)")
			("control_file", value<std::string>(&fifo), "The path to the commands FIFO")
			("frame-divider", value<unsigned int>(&frameDivider)->default_value(1), // Add frameDivider option
            	"Set the frame divider for video recording (1 = no divider, higher values reduce frame rate)")
			// Break nopreview flag; the preview will not work in rpicam-mjpeg!
			("nopreview,n", value<bool>(&nopreview)->default_value(true)->implicit_value(true),
			"	**DO NOT USE** The preview window does not work for rpicam-mjpeg")
			("status_file", value<std::string>(&status_output)->default_value("/dev/shm/mjpeg/status_mjpeg.txt"),
				"Set the status output file name")
			("media_path", value<std::string>(&media_path)->implicit_value("/var/www/html/media"),
				"Set the media path for storing RPi_Cam_Web_Interface thumbnails")
			("thumb_gen", value<std::string>(&thumb_gen)->default_value("vit")->implicit_value("vit"),
				"Enable thumbnail generation for v(ideo), i(mages) and t(imelapse). (vit = video, image, timelapse enabled)")
			("motion_pipe", value<std::string>(&motion_output),
				"The path to the Scheduler FIFO motion detection will output to.")
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
			throw std::runtime_error("The --output option is deprecated. Use --video-output, --preview-output, or --still-output, --motion-output instead.");
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

	// TODO: Something better than this :)
	void AdjustRaspiMjpegOptionsToThingsThatActuallyWorkWithLibcamera()
	{
		// Contrast
		{
			float normalized_contrast;
			if (contrast < 0.0f) {
				// If contrast is less than 0, map it to the range [0, 1]
				normalized_contrast = (contrast + 100.0f) * (1.0f / 100.0f);
			} else if (contrast == 0.0f) {
				// If contrast is 0, set it to 1
				normalized_contrast = 1.0f;
			} else {
				// If contrast is greater than 0, map it to the range [1.0f, 15.99f]
				normalized_contrast = 1 + (contrast * 14.99f) / 100.0f;
			}
			contrast = std::clamp(normalized_contrast, 0.0f, 15.99f);
		}

		// Brightness
		{
			LOG(1, "Adjusting brightness, was " << brightness);
			// Clamp brightness to the valid range [0, 100]
			brightness = std::max(0.0f, std::min(brightness, 100.0f));

			// Convert brightness to the range [-1.0f, 1.0f]
			float normalized_brightness = (brightness / 50.0f) - 1.0f;
			brightness = normalized_brightness;
			LOG(1, "Adjusted brightness, is " << brightness);
		}

		ev = std::max(-10.0f, std::min(ev, 10.0f));

		// AWB Gain
		{
			awb_gain_r /= 100;
			awb_gain_b /= 100; 
			awbgains = std::to_string(awb_gain_r) + "," + std::to_string(awb_gain_b);
		}

		// Gain
		{
			gain = std::max(100.0f, std::min(gain, 2000.0f));
			//according to the Raspicam-app github issue #349 iso/100 = gain
			gain /= 100;
		}

		// Saturation
		{
			float normalized_saturation;
			if (saturation < 0.0f) {
				// If saturation is less than 0, map it to the range [0, 1]
				normalized_saturation = (saturation + 100.0f) * (1.0f / 100.0f);
			} else if (saturation == 0.0f) {
				// If saturation is 0, set it to 1
				normalized_saturation = 1.0f;
			} else {
				// If saturation is greater than 0, map it to the range [1.0f, 15.99f]
				normalized_saturation = 1 + (saturation * 14.99f) / 100.0f;
			}

			saturation = std::clamp(normalized_saturation, 0.0f, 15.99f);
		}

		// Still Quality
		{
			// Clamp quality to the valid range [0, 100]
			stillOptions.quality = std::max(0.0f, std::min((float)stillOptions.quality, 100.0f));
			float normalized_quality;
			if (stillOptions.quality <= 10.0f) {
				// Map quality from [0, 10] to [60, 85]
				normalized_quality = 60.0f + (stillOptions.quality * 2.5f);
			} else {
				// Map quality from [10, 100] to [85, 100]
				normalized_quality = 85.0f + ((stillOptions.quality - 10.0f) * (15.0f / 90.0f));
			}
			stillOptions.quality = std::clamp(normalized_quality, 60.0f, 100.0f);
		}

		// Video Bitrate
		{
			// Ensure bitrate is non-negative
			auto bitrate = videoOptions.bitrate.bps();
		
			// Clamp bitrate to the valid range [0, 25000000]
			bitrate = std::min(bitrate, 25000000ul);
			videoOptions.bitrate.set(std::to_string(bitrate) + "bps");
		}

		// Sharpness
		{

			float normalized_sharpness;
			if (sharpness < 0.0f) {
				// If sharpness is less than 0, map it to the range [0, 1]
				normalized_sharpness = (sharpness + 100.0f) * (1.0f / 100.0f);
			} else if (sharpness == 0.0f) {
				// If sharpness is 0, set it to 1
				normalized_sharpness = 1.0f;
			} else {
				// If sharpness is greater than 0, map it to the range [1.0f, 15.99f]
				normalized_sharpness = 1 + (sharpness * 14.99f) / 100.0f;
			}

			sharpness = std::clamp(normalized_sharpness, 0.0f, 15.99f);
		}
	}

	void AdjustValuesBeforeStandardAdjustments() override
	{
		AdjustRaspiMjpegOptionsToThingsThatActuallyWorkWithLibcamera();
	}
	unsigned int frameDivider;  // Declare frameDivider here

	std::string video_output;
	std::string fifo;
	std::string status_output;
	std::string media_path;
	std::string thumb_gen;

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
