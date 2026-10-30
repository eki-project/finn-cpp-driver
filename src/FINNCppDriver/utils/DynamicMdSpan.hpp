/**
 * @file DynamicMdSpan.hpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Implements a multi dimensional span like type with a dynamic number of dimensions
 * @version 0.2
 * @date 2023-02-15
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef DYNAMICMDSPAN
#define DYNAMICMDSPAN

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace Finn {

    /**
     * @brief Implements a multi dimensional span like type with a dynamic number of dimensions
     *
     * Only the innermost dimension is materialised: the data is viewed as `innerCount()` contiguous rows of
     * `innerStride()` elements each, starting at `data()`.
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
        /**
         * @brief Element type of the row spans (const when the iterator does not allow writes)
         *
         */
        using SpanElement = std::conditional_t<std::is_const_v<std::remove_reference_t<std::iter_reference_t<IteratorType>>>, const T, T>;

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
         * @brief Spans of all inner most dimensions covered by the DynamicMDSpan, built on the first getMostInnerDims() call.
         * The inference paths only need the strides; with an innermost dimension of one element this would be one span per
         * element (8.4M spans, 134 MB, 9 ms per call for a batch of 4096 RadioML inputs)
         *
         */
        mutable std::vector<std::span<SpanElement>> mostInnerDims;
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
         * @brief Computes all strides needed to access the container correctly
         *
         * @param shape Shape that the input should be interpreted as
         */
        void setShape(const std::vector<unsigned int>& shape) {
            if (shape.empty()) {
                throw std::runtime_error("Can not create dynamic mdspan for empty Shape.");
            }
            const std::size_t shapeElements = std::accumulate(std::begin(shape), std::end(shape), std::size_t{1}, std::multiplies<std::size_t>());
            if (count != shapeElements) {
                throw std::runtime_error("Elements in input vector (" + std::to_string(count) + ") does not match elements in dimensions (" + std::to_string(shapeElements) + ")!");
            }

            // Begin building up strides from smallest to largest
            strides.resize(shape.size() + 1);
            auto ritStrides = strides.rbegin();
            // set last dim to 1, because it is a continous array
            (*ritStrides++) = 1;
            for (auto it = shape.rbegin(); it != shape.rend(); ++it, ++ritStrides) {
                (*ritStrides) = *(ritStrides - 1) * (*it);
            }

            mostInnerDims.clear();
        }

        /**
         * @brief Construct a new Dynamic Md Span object
         *
         * @param begin Iterator to begin of input container
         * @param end Iterator to end of input container
         * @param shape Shape in which the input container should be interpreted
         */
        DynamicMdSpan(const IteratorType begin, const IteratorType end, const std::vector<unsigned int>& shape) : count(static_cast<std::size_t>(std::distance(begin, end))), begin(begin), end(end) {
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
        std::vector<std::size_t> getStrides() const { return strides; }

        /**
         * @brief Get the Most Inner Dims object. Builds them on the first call, so concurrent first calls on one object race
         *
         * @return const std::vector<std::span<SpanElement>>&
         */
        const std::vector<std::span<SpanElement>>& getMostInnerDims() const {
            if (mostInnerDims.empty()) {
                const std::size_t stridingInner = innerStride();
                mostInnerDims.reserve(count / stridingInner);
                for (auto firstElem = begin; end - firstElem > 0; firstElem += static_cast<std::iter_difference_t<IteratorType>>(stridingInner)) {
                    mostInnerDims.emplace_back(std::span<SpanElement>(firstElem, stridingInner));
                }
            }
            return mostInnerDims;
        }

        /**
         * @brief Number of elements in one innermost row
         *
         */
        std::size_t innerStride() const { return *(strides.rbegin() + 1); }

        /**
         * @brief Number of innermost rows
         *
         */
        std::size_t innerCount() const { return count / innerStride(); }

        /**
         * @brief Total number of elements
         *
         */
        std::size_t size() const { return count; }

        /**
         * @brief Iterator to the first element
         *
         */
        IteratorType data() const { return begin; }
    };

}  // namespace Finn

#endif  // DYNAMICMDSPAN
