import time
import os
from PIL import Image, ImageFilter, ImageChops, ImageStat

def run_test(send_command):
    print("Testing 'sh' command (set sharpness)...")
    try:
        # Capture image with default sharpness
        still_output = "/tmp/cam.jpg"
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture default image for sharpness test.")
        image_default = Image.open(still_output).convert('L')  # Convert to grayscale

        # Set sharpness to +50
        send_command("sh 50")
        print("Set sharpness to +50")
        time.sleep(2)

        # Capture image with increased sharpness
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture image with increased sharpness.")
        image_sharp = Image.open(still_output).convert('L')

        # Reset sharpness to 0
        send_command("sh 0")
        print("Reset sharpness to 0")
        time.sleep(2)

        # Edge detection to measure sharpness
        edges_default = image_default.filter(ImageFilter.FIND_EDGES)
        edges_sharp = image_sharp.filter(ImageFilter.FIND_EDGES)

        # Compute the difference in edge strength
        diff = ImageChops.difference(edges_sharp, edges_default)
        stat_diff = ImageStat.Stat(diff)
        mean_diff = stat_diff.mean[0]

        if mean_diff < 5:  # Threshold can be adjusted
            raise Exception("Sharpness change did not affect the image as expected.")
        else:
            print("Sharpness change detected successfully.")

        print("'sh' command test completed.\n")
    except Exception as e:
        print(f"'sh' command test failed: {e}")
        raise
