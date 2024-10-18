import time

def run_test(send_command):
    print("Testing 'ag' command (set analog gain)...")
    # Set analog gain to red=150%, blue=150%
    send_command("ag 150 150")
    print("Set analog gain to red=150%, blue=150%")
    time.sleep(2)
    # Capture an image to verify gain changes
    send_command("im")
    time.sleep(2)
    # Manually verify the image for gain changes
    print("Captured image for analog gain verification.")

    # Reset analog gain to defaults (red=100%, blue=100%)
    send_command("ag 100 100")
    print("Reset analog gain to defaults")
    time.sleep(2)
    print("'ag' command test completed.\n")
