#ifndef HOSTCOMPUTATION_H
#define HOSTCOMPUTATION_H

#include <immintrin.h>  // For AVX/AVX2 intrinsics

#include <cstdint>
#include <functional>
#include <vector>

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

std::vector<float> multiplyAdd(const std::vector<int8_t> &inp) {
    static constexpr size_t N = addVec.size();
    // inp.size() is guaranteed to be a multiple of N
    std::vector<float> ret(inp.size());

    // Use raw pointers with __restrict to help compiler optimize
    const int8_t* __restrict inpPtr = inp.data();
    float* __restrict retPtr = ret.data();

    const size_t totalElements = inp.size();

// Since N is small, fully unroll the inner loop
#pragma omp simd
    for (size_t i = 0; i < totalElements; i += N) {
        // Manual unrolling for common small N values
        if constexpr (N >= 1)
            retPtr[i] = inpPtr[i] * scalingValue + addVec[0];
        if constexpr (N >= 2)
            retPtr[i + 1] = inpPtr[i + 1] * scalingValue + addVec[1];
        if constexpr (N >= 3)
            retPtr[i + 2] = inpPtr[i + 2] * scalingValue + addVec[2];
        if constexpr (N >= 4)
            retPtr[i + 3] = inpPtr[i + 3] * scalingValue + addVec[3];
        if constexpr (N >= 5)
            retPtr[i + 4] = inpPtr[i + 4] * scalingValue + addVec[4];
        if constexpr (N >= 6)
            retPtr[i + 5] = inpPtr[i + 5] * scalingValue + addVec[5];
        if constexpr (N >= 7)
            retPtr[i + 6] = inpPtr[i + 6] * scalingValue + addVec[6];
        if constexpr (N >= 8)
            retPtr[i + 7] = inpPtr[i + 7] * scalingValue + addVec[7];

        // For N > 8, handle remaining elements
        if constexpr (N > 8) {
            for (size_t j = 8; j < N; ++j) {
                retPtr[i + j] = inpPtr[i + j] * scalingValue + addVec[j];
            }
        }
    }

    return ret;
}



#endif /* HOSTCOMPUTATION_H */
