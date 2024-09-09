# rpicam-apps
This is a small suite of libcamera-based applications to drive the cameras on a Raspberry Pi platform.

>[!WARNING]
>These applications and libraries have been renamed from `libcamera-*` to `rpicam-*`. Symbolic links are installed to allow users to keep using the old application names, but these will be deprecated soon. Users are encouraged to adopt the new application and library names as soon as possible.

Running rpicam-mjpeg
--------------------

There is three subcommands available, each of which will output a different stream type. They are not configured to run concurrently, at this stage:

* `rpicam-mjpeg --stream preview --output /tmp/cam.jpg` will behave in a way similar to the RaspiMJPEG preview stream.
  - Terminate with Ctrl+C
  - `open /tmp/cam.jpg` should resemble a video stream if image viewer supports live-reloading (such as default RPi image viewer)
* `rpicam-mjpeg --stream still --output /tmp/cam.jpg` will save a timestamped JPEG every 3 seconds.
  - Terminate with Ctrl+C.
* `rpicam-mjpeg --stream video --output /tmp/vid.mp4` will save a 5s MP4 video.
  - Terminate by closing popup window, or waiting for 5s.
  - **NOTE:** Ending with Ctrl+C will result in a corrupt video.

Build
-----

* Build and install the raspberrypi/libcamera library; see documentation [here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-libcamera)
  - **NOTE:** Do not use the `libcamera` packages from the official repositories, these are outdated.
* Follow the official instructions to build rpicam-apps; see documentation [here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-rpicam-apps)

Once dependencies are installed, the build commands are:

```
# NOTE: `meson setup` only needs to be run once.
meson setup build -Denable_libav=enabled -Denable_drm=enabled -Denable_egl=enabled -Denable_qt=enabled -Denable_opencv=disabled -Denable_tflite=disabled
meson compile -C build
```

License
-------

The source code is made available under the simplified [BSD 2-Clause license](https://spdx.org/licenses/BSD-2-Clause.html).

Status
------

[![ToT libcamera build/run test](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml/badge.svg)](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml)
