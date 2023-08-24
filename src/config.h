#ifndef _SRC_CONFIG_H_
#define _SRC_CONFIG_H_

#include <string>

// Helpers
#include "utils/types.h"

namespace Config {
    constexpr PLATFORM platform = PLATFORM::ALVEO;
    constexpr TRANSFER_MODE transferMode = TRANSFER_MODE::MEMORY_BUFFERED;

    const bytewidth_list_t inputBytewidth = {1, 1, 2};
    const bytewidth_list_t outputBytewidth = {1, 1, 2};

    constexpr size_t idmaNum = 1;
    const std::vector<std::string> idmaNames = {"a", "b", "c"};
    const shape_list_t ishapeNormal = {{1, 2, 3}};
    const shape_list_t ishapeFolded = {{1, 2, 3}};
    const shape_list_t ishapePacked = {{1, 2, 3}};

    constexpr size_t odmaNum = 1;
    const std::vector<std::string> odmaNames = {"a", "b", "c"};
    const shape_list_t oshapeNormal = {{1, 2, 3}};
    const shape_list_t oshapeFolded = {{1, 2, 3}};
    const shape_list_t oshapePacked = {{1, 2, 3}};
};  // namespace Config

#endif  // _SRC_CONFIG_H_
