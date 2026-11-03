/**
 * @file ConfigTest.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Unittest for the FINN Runtime config
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */
#include <FINNCppDriver/utils/ConfigurationStructs.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include "gtest/gtest.h"

class ConfigTest : public ::testing::Test {
     protected:
    std::filesystem::path configPath;

    void SetUp() override { configPath = std::format("ConfigTest_{}.json", ::testing::UnitTest::GetInstance()->current_test_info()->name()); }

    void TearDown() override { std::filesystem::remove(configPath); }

    /**
     * @brief Build the JSON description of a device with the given number of IDMAs and ODMAs
     */
    static json makeDevice(unsigned int xrtDeviceIndex, std::size_t idmaCount, std::size_t odmaCount) {
        json idmas = json::array();
        for (std::size_t i = 0; i < idmaCount; ++i) {
            idmas.push_back({{"kernelName", std::format("idma{}", i)}, {"normalShape", {1, 300}}, {"foldedShape", {1, 10, 30}}, {"packedShape", {1, 10, 8}}});
        }
        json odmas = json::array();
        for (std::size_t i = 0; i < odmaCount; ++i) {
            odmas.push_back({{"kernelName", std::format("odma{}", i)}, {"normalShape", {1, 1}}, {"foldedShape", {1, 1, 1}}, {"packedShape", {1, 1, 1}}});
        }
        return {{"xrtDeviceIndex", xrtDeviceIndex}, {"xclbinPath", "finn-accel.xclbin"}, {"idmas", idmas}, {"odmas", odmas}};
    }

    /**
     * @brief Write the config to configPath and parse it
     */
    Finn::Config parse(const json& configJson) {
        std::ofstream(configPath) << configJson.dump();
        return Finn::createConfigFromPath(configPath);
    }

    /**
     * @brief Expect parsing the config to throw a std::runtime_error whose message contains expectedMessagePart
     */
    void expectConfigError(const json& configJson, const std::string& expectedMessagePart) {
        try {
            parse(configJson);
            ADD_FAILURE() << "Expected a std::runtime_error containing \"" << expectedMessagePart << "\", but parsing succeeded";
        } catch (const std::runtime_error& e) { EXPECT_NE(std::string(e.what()).find(expectedMessagePart), std::string::npos) << "Error message was: " << e.what(); }
    }
};

// Regression: a single device that is not xrtDeviceIndex 0 used to fail because the driver assumed device 0
TEST_F(ConfigTest, LegacySingleDeviceWithNonZeroIndex) {
    const Finn::Config config = parse(json::array({makeDevice(1, 1, 1)}));
    EXPECT_EQ(config.deviceWrappers.size(), 1U);
    EXPECT_EQ(config.inputDeviceIndex, 1U);
    EXPECT_EQ(config.outputDeviceIndex, 1U);
    EXPECT_EQ(config.inputKernelIndex, 0U);
    EXPECT_EQ(config.outputKernelIndex, 0U);
    EXPECT_NO_THROW(config.validate());
}

TEST_F(ConfigTest, ObjectFormatSingleDeviceDerivesIndices) {
    const Finn::Config config = parse({{"devices", json::array({makeDevice(2, 1, 1)})}});
    EXPECT_EQ(config.inputDeviceIndex, 2U);
    EXPECT_EQ(config.outputDeviceIndex, 2U);
    EXPECT_EQ(config.inputKernelIndex, 0U);
    EXPECT_EQ(config.outputKernelIndex, 0U);
}

TEST_F(ConfigTest, ObjectFormatExplicitIndices) {
    const Finn::Config config = parse({{"devices", json::array({makeDevice(0, 2, 1), makeDevice(3, 1, 2)})}, {"inputDeviceIndex", 0}, {"outputDeviceIndex", 3}, {"inputKernelIndex", 1}, {"outputKernelIndex", 1}});
    EXPECT_EQ(config.deviceWrappers.size(), 2U);
    EXPECT_EQ(config.inputDeviceIndex, 0U);
    EXPECT_EQ(config.outputDeviceIndex, 3U);
    EXPECT_EQ(config.inputKernelIndex, 1U);
    EXPECT_EQ(config.outputKernelIndex, 1U);
    EXPECT_NO_THROW(config.validate());
}

