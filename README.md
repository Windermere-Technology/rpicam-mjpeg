# rpicam-apps
This is a small suite of libcamera-based applications to drive the cameras on a Raspberry Pi platform.

>[!WARNING]
>These applications and libraries have been renamed from `libcamera-*` to `rpicam-*`. Symbolic links are installed to allow users to keep using the old application names, but these will be deprecated soon. Users are encouraged to adopt the new application and library names as soon as possible.

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

Running rpicam-mjpeg
--------------------

There are three subcommands available, each of which will output a different stream type.
At this stage, the subcommands are not configured to run concurrently:

### 1. Preview Stream

```bash
rpicam-mjpeg --stream preview --output /tmp/cam.jpg
```
* `rpicam-mjpeg --stream preview --output /tmp/cam.jpg` will behave in a way similar to the RaspiMJPEG preview stream.
  - Terminate with Ctrl+C
  - `open /tmp/cam.jpg` should resemble a video stream if image viewer supports live-reloading (such as default RPi image viewer)
    
### 2. Still Image Stream

```bash
rpicam-mjpeg --stream still --output /tmp/cam.jpg
```
* `rpicam-mjpeg --stream still --output /tmp/cam.jpg` will save a timestamped JPEG every 3 seconds.
  - Terminate with Ctrl+C.
  - Output files are saved in the `/tmp` directory.
    
### 3. Video Stream

```bash
rpicam-mjpeg --stream video --output /tmp/vid.mp4
```
* `rpicam-mjpeg --stream video --output /tmp/vid.mp4` will save a 5s MP4 video.
  - Automatically terminate after finishing the 5-second recording.
  - Alternatively, you can manually terminate the process by closing the popup window.
  - **NOTE:** Terminating with Ctrl+C will result in a corrupt video.
  - Output video is saved in the `/tmp` directory.


Clean and Rebuild
---------------------

If you need to clean the build and start fresh, follow these steps:

### 1. Remove the existing build directory:
```bash
rm -rf /home/pi/p05a-rpicam-apps/build
```

### 2. Set up and rebuild the project:
```bash
meson setup build
meson compile -C build
```

### 3. Reinstall the updated build(Optional):
```bash
sudo meson install -C build
```
> **Note:** The installation step is only required if you need to install the binaries system-wide. For local testing, you can simply run the binaries directly from the `build` directory.

This will ensure the previous build is removed, a fresh build is created, and the updated binaries are installed.


License
-------

The source code is made available under the simplified [BSD 2-Clause license](https://spdx.org/licenses/BSD-2-Clause.html).

Status
------

[![ToT libcamera build/run test](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml/badge.svg)](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml)
