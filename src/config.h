#ifndef _SRC_CONFIG_H_
#define _SRC_CONFIG_H_

#include <string>
#include <string_view>

// Helpers
#include "utils/types.h"

namespace Config {
    static constexpr PLATFORM platform = PLATFORM::ALVEO;
    static constexpr TRANSFER_MODE transferMode = TRANSFER_MODE::MEMORY_BUFFERED;


    static const bytewidth_list_t INPUT_BYTEWIDTH = {1, 1, 2};
    static const bytewidth_list_t OUTPUT_BYTEWIDTH = {1, 1, 2};

    static constexpr size_t idma_num = 1;
    static const std::initializer_list<std::string_view> IDMA_NAMES = {"a", "b", "c"};
    static const shape_list_t ISHAPE_NORMAL = {{1, 2, 3}};
    static const shape_list_t ISHAPE_FOLDED = {{1, 2, 3}};
    static const shape_list_t ISHAPE_PACKED = {{1, 2, 3}};

    static constexpr size_t odma_num = 1;
    static const std::initializer_list<std::string_view> ODMA_NAMES = {"a", "b", "c"};
    static const shape_list_t OSHAPE_NORMAL = {{1, 2, 3}};
    static const shape_list_t OSHAPE_FOLDED = {{1, 2, 3}};
    static const shape_list_t OSHAPE_PACKED = {{1, 2, 3}};
};  // namespace Config

#endif  // _SRC_CONFIG_H_