// The kernel index may be left out per direction if the selected device has a single DMA of that kind
TEST_F(ConfigTest, KernelIndexDerivedOnSelectedDevice) {
    const Finn::Config config = parse({{"devices", json::array({makeDevice(0, 1, 2), makeDevice(1, 2, 1)})}, {"inputDeviceIndex", 0}, {"outputDeviceIndex", 1}});
    EXPECT_EQ(config.inputKernelIndex, 0U);
    EXPECT_EQ(config.outputKernelIndex, 0U);
}

TEST_F(ConfigTest, MultipleDevicesWithoutDeviceIndexThrow) {
    expectConfigError({{"devices", json::array({makeDevice(0, 1, 1), makeDevice(1, 1, 1)})}, {"outputDeviceIndex", 1}}, "\"inputDeviceIndex\" must be given");
    expectConfigError({{"devices", json::array({makeDevice(0, 1, 1), makeDevice(1, 1, 1)})}, {"inputDeviceIndex", 0}}, "\"outputDeviceIndex\" must be given");
}

TEST_F(ConfigTest, LegacyFormatWithMultipleDevicesThrows) { expectConfigError(json::array({makeDevice(0, 1, 1), makeDevice(1, 1, 1)}), "\"inputDeviceIndex\" must be given"); }

TEST_F(ConfigTest, MultipleDmasWithoutKernelIndexThrow) {
    expectConfigError({{"devices", json::array({makeDevice(0, 2, 1)})}}, "\"inputKernelIndex\" must be given");
    expectConfigError({{"devices", json::array({makeDevice(0, 1, 2)})}}, "\"outputKernelIndex\" must be given");
}

TEST_F(ConfigTest, UnknownDeviceIndexThrows) {
    expectConfigError({{"devices", json::array({makeDevice(0, 1, 1)})}, {"inputDeviceIndex", 5}}, "no device has this xrtDeviceIndex");
    expectConfigError({{"devices", json::array({makeDevice(0, 1, 1)})}, {"outputDeviceIndex", 5}}, "no device has this xrtDeviceIndex");
}

TEST_F(ConfigTest, KernelIndexOutOfRangeThrows) {
    expectConfigError({{"devices", json::array({makeDevice(0, 2, 1)})}, {"inputKernelIndex", 2}}, "only has 2 IDMA");
    expectConfigError({{"devices", json::array({makeDevice(0, 1, 1)})}, {"outputKernelIndex", 1}}, "only has 1 ODMA");
}

TEST_F(ConfigTest, DeviceWithoutDmaThrows) {
    expectConfigError({{"devices", json::array({makeDevice(0, 0, 1)})}}, "has no IDMA");
    expectConfigError({{"devices", json::array({makeDevice(0, 1, 0)})}}, "has no ODMA");
}

TEST_F(ConfigTest, NoDevicesThrows) {
    expectConfigError(json::array(), "contains no devices");
    expectConfigError({{"devices", json::array()}}, "contains no devices");
}

TEST_F(ConfigTest, DuplicateXrtDeviceIndexThrows) { expectConfigError(json::array({makeDevice(1, 1, 1), makeDevice(1, 1, 1)}), "Duplicate xrtDeviceIndex 1"); }

TEST_F(ConfigTest, MissingDevicesListThrows) { expectConfigError({{"inputDeviceIndex", 0}}, "must be either a list of devices or an object with a \"devices\" list"); }

// A Config does not have to come from a file, so the driver validates it again before touching the FPGAs
TEST_F(ConfigTest, ValidateRejectsInvalidHandBuiltConfig) {
    const Finn::Config valid = parse(json::array({makeDevice(0, 1, 1)}));

    Finn::Config config = valid;
    config.inputDeviceIndex = 7;
    EXPECT_THROW(config.validate(), std::runtime_error);

    config = valid;
    config.outputDeviceIndex = 7;
    EXPECT_THROW(config.validate(), std::runtime_error);

    config = valid;
    config.inputKernelIndex = 1;
    EXPECT_THROW(config.validate(), std::runtime_error);

    config = valid;
    config.outputKernelIndex = 1;
    EXPECT_THROW(config.validate(), std::runtime_error);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
