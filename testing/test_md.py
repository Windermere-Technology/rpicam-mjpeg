import time
import os

def run_test(send_command):
    print("Testing 'md' command (motion detection)...")
    # Path to motion output (modify as per your configuration)
    motion_output = "/var/www/html/motion_output.txt"

    # Remove existing motion output if it exists
    if os.path.exists(motion_output):
        os.remove(motion_output)

    # Enable motion detection
    send_command("md 1")
    print("Enabled motion detection")
    time.sleep(5)  # Give some time for motion detection to start

    # Simulate motion or wait for motion events
    print("Waiting for motion detection (please move in front of the camera)...")
    time.sleep(10)  # Adjust timing as needed

    # Check if motion output is generated
    if os.path.exists(motion_output):
        print("Motion detected and output generated.")
    else:
        print("No motion detected or motion output not generated.")

    # Disable motion detection
    send_command("md 0")
    print("Disabled motion detection")
    time.sleep(2)
    print("'md' command test completed.\n")
