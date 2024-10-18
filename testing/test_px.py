import time
import os

def run_test(send_command):
    print("Testing 'px' command (set video resolution)...")
    # Set video resolution and frame rates
    send_command("px 1280 720 30 30 640 480 1")
    print("Set video resolution to 1280x720 at 30fps")
    time.sleep(2)

    # Start video recording to verify resolution
    video_output = "/tmp/cam.mp4"
    if os.path.exists(video_output):
        os.remove(video_output)

    send_command("ca 1 5")  # Record for 5 seconds
    print("Started video recording...")
    time.sleep(6)

    # Verify that the video file exists and has content
    if os.path.exists(video_output) and os.path.getsize(video_output) > 0:
        print("Video recorded successfully at new resolution.")
    else:
        print("Failed to record video at new resolution.")

    # Ensure recording is stopped
    send_command("ca 0")
    print("Stopped video recording.")
    time.sleep(2)
    print("'px' command test completed.\n")
