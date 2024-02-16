/**
 * @file DynamicMdSpan.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements a multi dimensional span like type with a dynamic number of dimensions
 * @version 0.1
 * @date 2023-02-15
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef DYNAMICMDSPAN
#define DYNAMICMDSPAN

#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <span>
#include <vector>

namespace Finn {

    /**
     * @brief Implements a multi dimensional span like type with a dynamic number of dimensions
     *
     * @tparam IteratorType
     */
    template<std::input_iterator IteratorType>
    class DynamicMdSpan {
         public:
        /**
         * @brief Value type referenced by DynamicMDSpan
         *
         */
        using T = typename std::iterator_traits<IteratorType>::value_type;

         private:
        /**
         * @brief Number of elements in DynamicMdSpan
         *
         */
        const std::size_t count;
        /**
         * @brief Access strides in the input span to get correct accesses for each dimension
         *
         */
        std::vector<std::size_t> strides;
        /**
         * @brief Spans of all inner most dimensions covered by the DynamicMDSpan
         *
         */
        std::vector<std::span<T>> mostInnerDims;
        /**
         * @brief Iterator to first element managed
         *
         */
        const IteratorType begin;
        /**
         * @brief Iterator to end of input container
         *
         */
        const IteratorType end;

         public:
        /**
         * @brief Computes all strides needed to access the container correctly and also computes all most inner dimension spans
         *
         * @param shape Shape that the input should be interpreted as
         */
        void setShape(const std::vector<uint>& shape) {
            if (shape.empty()) {
                throw std::runtime_error("Can not create dynamic mdspan for empty Shape.");
            }
            if (count != std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<size_t>())) {
                std::cout << "Distance: " << std::distance(begin, end) << " ; Accumulated Dimensions: " << std::accumulate(std::begin(shape), std::end(shape), 1.0, std::multiplies<double>()) << "\n";
                throw std::runtime_error("Elements in input vector does not match elements in dimensions!");
            }

            // Begin building up strides from smallest to largest
            strides.resize(shape.size() + 1);
            auto ritStrides = strides.rbegin();
            // set last dim to 1, because it is a continous array
            (*ritStrides++) = 1;
            for (auto it = shape.rbegin(); it != shape.rend(); ++it, ++ritStrides) {
                (*ritStrides) = *(ritStrides - 1) * (*it);
            }

            const std::size_t stridingInner = *(strides.rbegin() + 1);
            mostInnerDims.clear();
            mostInnerDims.reserve(count / stridingInner);
            IteratorType firstElem = begin;
            for (auto firstElem = begin; end - firstElem > 0; firstElem += stridingInner) {
                mostInnerDims.emplace_back(std::span<T>(firstElem, stridingInner));
            }
        }

        /**
         * @brief Construct a new Dynamic Md Span object
         *
         * @param begin Iterator to begin of input container
         * @param end Iterator to end of input container
         * @param shape Shape in which the input container should be interpreted
         */
        DynamicMdSpan(const IteratorType begin, const IteratorType end, const std::vector<uint>& shape) : count(std::distance(begin, end)), begin(begin), end(end) {
            if (count == 0) {
                throw std::runtime_error("Can not create dynamic mdspan for empty container.");
            }

            setShape(shape);
        };

        /**
         * @brief Get the Strides object
         *
         * @return std::vector<std::size_t>
         */
        std::vector<std::size_t> getStrides() { return strides; }

        /**
         * @brief Get the Most Inner Dims object
         *
         * @return std::vector<std::span<T>>
         */
        std::vector<std::span<T>> getMostInnerDims() const { return mostInnerDims; }
    };

}  // namespace Finn

#endif  // DYNAMICMDSPAN
