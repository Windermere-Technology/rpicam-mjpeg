import time

def run_test(send_command):
    print("Testing 'fl' command (set flipping)...")
    # Set horizontal and vertical flip
    send_command("fl 3")  # 3 sets both hflip and vflip
    print("Set horizontal and vertical flip")
    time.sleep(2)
    # Capture an image to verify flip
    send_command("im")
    time.sleep(2)
    # You may manually check the image to verify flip
    print("Captured image for flipping verification.")

    # Reset flipping
    send_command("fl 0")
    print("Reset flipping to normal")
    time.sleep(2)
    print("'fl' command test completed.\n")
