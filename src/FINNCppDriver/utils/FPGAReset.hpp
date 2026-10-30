/**
 * @file FPGAReset.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Provides functionality to reset FPGA devices
 * @version 0.2
 * @date 2025-07-12
 *
 * @copyright Copyright (c) 2025
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef FPGARESET_HPP
#define FPGARESET_HPP

#include <fcntl.h>  // For O_WRONLY
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <FINNCppDriver/utils/Logger.hpp>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifndef UNITTEST
    #include "xrt/xrt_device.h"
#endif

extern char** environ;  // NOLINT: provided by the C runtime, needed by posix_spawnp

namespace Finn {

    /**
     * @brief Helper function to execute a command and optionally capture its output
     *
     * Uses posix_spawn instead of fork: the driver is multi-threaded, and only async-signal-safe code may run between
     * fork and exec in such a process.
     *
     * @param args Vector of command arguments (first element is the command, looked up in PATH)
     * @param captureOutput Whether to capture and return the command's stdout
     * @param silenceOutput Whether to silence the command's stdout and stderr
     * @return std::pair<bool, std::string> Pair of (success status, command output if requested)
     */
    inline std::pair<bool, std::string> executeCommand(const std::vector<std::string>& args, bool captureOutput = false, bool silenceOutput = false) {
        if (args.empty()) {
            return {false, ""};
        }
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));  // NOLINT: posix_spawn takes char* const[] but does not modify the strings
        }
        argv.push_back(nullptr);

        int pipefd[2] = {-1, -1};
        if (captureOutput && pipe(pipefd) == -1) {
            FINN_LOG(loglevel::error) << "Pipe creation failed: " << strerror(errno);
            return {false, ""};
        }

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        if (captureOutput) {
            posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
            posix_spawn_file_actions_addclose(&actions, pipefd[0]);
            posix_spawn_file_actions_addclose(&actions, pipefd[1]);
        } else if (silenceOutput) {
            posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
            posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        }

        pid_t pid = -1;
        const int spawnResult = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        if (captureOutput) {
            close(pipefd[1]);
        }
        if (spawnResult != 0) {
            FINN_LOG(loglevel::error) << "Could not run " << args[0] << ": " << strerror(spawnResult);
            if (captureOutput) {
                close(pipefd[0]);
            }
            return {false, ""};
        }

        std::string output;
        if (captureOutput) {
            char buffer[4096];
            while (true) {
                const ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    output.append(buffer, static_cast<std::size_t>(bytesRead));
                } else if (bytesRead == 0 || errno != EINTR) {
                    break;
                }
            }
            close(pipefd[0]);
        }

        int status = 0;
        while (waitpid(pid, &status, 0) == -1) {
            if (errno != EINTR) {
                FINN_LOG(loglevel::error) << "waitpid failed for " << args[0] << ": " << strerror(errno);
                return {false, output};
            }
        }
        return {WIFEXITED(status) && WEXITSTATUS(status) == 0, output};
    }

    /**
     * @brief Reset a specific FPGA device
     *
     * @param bdf The PCIe BDF of the device to reset (xbutil only accepts BDFs)
     * @return Whether the reset was successful
     */
    inline bool resetFPGA(const std::string& bdf) {
        auto [success, output] = executeCommand({"xbutil", "reset", "--force", "-d", bdf}, false, true);
        if (success) {
            FINN_LOG(loglevel::info) << "Successfully reset FPGA device: " << bdf;
        } else {
            FINN_LOG(loglevel::error) << "Failed to reset FPGA device: " << bdf;
        }
        return success;
    }

    /**
     * @brief Get a list of all available FPGA devices (BDFs) as reported by xbutil examine
     *
     * @return Vector of device BDFs
     */
    inline std::vector<std::string> getDevices() {
        std::vector<std::string> devices;
        auto [success, output] = executeCommand({"xbutil", "examine"}, true);

        if (!success) {
            FINN_LOG(loglevel::error) << "Failed to execute 'xbutil examine'";
            return devices;
        }

        // Device lines look like "[0000:61:00.1]  :  xilinx_u55c_..."
        std::size_t pos = 0;
        while ((pos = output.find('[', pos)) != std::string::npos) {
            const std::size_t close = output.find(']', pos);
            if (close == std::string::npos) {
                break;
            }
            const std::string candidate = output.substr(pos + 1, close - pos - 1);
            if (candidate.size() >= 12 && candidate[4] == ':' && candidate[7] == ':' && candidate[10] == '.') {
                devices.push_back(candidate);
            }
            pos = close + 1;
        }
        return devices;
    }

    /**
     * @brief Whether the user disabled the reset on driver construction via the environment (FINN_SKIP_FPGA_RESET=1)
     *
     * Skipping the reset makes driver construction several seconds faster, but a previous run that left data in the
     * accelerator is then not cleaned up.
     */
    inline bool fpgaResetDisabled() {
        const char* value = std::getenv("FINN_SKIP_FPGA_RESET");
        return value != nullptr && *value != '\0' && std::string_view(value) != "0";
    }

    /**
     * @brief Reset the FPGA device with the given XRT device index, or all devices for index -1
     *
     * The BDF is resolved through XRT itself, so the index always refers to the same card that xrt::device(index) opens.
     *
     * @throws std::runtime_error if the device cannot be found or the reset fails
     */
    inline void resetFPGAS([[maybe_unused]] const int index = -1) {
#ifdef UNITTEST
        // In unit tests, we might want to mock this function or skip it
        FINN_LOG(loglevel::info) << "Skipping FPGA reset in unit tests.";
        return;
#else
        if (fpgaResetDisabled()) {
            FINN_LOG(loglevel::warning) << "FPGA reset skipped because FINN_SKIP_FPGA_RESET is set";
            return;
        }

        if (index != -1) {
            std::string bdf;
            {
                // Resolve the BDF with a short-lived handle so that no handle is open while the card is reset
                xrt::device device(static_cast<unsigned int>(index));
                bdf = device.get_info<xrt::info::device::bdf>();
            }
            if (!resetFPGA(bdf)) {
                logAndError<std::runtime_error>("Failed to reset FPGA device at index: " + std::to_string(index) + " (" + bdf + ")");
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));  // Wait for the reset to complete
            return;
        }

        std::vector<std::string> devices = getDevices();
        if (devices.empty()) {
            logAndError<std::runtime_error>("No FPGA devices found. Cannot reset.");
        }

        std::string failedDevices;
        for (const auto& device : devices) {
            if (!resetFPGA(device)) {
                if (!failedDevices.empty()) {
                    failedDevices += ", ";
                }
                failedDevices += device;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));  // Wait for the reset to complete

        if (!failedDevices.empty()) {
            logAndError<std::runtime_error>("Failed to reset FPGA device(s): " + failedDevices);
        }
        FINN_LOG(loglevel::info) << "Successfully reset all " << devices.size() << " FPGA device(s)";
#endif
    }

}  // namespace Finn

#endif  // !FPGARESET_HPP
