import time

def run_test(send_command):
    print("Testing 'sc' command (set counts)...")
    # Send 'sc' command
    send_command("sc")
    print("Set counts command sent.")
    # As counts are internal, we verify that the command does not cause errors
    time.sleep(2)
    print("'sc' command test completed.\n")
