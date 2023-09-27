#ifndef DATAPACKING_HPP
#define DATAPACKING_HPP

#include <cstdint>
#include <vector>

namespace Finn {

    template<typename T>
    std::vector<uint8_t> pack(const std::vector<T>& foldedVec) {}

}  // namespace Finn

#endif  // DATAPACKING_HPP
