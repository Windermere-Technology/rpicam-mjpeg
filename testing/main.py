import subprocess
import os
import time
import sys

# Paths
FIFO_PATH = "/var/www/html/FIFO"
PROGRAM_PATH = "./build/apps/rpicam-mjpeg"
VIDEO_OUTPUT = "/tmp/cam.mp4"
STILL_OUTPUT = "/tmp/cam.jpg"
PREVIEW_OUTPUT = "/dev/shm/mjpeg/cam.jpg"

class TestResult:
    def __init__(self, name):
        self.name = name
        self.passed = True
        self.message = ""

def run_program():
    # Ensure the FIFO exists
    if not os.path.exists(FIFO_PATH):
        print("Please create the FIFO file first, with permission set to 777")

    # Command to run the rpicam_mjpeg program
    cmd = [
        PROGRAM_PATH,
        "--video_path", VIDEO_OUTPUT,
        "--image_path", STILL_OUTPUT,
        "--preview_path", PREVIEW_OUTPUT,
        "--control_file", FIFO_PATH
    ]

    # Start the rpicam_mjpeg program
    process = subprocess.Popen(cmd)
    print("Started rpicam_mjpeg program")

    return process

def send_command(command):
    print(f"Sending command: {command}")
    with open(FIFO_PATH, 'w') as fifo:
        fifo.write(command + '\n')

def main():
    # Run the program
    process = run_program()

    test_results = []

    try:
        # Import test scripts
        import test_im
        import test_ca
        import test_pv
        import test_ro
        import test_fl
        import test_sc
        import test_md
        import test_wb
        import test_mm
        import test_ec
        import test_ag
        import test_is
        import test_px
        import test_co
        import test_br
        import test_sa
        import test_qu
        import test_bi
        import test_sh

        # Run tests
        test_modules = [
            test_im,
            test_ca,
            test_pv,
            test_ro,
            test_fl,
            test_sc,
            test_md,
            test_wb,
            test_mm,
            test_ec,
            test_ag,
            test_is,
            test_px,
            test_co,
            test_br,
            test_sa,
            test_qu,
            test_bi,
            test_sh
        ]

        for test_module in test_modules:
            result = TestResult(test_module.__name__)
            try:
                test_module.run_test(send_command)
                result.passed = True
                result.message = "Test passed"
            except Exception as e:
                result.passed = False
                result.message = str(e)
            test_results.append(result)

    finally:
        # Clean up: Terminate the rpicam_mjpeg process
        process.terminate()
        process.wait()
        print("Terminated rpicam_mjpeg program")

    # Generate testing report
    with open('testing_report.txt', 'w') as report_file:
        for result in test_results:
            status = "PASSED" if result.passed else "FAILED"
            report_file.write(f"{result.name}: {status}\n")
            report_file.write(f"  {result.message}\n")
            print(f"{result.name}: {status}\n")
    # Exit with code 1 if any test failed
    if any(not result.passed for result in test_results):
        sys.exit(1)

if __name__ == "__main__":
    main()
