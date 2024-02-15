#ifndef DYNAMICMDSPAN
#define DYNAMICMDSPAN

#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <span>
#include <vector>

namespace Finn {

    template<std::input_iterator IteratorType>
    class DynamicMdSpan {
    public:
        using T = typename std::iterator_traits<IteratorType>::value_type;

    private:
        const std::size_t count;
        std::vector<std::size_t> strides;
        std::vector<std::span<T>> mostInnerDims;
        const IteratorType begin;
        const IteratorType end;

    public:
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

        DynamicMdSpan(const IteratorType begin, const IteratorType end, const std::vector<uint>& shape) : count(std::distance(begin, end)), begin(begin), end(end) {
            if (count == 0) {
                throw std::runtime_error("Can not create dynamic mdspan for empty container.");
            }

            setShape(shape);
        };

        std::vector<std::size_t> getStrides() { return strides; }

        std::vector<std::span<T>> getMostInnerDims() const { return mostInnerDims; }
    };

}  // namespace Finn

#endif  // DYNAMICMDSPAN
