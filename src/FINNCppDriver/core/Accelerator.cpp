/**
 * @file Accelerator.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de), Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Implements a wrapper to hide away details of FPGA implementation
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include "Accelerator.h"

#include <FINNCppDriver/core/DeviceHandler.h>          // for DeviceHandler, Uncheck...
#include <FINNCppDriver/utils/ConfigurationStructs.h>  // IWYU pragma: keep
#include <FINNCppDriver/utils/FinnUtils.h>             // for logAndError, unreachable
#include <FINNCppDriver/utils/Logger.h>                // for operator<<, DevNull

#include <algorithm>  // for count_if, find_if, tra...
#include <cstddef>    // for size_t
#include <iterator>   // for back_insert_iterator
#include <stdexcept>  // for runtime_error

#include "ert.h"  // for ert_cmd_state

namespace Finn {
    Accelerator::Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions, bool synchronousInference, unsigned int hostBufferSize) {
        std::transform(deviceDefinitions.begin(), deviceDefinitions.end(), std::back_inserter(devices), [hostBufferSize, synchronousInference](const DeviceWrapper& dew) { return DeviceHandler(dew, synchronousInference, hostBufferSize); });
    }

    std::string Accelerator::loggerPrefix() { return "[Accelerator] "; }


    /****** GETTER / SETTER ******/
    DeviceHandler& Accelerator::getDeviceHandler(unsigned int deviceIndex) {
        if (!containsDevice(deviceIndex)) {
            FinnUtils::logAndError<std::runtime_error>("Tried retrieving a deviceHandler with an unknown index " + std::to_string(deviceIndex));
        }
        auto isCorrectHandler = [deviceIndex](const DeviceHandler& dhh) { return dhh.getDeviceIndex() == deviceIndex; };
        if (auto dhIt = std::find_if(devices.begin(), devices.end(), isCorrectHandler); dhIt != devices.end()) {
            return *dhIt;
        }
        FinnUtils::unreachable();
        return devices[0];
    }

    bool Accelerator::containsDevice(unsigned int deviceIndex) {
        return std::count_if(devices.begin(), devices.end(), [deviceIndex](const DeviceHandler& dh) { return dh.getDeviceIndex() == deviceIndex; }) > 0;
    }

    std::vector<DeviceHandler>::iterator Accelerator::begin() { return devices.begin(); }

    std::vector<DeviceHandler>::iterator Accelerator::end() { return devices.end(); }


    /****** USER METHODS ******/

    // cppcheck-suppress unusedFunction
    [[maybe_unused]] UncheckedStore Accelerator::storeFactory(const unsigned int deviceIndex, const std::string& inputBufferKernelName) {
        if (devices.empty()) {
            FinnUtils::logAndError<std::runtime_error>("Something went wrong. The device list should not be empty.");
        }
        if (containsDevice(deviceIndex)) {
            DeviceHandler& devHand = getDeviceHandler(deviceIndex);
            if (devHand.containsBuffer(inputBufferKernelName, IO::INPUT)) {
                return { devHand, inputBufferKernelName };
            }
        }
        FinnUtils::logAndError<std::runtime_error>("Tried creating a store-closure on a deviceIndex or kernelBufferName which don't exist! Queried index: " + std::to_string(deviceIndex) + ", KernelBufferName: " + inputBufferKernelName);
        FinnUtils::unreachable();
        return { devices[0], "" };
    }

    bool Accelerator::run(const unsigned int deviceIndex, const std::string& inputBufferKernelName) {
        if (containsDevice(deviceIndex)) {
            return getDeviceHandler(deviceIndex).run(inputBufferKernelName);
        }
        else {
            if (containsDevice(0)) {
                return getDeviceHandler(0).run(inputBufferKernelName);
            }
            else {
                // cppcheck-suppress missingReturn
                FinnUtils::logAndError<std::runtime_error>("Tried running data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }

    Finn::vector<uint8_t> Accelerator::retrieveResults(const unsigned int deviceIndex, const std::string& outputBufferKernelName, bool forceArchival) {
        if (containsDevice(deviceIndex)) {
            FINN_LOG_DEBUG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Retrieving results from the specified device index! [accelerator.retrueveResults()]";
            return getDeviceHandler(deviceIndex).retrieveResults(outputBufferKernelName, forceArchival);
        }
        else {
            if (containsDevice(0)) {
                FINN_LOG_DEBUG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Retrieving results from 0  device index! [accelerator.retrueveResults()]";
                return getDeviceHandler(0).retrieveResults(outputBufferKernelName, forceArchival);
            }
            else {
                // cppcheck-suppress missingReturn
                FinnUtils::logAndError<std::runtime_error>("Tried receiving data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }


    ert_cmd_state Accelerator::read(const unsigned int deviceIndex, const std::string& outputBufferKernelName, unsigned int samples) {
        if (containsDevice(deviceIndex)) {
            FINN_LOG_DEBUG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Reading from the specified device index! [accelerator.read()]";
            return getDeviceHandler(deviceIndex).read(outputBufferKernelName, samples);
        }
        else {
            if (containsDevice(0)) {
                FINN_LOG_DEBUG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Reading from 0 device index! [accelerator.read()]";
                return getDeviceHandler(0).read(outputBufferKernelName, samples);
            }
            else {
                FinnUtils::logAndError<std::runtime_error>("Tried reading data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }

    size_t Accelerator::size(SIZE_SPECIFIER ss, unsigned int deviceIndex, const std::string& bufferName) { return getDeviceHandler(deviceIndex).size(ss, bufferName); }

}  // namespace Finn
