// PLACEHOLDER FOR TESTING
#include <string>
#include <vector>

const std::string PLATFORM = "alveo";
const std::string TRANSFER_MODE = "memory_buffered";

const std::initializer_list<int> INPUT_BYTEWIDTH = {1, 1, 2};
const std::initializer_list<int> OUTPUT_BYTEWIDTH = {1, 1, 2};

const std::initializer_list<std::string> IDMA_NAMES = {"a", "b", "c"};
const std::initializer_list<std::initializer_list<int>> ISHAPE_NORMAL = {{1, 2, 3}};
const std::initializer_list<std::initializer_list<int>> ISHAPE_FOLDED = {{1, 2, 3}};
const std::initializer_list<std::initializer_list<int>> ISHAPE_PACKED = {{1, 2, 3}};

const std::initializer_list<std::string> ODMA_NAMES = {"a", "b", "c"};
const std::initializer_list<std::initializer_list<int>> OSHAPE_NORMAL = {{1, 2, 3}};
const std::initializer_list<std::initializer_list<int>> OSHAPE_FOLDED = {{1, 2, 3}};
const std::initializer_list<std::initializer_list<int>> OSHAPE_PACKED = {{1, 2, 3}};