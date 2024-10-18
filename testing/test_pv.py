import time
import os

def run_test(send_command):
    print("Testing 'pv' command (set preview parameters)...")
    # Path to preview output
    preview_output = "/dev/shm/mjpeg/cam.jpg"

    # Remove existing preview if it exists
    if os.path.exists(preview_output):
        os.remove(preview_output)

    # Send 'pv' command with quality=100, width=640, divider=1
    send_command("pv 100 640 1")
    print("Set preview parameters to quality=100, width=640, divider=1")
    time.sleep(2)

    # Verify that the preview image exists
    if os.path.exists(preview_output):
        print("Preview image updated successfully.")
    else:
        print("Failed to update preview image.")
    print("'pv' command test completed.\n")
