import time
import os

def run_test(send_command):
    print("Testing 'ca' command (start/stop video recording)...")
    # Remove existing video if it exists
    video_output = "/tmp/cam.mp4"
    if os.path.exists(video_output):
        os.remove(video_output)

    # Start video recording with duration limit
    send_command("ca 1 5")  # Record for 5 seconds
    print("Started video recording...")
    time.sleep(6)  # Wait for recording to complete

    # Verify that the video file exists and has content
    if os.path.exists(video_output) and os.path.getsize(video_output) > 0:
        print("Video recorded successfully.")
    else:
        print("Failed to record video.")

    # Ensure recording is stopped
    send_command("ca 0")
    print("Stopped video recording.")
    time.sleep(2)
    print("'ca' command test completed.\n")
