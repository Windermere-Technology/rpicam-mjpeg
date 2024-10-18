import time
import os
from PIL import Image, ImageStat

def run_test(send_command):
    print("Testing 'br' command (set brightness)...")
    try:
        # Capture image with default brightness
        still_output = "/tmp/cam.jpg"
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture default image for brightness test.")
        image_default = Image.open(still_output)
        stat_default = ImageStat.Stat(image_default)
        avg_brightness_default = sum(stat_default.mean) / len(stat_default.mean)

        # Set brightness to 70%
        send_command("br 70")
        print("Set brightness to 70%")
        time.sleep(2)

        # Capture image with increased brightness
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture image with increased brightness.")
        image_bright = Image.open(still_output)
        stat_bright = ImageStat.Stat(image_bright)
        avg_brightness_bright = sum(stat_bright.mean) / len(stat_bright.mean)

        # Reset brightness to 50%
        send_command("br 50")
        print("Reset brightness to 50%")
        time.sleep(2)

        # Compare average brightness to see if there is a significant difference
        brightness_diff = avg_brightness_bright - avg_brightness_default
        if brightness_diff < 5:  # Threshold can be adjusted
            raise Exception("Brightness change did not affect the image as expected.")
        else:
            print("Brightness change detected successfully.")

        print("'br' command test completed.\n")
    except Exception as e:
        print(f"'br' command test failed: {e}")
        raise
