// PLACEHOLDER FOR TESTING
#include <string>
#include <vector>

#include "utils/driver.h"

constexpr std::string_view PLATFORM = "alveo";
constexpr TRANSFER_MODE transferMode = TRANSFER_MODE::MEMORY_BUFFERED;

constexpr std::initializer_list<unsigned int> INPUT_BYTEWIDTH = {1, 1, 2};
constexpr std::initializer_list<unsigned int> OUTPUT_BYTEWIDTH = {1, 1, 2};

const std::initializer_list<std::string_view> IDMA_NAMES = {"a", "b", "c"};
constexpr std::initializer_list<std::initializer_list<unsigned int>> ISHAPE_NORMAL = {{1, 2, 3}};
constexpr std::initializer_list<std::initializer_list<unsigned int>> ISHAPE_FOLDED = {{1, 2, 3}};
constexpr std::initializer_list<std::initializer_list<unsigned int>> ISHAPE_PACKED = {{1, 2, 3}};

const std::initializer_list<std::string_view> ODMA_NAMES = {"a", "b", "c"};
constexpr std::initializer_list<std::initializer_list<unsigned int>> OSHAPE_NORMAL = {{1, 2, 3}};
constexpr std::initializer_list<std::initializer_list<unsigned int>> OSHAPE_FOLDED = {{1, 2, 3}};
constexpr std::initializer_list<std::initializer_list<unsigned int>> OSHAPE_PACKED = {{1, 2, 3}};