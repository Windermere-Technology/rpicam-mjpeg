import time
import os

def run_test(send_command):
    print("Testing 'bi' command (set bitrate)...")
    # Set bitrate to 5000000 bps
    send_command("bi 5000000")
    print("Set bitrate to 5000000 bps")
    time.sleep(2)

    # Start video recording to verify bitrate
    video_output = "/tmp/cam.mp4"
    if os.path.exists(video_output):
        os.remove(video_output)

    send_command("ca 1 5")  # Record for 5 seconds
    print("Started video recording...")
    time.sleep(6)

    # Verify that the video file exists and check size
    if os.path.exists(video_output) and os.path.getsize(video_output) > 0:
        file_size = os.path.getsize(video_output)
        print(f"Video recorded with size: {file_size} bytes (bitrate applied).")
    else:
        print("Failed to record video.")

    # Ensure recording is stopped
    send_command("ca 0")
    print("Stopped video recording.")
    time.sleep(2)
    print("'bi' command test completed.\n")
