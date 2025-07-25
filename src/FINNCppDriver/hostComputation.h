#ifndef HOSTCOMPUTATION_H
#define HOSTCOMPUTATION_H

#include <vector>
#include <functional>
#include <cstdint>
#include "../../hostParameters.h"

static constexpr float a = 254 / (thresholds[254] - thresholds[0]);

std::vector<int8_t> multithresholdLinearPerTensor(const std::vector<float> &inp) {
  const size_t size = inp.size();
  std::vector<int8_t> ret(size, -127);

  // Pre-compute constants
  constexpr float min_threshold = thresholds[0] - 0.5f;
  constexpr float max_threshold = thresholds[254] + 0.5f;
  constexpr float offset = thresholds[0];

  // Direct pointers for better performance with small vectors
  int8_t *__restrict ret_data = ret.data();
  const float *__restrict inp_data = inp.data();

  // For small vectors, manual loop unrolling often performs better than SIMD
  // Process elements in groups of 4 when possible
  size_t i = 0;
  for (; i + 3 < size; i += 4) {
    // Process 4 elements at once to improve instruction-level parallelism
    float val0 =
        inp_data[i] < min_threshold
            ? min_threshold
            : (inp_data[i] > max_threshold ? max_threshold : inp_data[i]);
    float val1 = inp_data[i + 1] < min_threshold
                     ? min_threshold
                     : (inp_data[i + 1] > max_threshold ? max_threshold
                                                        : inp_data[i + 1]);
    float val2 = inp_data[i + 2] < min_threshold
                     ? min_threshold
                     : (inp_data[i + 2] > max_threshold ? max_threshold
                                                        : inp_data[i + 2]);
    float val3 = inp_data[i + 3] < min_threshold
                     ? min_threshold
                     : (inp_data[i + 3] > max_threshold ? max_threshold
                                                        : inp_data[i + 3]);

    ret_data[i] += static_cast<int8_t>(
        std::clamp(static_cast<int>((val0 - offset) * a), 0, 254));
    ret_data[i + 1] += static_cast<int8_t>(
        std::clamp(static_cast<int>((val1 - offset) * a), 0, 254));
    ret_data[i + 2] += static_cast<int8_t>(
        std::clamp(static_cast<int>((val2 - offset) * a), 0, 254));
    ret_data[i + 3] += static_cast<int8_t>(
        std::clamp(static_cast<int>((val3 - offset) * a), 0, 254));
  }

  // Handle remaining elements
  for (; i < size; ++i) {
    float val =
        inp_data[i] < min_threshold
            ? min_threshold
            : (inp_data[i] > max_threshold ? max_threshold : inp_data[i]);
    ret_data[i] += static_cast<int8_t>(
        std::clamp(static_cast<int>((val - offset) * a), 0, 254));
  }

  return ret;
}

#endif /* HOSTCOMPUTATION_H */
