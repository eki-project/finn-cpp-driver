#include <iostream>

#include "utils/mdspan.h"

int main() {
  std::cout << "Hello World! " << std::endl;
  std::array<int, 4> d = {0, 1, 2, 3};
  stdex::mdspan m{d.data(), stdex::extents{2, 2}};
  for (std::size_t i = 0; i < m.extent(0); ++i)
    for (std::size_t j = 0; j < m.extent(1); ++j)
      std::cout << "m[" << i << ", " << j << "] == " << m(i, j) << " Using []\n";
}