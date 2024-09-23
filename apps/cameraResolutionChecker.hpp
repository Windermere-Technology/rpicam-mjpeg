#ifndef CAMERA_RESOLUTION_CHECKER_H
#define CAMERA_RESOLUTION_CHECKER_H

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

class CameraResolutionChecker {
public:
    // Method to check available resolutions for video modes
    std::pair<int, int> getHighestVideoResolution();


private:
    // Helper method to execute system command and capture output
    void executeCommand(const std::string& command, const std::string& outputFile);
};

#endif // CAMERA_RESOLUTION_CHECKER_H
