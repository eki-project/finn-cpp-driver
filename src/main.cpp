#include <iostream>

#include "utils/finn_types/datatype.h"

int main() {
  DatatypeInt<16> b;
  std::cout << b.sign() << "\n";
  std::cout << "Hello World! " << std::endl;
}