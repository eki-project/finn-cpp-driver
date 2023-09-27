#include "../../src/utils/FinnDatatypes.hpp"
#include "../../src/utils/Types.h"
#include "../../src/utils/FinnUtils.h"
#include "../../src/utils/ConfigurationStructs.h"
#include "gtest/gtest.h"
#include <vector>
#include <array>
#include <memory>
#include <filesystem>

namespace FinnUnittest {
    const std::string configFilePath = "../../src/config/exampleConfig.json";
    Finn::Config unittestConfig = Finn::createConfigFromPath(std::filesystem::path(configFilePath));

    auto myShapeNormal = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(unittestConfig.deviceWrappers[0].idmas[0])).normalShape;
    auto myShapeFolded = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(unittestConfig.deviceWrappers[0].idmas[0])).foldedShape;
    auto myShapePacked = (*std::dynamic_pointer_cast<Finn::ExtendedBufferDescriptor>(unittestConfig.deviceWrappers[0].idmas[0])).packedShape;

    const unsigned int hostBufferSize = 10;
    const size_t elementsPerPart = FinnUtils::shapeToElements(myShapePacked);
    const size_t parts = 10;
}