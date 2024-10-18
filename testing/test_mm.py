import time

def run_test(send_command):
    print("Testing 'mm' command (set metering mode)...")
    # Set metering mode to 'spot'
    send_command("mm spot")
    print("Set metering mode to 'spot'")
    time.sleep(2)
    # Capture an image to verify metering mode
    send_command("im")
    time.sleep(2)
    # Manually verify the image for metering mode effect
    print("Captured image for metering mode verification.")

    # Reset metering mode to 'average'
    send_command("mm average")
    print("Reset metering mode to 'average'")
    time.sleep(2)
    print("'mm' command test completed.\n")
