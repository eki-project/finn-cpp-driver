#include <iostream>

#include "utils/finn_types/datatype.h"
#include "utils/mdspan.h"


int main() {
  static DatatypeInt<16> b;
  std::initializer_list<int> iList = {4, 2, 1};

  std::array<int, 8> mem{};
  int i = 0;
  for (auto&& var : mem) {
    var = ++i;
  }
  auto spa = makeMDSpan(mem.data(), {4, 2, 1});
  std::cout << spa(0, 1, 0) << "\n";
  std::cout << spa(0, 0, 0) << "\n";
  std::cout << spa(1, 1, 0) << "\n";
  std::cout << spa(1, 0, 0) << "\n";
  std::cout << b.getNumPossibleValues() << "\n";
  std::cout << "Hello World! " << std::endl;
}