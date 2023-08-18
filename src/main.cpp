#include <iostream>

#include "utils/finn_types/datatype.hpp"
#include "utils/mdspan.h"


int main() {
  DatatypeFloat a;
  DatatypeFloat d;
  static_assert(a == d);

  static DatatypeInt<16> b;
  static DatatypeInt<17> c;
  static_assert(b != c);

  static DatatypeInt<17> e;
  static_assert(e == c);


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