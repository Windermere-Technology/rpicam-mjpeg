import time

def run_test(send_command):
    print("Testing 'ro' command (set rotation)...")
    # Set rotation to 180 degrees
    send_command("ro 180")
    print("Set rotation to 180 degrees")
    time.sleep(2)
    # Capture an image to verify rotation
    send_command("im")
    time.sleep(2)
    # You may manually check the image to verify rotation
    print("Captured image for rotation verification.")

    # Reset rotation to 0 degrees
    send_command("ro 0")
    print("Reset rotation to 0 degrees")
    time.sleep(2)
    print("'ro' command test completed.\n")
