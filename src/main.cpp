#include <algorithm>
#include <iostream>

#include "utils/finn_types/datatype.hpp"
#include "utils/mdspan.h"


int main() {
    DatatypeFloat aFL;
    DatatypeFloat dFL;
    static_assert(aFL == dFL);

    static DatatypeInt<16> bIN;
    static DatatypeInt<17> cIN;
    static_assert(bIN != cIN);
    std::cout << (bIN.allowed(1)) << "\n";
    static DatatypeUInt<17> zUI;
    std::cout << (zUI.allowed(-1)) << "\n";

    static DatatypeInt<17> eIN;
    static_assert(eIN == cIN);


    std::array<int, 8> mem{};
    std::iota(mem.begin(), mem.end(), 0);

    auto spa = makeMDSpan(mem.data(), {4, 2, 1});
    std::cout << spa(0, 1, 0) << "\n";
    std::cout << spa(0, 0, 0) << "\n";
    std::cout << spa(1, 1, 0) << "\n";
    std::cout << spa(1, 0, 0) << "\n";
    std::cout << bIN.getNumPossibleValues() << "\n";
    std::cout << "Hello World! " << std::endl;
}