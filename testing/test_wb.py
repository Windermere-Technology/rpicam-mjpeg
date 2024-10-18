import time
import os
from PIL import Image, ImageStat

def run_test(send_command):
    print("Testing 'wb' command (set white balance)...")
    try:
        # Capture image with default white balance
        still_output = "/tmp/cam.jpg"
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture default image for white balance test.")
        image_default = Image.open(still_output)
        stat_default = ImageStat.Stat(image_default)
        avg_color_default = stat_default.mean

        # Set white balance to 'cloudy'
        send_command("wb cloudy")
        print("Set white balance to 'cloudy'")
        time.sleep(2)

        # Capture image with 'cloudy' white balance
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture image with 'cloudy' white balance.")
        image_cloudy = Image.open(still_output)
        stat_cloudy = ImageStat.Stat(image_cloudy)
        avg_color_cloudy = stat_cloudy.mean

        # Reset white balance to 'auto'
        send_command("wb auto")
        print("Reset white balance to 'auto'")
        time.sleep(2)

        # Compare average colors to see if there is a significant difference
        color_diff = [abs(c1 - c2) for c1, c2 in zip(avg_color_default, avg_color_cloudy)]
        if all(d < 5 for d in color_diff):  # Threshold can be adjusted
            raise Exception("White balance change did not affect the image as expected.")
        else:
            print("White balance change detected successfully.")

        print("'wb' command test completed.\n")
    except Exception as e:
        print(f"'wb' command test failed: {e}")
        raise
