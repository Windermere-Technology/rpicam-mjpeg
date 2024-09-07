# rpicam-apps
This is a small suite of libcamera-based applications to drive the cameras on a Raspberry Pi platform.

>[!WARNING]
>These applications and libraries have been renamed from `libcamera-*` to `rpicam-*`. Symbolic links are installed to allow users to keep using the old application names, but these will be deprecated soon. Users are encouraged to adopt the new application and library names as soon as possible.

Running rpicam-mjpeg
--------------------

There is three subcommands available, each of which will output a different stream type
* `rpicam-mjpeg --stream preview --output /tmp/cam.jpg` will behave in a way similar to the RaspiMJPEG preview stream
* `rpicam-mjpeg --stream still --output /tmp/cam.jpg` will save a single JPEG
* `rpicam-mjpeg --stream video --output /tmp/vid.mp4` will save a H264 MP4 video

Build
-----

* Build and install the raspberrypi/libcamera library; see documentation [here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-libcamera)
* Follow the official instructions to build rpicam-apps; see documentation [here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-rpicam-apps)

License
-------

The source code is made available under the simplified [BSD 2-Clause license](https://spdx.org/licenses/BSD-2-Clause.html).

Status
------

[![ToT libcamera build/run test](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml/badge.svg)](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml)
