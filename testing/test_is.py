import time

def run_test(send_command):
    print("Testing 'is' command (set ISO)...")
    # Set ISO to 800
    send_command("is 800")
    print("Set ISO to 800")
    time.sleep(2)
    # Capture an image to verify ISO setting
    send_command("im")
    time.sleep(2)
    # Manually verify the image for ISO changes
    print("Captured image for ISO verification.")

    # Reset ISO to default (e.g., 100)
    send_command("is 100")
    print("Reset ISO to 100")
    time.sleep(2)
    print("'is' command test completed.\n")
