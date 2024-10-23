# rpicam-mjpeg
A replacement high-level driver for the Raspberry Pi RaspiMJPEG C++, integrated with the RPi_Cam_Web_Interface.


# Table of Contents

1. [Introduction](#1-introduction)
   
2. [Setup](#2-setup)
   - [Automatic Setup (Recommended)](#automatic-setup-recommended)
   - [Manually Install RPi_Cam_Web_Interface (Optional)](#manually-install-rpi_cam_web_interface-optional)

3. [Running rpicam-mjpeg on RPi_Cam_Web_Interface](#3-running-rpicam-mjpeg-on-rpi_cam_web_interface)

4. [Starting the Web Interface](#4-starting-the-web-interface)

5. [Running rpicam-mjpeg](#5-running-rpicam-mjpeg)

6. [Clean and Rebuild](#6-clean-and-rebuild)

7. [Testing](#7-testing)

8. [FIFO](#8-fifo)
    
## 1. Introduction

This program is designed to develop a **libcamera-based, multi-stream camera system**. It supports the latest Raspberry Pi models and offers features such as:

- **Preview streams**: A real-time view of the camera feed.
- **Full-resolution video recording**: Capture high-quality video.
- **Still image capture**: Take high-resolution photos.
- **Motion detection**: Automatically trigger events when motion is detected.

### RPi_Cam_Web_Interface Integration

A key aspect of this system is its integration with the **RPi_Cam_Web_Interface**, providing a graphical interface(GUI) for managing all camera operations instead of command line interface(CLI). 


### Supported FIFO Commands. 
See [8. FIFO](#8-fifo) for more details.

| Command | Description                    |
|---------|--------------------------------|
| `im`    | Capture still image            |
| `ca`    | Start/stop video recording     |
| `pv`    | Setup preview                  |
| `ro`    | Set rotation                   |
| `fl`    | Set flipping                   |
| `sc`    | Set counts                     |
| `md`    | Setup motion detection         |
| `wb`    | Adjust white balance           |
| `mm`    | Set metering mode for exposure |
| `ec`    | Adjust exposure compensation   |
| `ag`    | Set analogue gain              |
| `is`    | Set ISO                        |
| `px`    | Set video resolution           |
| `co`    | Adjust image contrast          |
| `br`    | Adjust image brightness        |
| `sa`    | Adjust image saturation        |
| `qu`    | Set image quality              |
| `bi`    | Set video bitrate              |
| `sh`    | Adjust image sharpness         |


## 2. Setup

### Automatic Setup (Recommended)

### Step 1: Install dependencies + rpicam-mjpeg
```bash
git clone git@github.com:consiliumsolutions/RPi_Cam_Web_Interface.git
cd RPi_Cam_Web_Interface
bin/install-rpicam-mjpeg.sh
```

### Step 2: Install the web application
```bash
./install.sh
```

### Step 3: Run the web application + rpicam-mjpeg
```bash
./start.sh
```
Visit our program at: http://localhost/html/ and start using it!

Now, you can directly skip to [3. Running rpicam-mjpeg on RPi_Cam_Web_Interface](#3-running-rpicam-mjpeg-on-rpi_cam_web_interface).

### Manual Setup (Try this only when the Automatic Setup doesn't work)
Follow the following steps to manually set up and install the **rpicam-apps** system on your Raspberry Pi.

### Step 1: Build libcamera
* Build and install the raspberrypi/libcamera library; see documentation [here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-libcamera)
  - **NOTE:** Do not use the `libcamera` packages from the official repositories, these are outdated.

### Step 2: Build rpicam-apps
1. First fetch the necessary dependencies for rpicam-apps.
```bash
sudo apt install -y cmake libboost-program-options-dev libdrm-dev libexif-dev libavdevice-dev
sudo apt install -y meson ninja-build
```
2. Configure the rpicam-apps build
For desktop-based operating systems like Raspberry Pi OS:
```bash
meson setup build -Denable_libav=enabled -Denable_drm=enabled -Denable_egl=enabled -Denable_qt=enabled -Denable_opencv=disabled -Denable_tflite=disabled
```

> **NOTE: `meson setup` only needs to be run once.**


3. Build rpicam-apps with the following command:
```bash
meson compile -C build
```

Final Step (Optional)
To install the `rpicam-apps` binaries system-wide, run:

```bash
sudo meson install -C build
```

> This allows you to use the binaries from anywhere in the terminal without needing to navigate to the build directory each time. 

The official instructions to build rpicam-apps; see documentation [here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-rpicam-apps)

### Step 3: Install RPi_Cam_Web_Interface

This section focuses on setting up the web interface for managing the camera system.

-------
#### Manually Install RPi_Cam_Web_Interface (Optional)

#### GPAC

GPAC is not available in the Bookworm repos (see issue #689). Follow these instructions to build GPAC from source:

1. Clone the GPAC repository:
   ```bash
   git clone https://github.com/gpac/gpac.git
   ```

2. Install the necessary dependencies:
   ```bash
   sudo apt install build-essential pkg-config g++ git cmake yasm
   sudo apt install zlib1g-dev libfreetype6-dev libjpeg-dev libpng-dev libmad0-dev libfaad-dev libogg-dev \
       libvorbis-dev libtheora-dev liba52-0.7.4-dev libavcodec-dev libavformat-dev libavutil-dev \
       libswscale-dev libavdevice-dev libnghttp2-dev libopenjp2-7-dev libcaca-dev libxv-dev \
       x11proto-video-dev libgl1-mesa-dev libglu1-mesa-dev x11proto-gl-dev libxvidcore-dev \
       libssl-dev libjack-jackd2-dev libasound2-dev libpulse-dev libsdl2-dev dvb-apps mesa-utils \
       libcurl4-openssl-dev
   ```

3. Build GPAC:
   ```bash
   cd gpac
   ./configure
   make
   ```

4. Install GPAC system-wide:
   ```bash
   sudo make install
   ```

#### RPi_Cam_Web_Interface

After building and installing GPAC, proceed with the **RPi_Cam_Web_Interface** installation:

1. Clone the **RPi_Cam_Web_Interface** repository:
   ```bash
   git clone https://github.com/silvanmelchior/RPi_Cam_Web_Interface.git
   cd RPi_Cam_Web_Interface
   ```

2. Modify the `install.sh` script to exclude GPAC installation:
   ```bash
   sed -i 's/gpac//p' install.sh
   ```

3. Run the installation script:
   ```bash
   ./install.sh
   ```

4. During installation, use the following options:
   - Start automatically: **No**
   - Port: **80**
--------------------
## 3. Running rpicam-mjpeg on RPi_Cam_Web_Interface

### Prerequisites

If you don’t have the following, create them:

```bash
mkfifo /var/www/html/FIFO  # This should have been created by the installer, however, check if it exists.
mkdir /dev/shm/mjpeg       # This is where the preview stream will be written; it will be deleted across reboots.
```
* This acts the scheduler's FIFO file we will be writing into. 
* `cat` to see the updates in the file

## 4. Starting the Web Interface

Visit `http://localhost/html/` on your Raspberry Pi to access the RPi_Cam_Web_Interface.
> **Info**  
> However, since the browser on the Raspberry Pi might be laggy, it's recommended to access the interface from another device on the same network.  
> Simply open a browser and navigate to `http://<Raspberry_Pi_IP>/html/` (replace `<Raspberry_Pi_IP>` with your Raspberry Pi's local IP address).

The web interface should already be running, but if it is not, you can start the Apache web server using the following command:

```bash
sudo systemctl start apache2
```

When you open the interface for the first time (by visiting `http://<Raspberry_Pi_IP>/html/`), it may appear broken. This is expected as we need to manually set the status updates. 

To fix this, set the default state to `ready` by running the following command:

```bash
echo ready | tr -d '\n' > /dev/shm/mjpeg/status_mjpeg.txt
```

> **Note:** The file will **not** work if there is a trailing newline, which is why we use `tr -d '\n'` to remove it.

After setting the status, you should now see text on the buttons in the web interface, indicating that it is ready.

## 5. Running rpicam-mjpeg

Now, you can start running **rpicam-mjpeg** by executing the following command:

```bash
./build/apps/rpicam-mjpeg --preview-output /dev/shm/mjpeg/cam.jpg --video-output /tmp/cam.mp4 --still-output /tmp/cam.jpg --fifo /var/www/html/FIFO
```

At this point, your web interface should successfully display the preview.
This means that you have successfully configured the **RPi_Cam_Web_Interface** and integrated it with **rpicam-mjpeg**.

### Quitting the FIFO Environment

To quit the FIFO environment and stop **rpicam-mjpeg**, use `Ctrl + C` in the terminal where it is running.

## 6. Clean and Rebuild


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

## 7. Testing

This project implements comprehensive automated testing for the `rpicam_mjpeg` program using Python scripts. The testing suite ensures that each FIFO command functions as expected and that any changes to the codebase do not introduce regressions. The tests include both functional command execution and detailed verification of their effects using image analysis.

### Overview

- **Automated Tests**: Each FIFO command (`im`, `ca`, `pv`, `ro`, `fl`, `sc`, `md`, `wb`, `mm`, `ec`, `ag`, `is`, `px`, `co`, `br`, `sa`, `qu`, `bi`, `sh`) is tested individually to verify its functionality.

### Installation

1. **Install Dependencies**:

   ```bash
   sudo apt install python3 python3-pillow
   ```

2. **Ensure FIFO Directory Exists**

   Ensure FIFO Directory Exists:
The FIFO is located at /var/www/html/FIFO. Ensure this directory exists and has the appropriate permissions.

   ```bash
   sudo mkdir -p /var/www/html
   sudo mkfifo /var/www/html/FIFO
   sudo chmod 777 /var/www/html/FIFO
   ```

### Run the Test
```bash
python3 testing/main.py
```

### Check Test Results
The testing result would be stored in `testing_report.txt` in the main directory, it would also be printed out.

## 8. FIFO

You don’t need FIFO commands to interact with the camera system, but if you want to use it for advanced control, here are the steps:

To quit the FIFO environment and stop **rpicam-mjpeg**, use `Ctrl + C` in the terminal where it is running.

### 1: Still Image Capture
On terminal a:
```bash
./build/apps/rpicam-mjpeg --still-output /tmp/cam.jpg --fifo /tmp/FIFO
```

On terminal b:
```bash
echo 'im' > /tmp/FIFO
```
Run this command to take a still picture

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

### 4: Motion
On terminal a:
```bash
./build/apps/rpicam-mjpeg --motion-output /tmp/schedulerFIFO --fifo /tmp/FIFO
```

On terminal b:
```bash
echo 'md 1' > /tmp/FIFO
```
To start motion detection. 
- `1`: starts the motion detection
> **NOTE: `cat` to see the updates**

```bash
echo 'md 0' > /tmp/FIFO
```
To stop motion detection. 
- `0`: stops the motion detection

### 5: Metering
On terminal a:
```bash
./build/apps/rpicam-mjpeg --video-output /tmp/vid.mp4 --fifo /tmp/FIFO
echo 'ca 1 30' > /tmp/FIFO
echo 'mm centre' > /tmp/FIFO
```
To change metering option during video recording;

### 5: Exposure compensation
On terminal a:
```bash
./build/apps/rpicam-mjpeg --video-output /tmp/vid.mp4 --fifo /tmp/FIFO
```

On terminal b:
```bash
echo 'ca 1 30' > /tmp/FIFO
echo 'ec 5' > /tmp/FIFO
```
To change exposure during video recording, restricted between -10 to 10;

### 6: Red and Blue gain
On terminal a:
```bash
./build/apps/rpicam-mjpeg --video-output /tmp/vid.mp4 --fifo /tmp/FIFO
```

On terminal b:
```bash
echo 'ca 1 30' > /tmp/FIFO
echo 'ag 100 100' > /tmp/FIFO
```
To change red and blue gain during video recording;

### 7: ISO
On terminal a:
```bash
./build/apps/rpicam-mjpeg --video-output /tmp/vid.mp4 --fifo /tmp/FIFO
```

On terminal b:
```bash
echo 'ca 1 30' > /tmp/FIFO
echo 'is 1000' > /tmp/FIFO
```
To change iso during video recording;

License
-------

The source code is made available under the simplified [BSD 2-Clause license](https://spdx.org/licenses/BSD-2-Clause.html).

Status
------
[![ToT libcamera build/run test](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml/badge.svg)](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml)

