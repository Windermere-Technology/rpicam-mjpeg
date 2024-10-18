import time
import os

def run_test(send_command):
    print("Testing 'qu' command (set image quality)...")
    # Set image quality to 10%
    send_command("qu 10")
    print("Set image quality to 10%")
    time.sleep(2)
    # Capture an image to verify quality
    send_command("im")
    time.sleep(2)
    # Verify the size of the image file
    still_output = "/tmp/cam.jpg"
    if os.path.exists(still_output):
        file_size = os.path.getsize(still_output)
        print(f"Image captured with size: {file_size} bytes (low quality expected).")
    else:
        print("Failed to capture image.")

    # Reset image quality to 100%
    send_command("qu 100")
    print("Reset image quality to 100%")
    time.sleep(2)
    print("'qu' command test completed.\n")
