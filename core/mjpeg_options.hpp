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

struct MjpegOptions : public StillOptions
{
	MjpegOptions() : StillOptions()
	{
		using namespace boost::program_options;
		// clang-format off
		options_.add_options()
			("stream", value<std::string>(&stream), "Select the output stream type (preview, still or video)")
			("fifo", value<std::string>(&fifo), "The path to the commands FIFO")
			;
		// clang-format on
	}

	std::string stream;
	std::string fifo;

	virtual void Print() const override
	{
		Options::Print();
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

private:
	std::string timelapse_;
};
