# rpicam-apps
This is a small suite of libcamera-based applications to drive the cameras on a Raspberry Pi platform.

>[!WARNING]
>These applications and libraries have been renamed from `libcamera-*` to `rpicam-*`. Symbolic links are installed to allow users to keep using the old application names, but these will be deprecated soon. Users are encouraged to adopt the new application and library names as soon as possible.

Build
-----

### Step 1: Building libcamera
* Build and install the raspberrypi/libcamera library; see documentation [here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-libcamera)
  - **NOTE:** Do not use the `libcamera` packages from the official repositories, these are outdated.

### Step 2: Building rpicam-apps
1. First fetch the necessary dependencies for rpicam-apps.
```bash
sudo apt install -y cmake libboost-program-options-dev libdrm-dev libexif-dev libavdevice-dev
sudo apt install -y meson ninja-build
```
4. Configure the rpicam-apps build
For desktop-based operating systems like Raspberry Pi OS:
```bash
meson setup build -Denable_libav=enabled -Denable_drm=enabled -Denable_egl=enabled -Denable_qt=enabled -Denable_opencv=disabled -Denable_tflite=disabled
```

> **NOTE: `meson setup` only needs to be run once.**


5. Build rpicam-apps with the following command:
```bash
meson compile -C build
```

Final Step (Optional)
To install the `rpicam-apps` binaries system-wide, run:

```bash
sudo meson install -C build
```

> This allows you to use the binaries from anywhere in the terminal without needing to navigate to the build directory each time. If you are only testing locally, you can skip this step and run the binaries directly from the `build` directory.

The official instructions to build rpicam-apps; see documentation [here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-rpicam-apps)


---


Running rpicam-mjpeg
--------------------

There are three subcommands available, each of which will output a different stream type.
At this stage, the subcommands are not configured to run concurrently:

### 1. Preview Stream

```bash
./build/apps/rpicam-mjpeg --preview-output /tmp/cam.jpg --preview-width 640 --preview-height 480
```
* `./build/apps/rpicam-mjpeg --preview-output /tmp/cam.jpg` will behave in a way similar to the RaspiMJPEG preview stream.
  - `open /tmp/cam.jpg` while running should resemble a video stream if image viewer supports live-reloading (such as default RPi image viewer)
  - Terminate with Ctrl+C
    
### 2. Still Image Stream

```bash
./build/apps/rpicam-mjpeg --still-output /tmp/cam.jpg
```
* `./build/apps/rpicam-mjpeg --stream still --output /tmp/cam.jpg` will save a timestamped JPEG.
  - Output files are saved in the `/tmp` directory.
    
### 3. Video Stream

```bash
./build/apps/rpicam-mjpeg --video-output /tmp/vid.mp4
```
* `./build/apps/rpicam-mjpeg --video-output /tmp/vid.mp4` will save a 5s MP4 video.
  - Automatically terminate after finishing the 5-second recording.
  - Alternatively, you can manually terminate the process by closing the popup window.
  - **NOTE:** Terminating with Ctrl+C will result in a corrupt video.
  - Output video is saved in the `/tmp` directory.

### 4. Multi Stream
```bash
./build/apps/rpicam-mjpeg --video-output /tmp/vid.mp4 --preview-output /tmp/cam.jpg --preview-width 640 --preview-height 480
```

This will save a 5s MP4 video `vid.mp4`, and while the video is being captured, also output a preview stream at `/tmp/cam.jpg`

---

FIFO
-----

Currently, FIFO can take 3 different commands:

Firstly, create FIFO by:
```bash
mkfifo /tmp/FIFO
```

### 1: Still Image Capture
On terminal a:
```bash
./build/apps/rpicam-mjpeg --still-output /tmp/cam.jpg --fifo /tmp/FIFO
```

On terminal b:
```bash
echo 'im' > /tmp/FIFO
```
Run this command to take a still picture :wink:



### 2: Video Recording
On terminal a:
```bash
./build/apps/rpicam-mjpeg --video-output /tmp/vid.mp4 --fifo /tmp/FIFO --nopreview
```

On terminal b:
```bash
echo 'ca 1 10' > /tmp/FIFO
```
To record video for 10 seconds;
Or
```bash
echo 'ca 0' > /tmp/FIFO
```
To stop recording. (TBD)

### 3: Preview
On terminal a:
```bash
./build/apps/rpicam-mjpeg --preview-output /tmp/cam.jpg --fifo /tmp/FIFO --nopreview
```

On terminal b:
```bash
echo 'pv 1000 500' > /tmp/FIFO
```
To set the size of preview window as 1000 x 500.

### 4: Metering
On terminal a:
```bash
./build/apps/rpicam-mjpeg --video-output /tmp/vid.mp4 --fifo /tmp/FIFO
```

On terminal b:
```bash
echo 'ca 1 30' > /tmp/FIFO
echo 'mm centre' > /tmp/FIFO
```
To change metering option during video recording;

---

Clean and Rebuild
---------------------

If you need to clean the build and start fresh, follow these steps:

### 1. Remove the existing build directory:
```bash
rm -rf ./build
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

## Multistream Stat
**TODO:**
- [x] Make use of 2 YUV streams and a RAW stream concurrently.
- [ ] Modification to support MotionOptions
- [ ] Assign RAW stream to motion

License
-------

The source code is made available under the simplified [BSD 2-Clause license](https://spdx.org/licenses/BSD-2-Clause.html).

Status
------

[![ToT libcamera build/run test](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml/badge.svg)](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml)

