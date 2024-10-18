import time
import os
from PIL import Image

def run_test(send_command):
    print("Testing 'im' command (capture image)...")
    still_output = "/tmp/cam.jpg"
    try:
        # Remove existing image if it exists
        if os.path.exists(still_output):
            os.remove(still_output)

        # Send 'im' command to capture an image
        send_command("im")
        time.sleep(2)

        # Verify that the image file exists and is valid
        if os.path.exists(still_output):
            # Attempt to open the image
            with Image.open(still_output) as img:
                img.verify()  # Verify that it's a valid image
            print("Image captured successfully.")
        else:
            raise Exception("Failed to capture image.")
        print("'im' command test completed.\n")
    except Exception as e:
        print(f"'im' command test failed: {e}")
        raise
