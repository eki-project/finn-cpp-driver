// PLACEHOLDER FOR TESTING
#include <string>
#include <vector>

const std::string platform = "alveo";
const std::string transferMode = "memory_buffered";

const std::vector<int> INPUT_BYTEWIDTH = {1, 1, 2};
const std::vector<int> OUTPUT_BYTEWIDTH = {1, 1, 2};

const std::vector<std::string> IDMA_NAMES = {"a", "b", "c"};
const std::vector<std::vector<int>> ISHAPE_NORMAL = {{1, 2, 3}};
const std::vector<std::vector<int>> ISHAPE_FOLDED = {{1, 2, 3}};
const std::vector<std::vector<int>> ISHAPE_PACKED = {{1, 2, 3}};

const std::vector<std::string> ODMA_NAMES = {"a", "b", "c"};
const std::vector<std::vector<int>> OSHAPE_NORMAL = {{1, 2, 3}};
const std::vector<std::vector<int>> OSHAPE_FOLDED = {{1, 2, 3}};
const std::vector<std::vector<int>> OSHAPE_PACKED = {{1, 2, 3}};