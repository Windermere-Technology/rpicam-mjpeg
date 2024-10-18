import time

def run_test(send_command):
    print("Testing 'ec' command (set exposure compensation)...")
    # Set exposure compensation to +2
    send_command("ec 2")
    print("Set exposure compensation to +2")
    time.sleep(2)
    # Capture an image to verify exposure compensation
    send_command("im")
    time.sleep(2)
    # Manually verify the image for exposure changes
    print("Captured image for exposure compensation verification.")

    # Reset exposure compensation to 0
    send_command("ec 0")
    print("Reset exposure compensation to 0")
    time.sleep(2)
    print("'ec' command test completed.\n")
