/**
 * @file FPGAReset.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Provides functionality to reset FPGA devices
 * @version 0.1
 * @date 2025-07-12
 *
 * @copyright Copyright (c) 2025
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef FPGARESET_HPP
#define FPGARESET_HPP

#include <fcntl.h>  // For open()
#include <sys/wait.h>
#include <unistd.h>

#include <FINNCppDriver/utils/Logger.hpp>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace Finn {

    /**
     * @brief Helper function to execute a command and optionally capture its output
     *
     * @param args Vector of command arguments (first element is the command)
     * @param captureOutput Whether to capture and return the command output
     * @param silenceOutput Whether to silence command's stdout and stderr
     * @return std::pair<bool, std::string> Pair of (success status, command output if requested)
     */
    std::pair<bool, std::string> executeCommand(const std::vector<std::string>& args, bool captureOutput = false, bool silenceOutput = false) {
        int pipefd[2];
        std::string output;

        if (captureOutput && pipe(pipefd) == -1) {
            std::cerr << "Pipe creation failed: " << strerror(errno) << std::endl;
            return {false, output};
        }

        pid_t pid = fork();

        if (pid == -1) {
            std::cerr << "Fork failed: " << strerror(errno) << std::endl;
            if (captureOutput) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            return {false, output};
        } else if (pid == 0) {
            // Child process
            if (captureOutput) {
                close(pipefd[0]);  // Close read end
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            } else if (silenceOutput) {
                // Redirect stdout and stderr to /dev/null
                int devnull = open("/dev/null", O_WRONLY);
                if (devnull >= 0) {
                    dup2(devnull, STDOUT_FILENO);
                    dup2(devnull, STDERR_FILENO);
                    close(devnull);
                }
            }

            // Prepare arguments for exec
            std::vector<std::string> mutableArgs(args.begin(), args.end());
            std::vector<char*> c_args;
            for (auto& arg : mutableArgs) {
                c_args.push_back(arg.data());
            }
            c_args.push_back(nullptr);

            execvp(c_args[0], c_args.data());

            // If we get here, exec failed
            std::cerr << "Exec failed for " << args[0] << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
        } else {
            // Parent process
            if (captureOutput) {
                close(pipefd[1]);  // Close write end

                // Read output from pipe
                char buffer[4096];
                ssize_t bytes_read;

                while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                    buffer[bytes_read] = '\0';
                    output += buffer;
                }

                close(pipefd[0]);
            }

            // Wait for child process to finish
            int status;
            waitpid(pid, &status, 0);

            return {WIFEXITED(status) && WEXITSTATUS(status) == 0, output};
        }
    }

    /**
     * Reset a specific FPGA device
     * @param deviceId The device ID to reset
     * @return Whether the reset was successful
     */
    bool resetFPGA(const std::string& deviceId) {
        auto [success, _] = executeCommand({"xbutil", "reset", "--force", "-d", deviceId}, false, true);
        if (success) {
            FINN_LOG(loglevel::info) << "Successfully reset FPGA device: " << deviceId << std::endl;
        } else {
            std::cerr << "Failed to reset FPGA device: " << deviceId << std::endl;
        }
        return success;
    }

    /**
     * Get a list of all available FPGA devices
     * @return Vector of device IDs
     */
    std::vector<std::string> getDevices() {
        std::vector<std::string> devices;
        auto [success, output] = executeCommand({"xbutil", "examine"}, true);

        if (!success) {
            std::cerr << "Failed to execute 'xbutil examine'" << std::endl;
            return devices;
        }

        // Parse output
        std::istringstream stream(output);
        std::string line;

        while (std::getline(stream, line)) {
            size_t open_bracket = line.find('[');
            size_t close_bracket = line.find(']');

            if (open_bracket != std::string::npos && close_bracket != std::string::npos) {
                std::string device = line.substr(open_bracket + 1, close_bracket - open_bracket - 1);
                devices.push_back(device);
            }
        }

        return devices;
    }

    /**
     * Reset all available FPGA devices
     * Throws runtime_error if no devices are found or if any reset fails
     */
    void resetFPGAS(const int index = -1) {
#ifdef UNITTEST
        // In unit tests, we might want to mock this function or skip it
        FINN_LOG(loglevel::info) << "Skipping FPGA reset in unit tests." << std::endl;
        return;
#endif

        std::vector<std::string> devices = getDevices();
        if (devices.empty()) {
            logAndError<std::runtime_error>("No FPGA devices found. Cannot reset.");
        }

        if (index != -1) {
            if (index >= devices.size()) {
                logAndError<std::runtime_error>("Invalid device index: " + std::to_string(index) + ". Available devices: " + std::to_string(devices.size()));
            }
            if (!resetFPGA(devices[index])) {
                logAndError<std::runtime_error>("Failed to reset FPGA device at index: " + std::to_string(index));
            }
            std::this_thread::sleep_for(1000ms);  // Wait for the reset to complete
            return;
        }

        bool allSuccessful = true;
        std::string failedDevices;

        for (const auto& device : devices) {
            if (!resetFPGA(device)) {
                allSuccessful = false;
                if (!failedDevices.empty())
                    failedDevices += ", ";
                failedDevices += device;
            }
        }
        std::this_thread::sleep_for(1000ms);  // Wait for the reset to complete

        if (!allSuccessful) {
            logAndError<std::runtime_error>("Failed to reset FPGA device(s): " + failedDevices);
        } else {
            FINN_LOG(loglevel::info) << "Successfully reset all " << devices.size() << " FPGA device(s)" << std::endl;
        }
    }

}  // namespace Finn

#endif  // !FPGARESET_HPP