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

#include <algorithm>  // for transform
#include <cstddef>    // for size_t
#include <functional>
#include <iterator>  // for back_insert_iterator, back_inserter
#include <memory>    // for allocator_traits<>::value_type

#include "../utils/ConfigurationStructs.h"
#include "../utils/FinnUtils.h"
#include "DeviceHandler.h"
#include "ert.h"

namespace Finn {
    Accelerator::Accelerator(const std::vector<DeviceWrapper>& deviceDefinitions, unsigned int hostBufferSize) {
        std::transform(deviceDefinitions.begin(), deviceDefinitions.end(), std::back_inserter(devices), [hostBufferSize](const DeviceWrapper& dew) { return DeviceHandler(dew, hostBufferSize); });
    }

    std::string Accelerator::loggerPrefix() { return "[Accelerator] "; }

    /****** GETTER / SETTER ******/
    DeviceHandler& Accelerator::getDeviceHandler(unsigned int deviceIndex) {
        if (!containsDevice(deviceIndex)) {
            FinnUtils::logAndError<std::runtime_error>("Tried retrieving a deviceHandler with an unknown index");
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
    //! Make this either a factory or do the checks before calling store to save performance
    bool Accelerator::store(const std::vector<uint8_t>& data, const unsigned int deviceIndex, const std::string& inputBufferKernelName) {
        if (containsDevice(deviceIndex)) {
            return getDeviceHandler(deviceIndex).store(data, inputBufferKernelName);
        } else {
            if (containsDevice(0)) {
                return getDeviceHandler(0).store(data, inputBufferKernelName);
            } else {
                FinnUtils::logAndError<std::runtime_error>("Tried storing data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }

    // cppcheck-suppress unusedFunction
    [[maybe_unused]] UncheckedStore Accelerator::storeFactory(const unsigned int deviceIndex, const std::string& inputBufferKernelName) {
        if (containsDevice(deviceIndex)) {
            DeviceHandler& devHand = getDeviceHandler(deviceIndex);
            if (devHand.containsBuffer(inputBufferKernelName, IO::INPUT)) {
                return {devHand, inputBufferKernelName};
            }
        }
        FinnUtils::logAndError<std::runtime_error>("Tried creating a store-closure on a deviceIndex or kernelBufferName which don't exist!");
        FinnUtils::unreachable();
        return {devices[0], ""};
    }

    bool Accelerator::run(const unsigned int deviceIndex, const std::string& inputBufferKernelName) {
        if (containsDevice(deviceIndex)) {
            return getDeviceHandler(deviceIndex).run(inputBufferKernelName);
        } else {
            if (containsDevice(0)) {
                return getDeviceHandler(0).run(inputBufferKernelName);
            } else {
                FinnUtils::logAndError<std::runtime_error>("Tried running data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }

    std::vector<std::vector<uint8_t>> Accelerator::retrieveResults(const unsigned int deviceIndex, const std::string& outputBufferKernelName, bool forceArchival) {
        if (containsDevice(deviceIndex)) {
            FINN_LOG_DEBUG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Retrieving results from the specified device index! [accelerator.retrueveResults()]";
            return getDeviceHandler(deviceIndex).retrieveResults(outputBufferKernelName, forceArchival);
        } else {
            if (containsDevice(0)) {
                FINN_LOG_DEBUG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Retrieving results from 0  device index! [accelerator.retrueveResults()]";
                return getDeviceHandler(0).retrieveResults(outputBufferKernelName, forceArchival);
            } else {
                FinnUtils::logAndError<std::runtime_error>("Tried receiving data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }


    ert_cmd_state Accelerator::read(const unsigned int deviceIndex, const std::string& outputBufferKernelName, unsigned int samples) {
        if (containsDevice(deviceIndex)) {
            FINN_LOG_DEBUG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Reading from the specified device index! [accelerator.read()]";
            return getDeviceHandler(deviceIndex).read(outputBufferKernelName, samples);
        } else {
            if (containsDevice(0)) {
                FINN_LOG_DEBUG(Logger::getLogger(), loglevel::info) << loggerPrefix() << "Reading from 0 device index! [accelerator.read()]";
                return getDeviceHandler(0).read(outputBufferKernelName, samples);
            } else {
                FinnUtils::logAndError<std::runtime_error>("Tried reading data in a devicehandler with an invalid deviceIndex!");
            }
        }
    }

    size_t Accelerator::size(SIZE_SPECIFIER ss, unsigned int deviceIndex, const std::string& bufferName) { return getDeviceHandler(deviceIndex).size(ss, bufferName); }

}  // namespace Finn
