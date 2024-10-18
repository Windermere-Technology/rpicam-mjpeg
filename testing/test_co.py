import time
import os
from PIL import Image, ImageStat

def run_test(send_command):
    print("Testing 'co' command (set contrast)...")
    try:
        # Capture image with default contrast
        still_output = "/tmp/cam.jpg"
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture default image for contrast test.")
        image_default = Image.open(still_output).convert('L')  # Convert to grayscale

        # Set contrast to +50
        send_command("co 50")
        print("Set contrast to +50")
        time.sleep(2)

        # Capture image with increased contrast
        if os.path.exists(still_output):
            os.remove(still_output)
        send_command("im")
        time.sleep(2)
        if not os.path.exists(still_output):
            raise Exception("Failed to capture image with increased contrast.")
        image_contrast = Image.open(still_output).convert('L')

        # Reset contrast to 0
        send_command("co 0")
        print("Reset contrast to 0")
        time.sleep(2)

        # Compute the histogram standard deviation as a measure of contrast
        stat_default = ImageStat.Stat(image_default)
        contrast_default = stat_default.stddev[0]

        stat_contrast = ImageStat.Stat(image_contrast)
        contrast_contrast = stat_contrast.stddev[0]

        # Compare the contrast values
        contrast_diff = contrast_contrast - contrast_default
        if contrast_diff < 5:  # Threshold can be adjusted
            raise Exception("Contrast change did not affect the image as expected.")
        else:
            print("Contrast change detected successfully.")

        print("'co' command test completed.\n")
    except Exception as e:
        print(f"'co' command test failed: {e}")
        raise
