#include "cameraResolutionChecker.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <utility>
#include <limits>

// Method to check available video modes and return the highest resolution
std::pair<int, int> CameraResolutionChecker::getHighestVideoResolution() {
    const std::string outputFile = "camera_info.txt";
    
    // Run the libcamera command to list available camera modes and store it in a file
    std::string command = "libcamera-hello --list-camera > " + outputFile;
    std::system(command.c_str());

    std::ifstream file(outputFile);
    std::string line;
    int maxWidth = 0, maxHeight = 0;
    
    std::cout << "Available Video Modes:\n";

    // Read the file to find video modes and determine the highest resolution
    while (std::getline(file, line)) {
        if (line.find("fps") != std::string::npos) {
            std::cout << line << std::endl;
            
            // Extract the resolution from the line
            size_t xPos = line.find('x');
            if (xPos != std::string::npos) {
                try {
                    int width = std::stoi(line.substr(0, xPos));
                    int height = std::stoi(line.substr(xPos + 1, line.find(' ', xPos) - xPos));
                    
                    // Compare to find the highest resolution
                    if (width * height > maxWidth * maxHeight) {
                        maxWidth = width;
                        maxHeight = height;
                    }
                } catch (const std::invalid_argument &e) {
                    std::cerr << "Error: Invalid number format in resolution." << std::endl;
                } catch (const std::out_of_range &e) {
                    std::cerr << "Error: Number out of range in resolution." << std::endl;
                }
            }
        }
    }

    // Clean up the temporary file
    std::remove(outputFile.c_str());

    // Return the highest resolution found
    if (maxWidth == 0 || maxHeight == 0) {
        std::cout << "No valid video resolutions found." << std::endl;
    }

    return {maxWidth, maxHeight};
}
