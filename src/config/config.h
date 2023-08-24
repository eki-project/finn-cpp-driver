#ifndef _SRC_CONFIG_H_
#define _SRC_CONFIG_H_

#include <string>
#include <string_view>

// Helpers
#include "utils/types.h"

namespace Config {
    constexpr PLATFORM platform = PLATFORM::ALVEO;
    constexpr TRANSFER_MODE transferMode = TRANSFER_MODE::MEMORY_BUFFERED;

    const bytewidth_list_t INPUT_BYTEWIDTH = {1, 1, 2};
    const bytewidth_list_t OUTPUT_BYTEWIDTH = {1, 1, 2};

    constexpr size_t idma_num = 1;
    const std::initializer_list<std::string> IDMA_NAMES = {"a", "b", "c"};
    const shape_list_t ISHAPE_NORMAL = {{1, 2, 3}};
    const shape_list_t ISHAPE_FOLDED = {{1, 2, 3}};
    const shape_list_t ISHAPE_PACKED = {{1, 2, 3}};

    constexpr size_t odma_num = 1;
    const std::initializer_list<std::string> ODMA_NAMES = {"a", "b", "c"};
    const shape_list_t OSHAPE_NORMAL = {{1, 2, 3}};
    const shape_list_t OSHAPE_FOLDED = {{1, 2, 3}};
    const shape_list_t OSHAPE_PACKED = {{1, 2, 3}};
};  // namespace Config

#endif  // _SRC_CONFIG_H_
