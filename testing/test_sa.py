import time
import os
from PIL import Image, ImageStat

def run_test(send_command):
    print("Testing 'sa' command (set saturation)...")
    try:
        # Capture image with default saturation
        still_output = "/tmp/cam.jpg"
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture default image for saturation test.")
        image_default = Image.open(still_output).convert('HSV')
        stat_default = ImageStat.Stat(image_default)
        avg_saturation_default = stat_default.mean[1]

        # Set saturation to -50
        send_command("sa -50")
        print("Set saturation to -50")
        time.sleep(2)

        # Capture image with decreased saturation
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture image with decreased saturation.")
        image_desaturated = Image.open(still_output).convert('HSV')
        stat_desaturated = ImageStat.Stat(image_desaturated)
        avg_saturation_desaturated = stat_desaturated.mean[1]

        # Reset saturation to 0
        send_command("sa 0")
        print("Reset saturation to 0")
        time.sleep(2)

        # Compare average saturation to see if there is a significant difference
        saturation_diff = avg_saturation_default - avg_saturation_desaturated
        if saturation_diff < 5:  # Threshold can be adjusted
            raise Exception("Saturation change did not affect the image as expected.")
        else:
            print("Saturation change detected successfully.")

        print("'sa' command test completed.\n")
    except Exception as e:
        print(f"'sa' command test failed: {e}")
        raise
