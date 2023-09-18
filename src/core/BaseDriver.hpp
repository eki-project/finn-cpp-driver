#ifndef BASEDRIVER_HPP
#define BASEDRIVER_HPP

#include <cinttypes>  // for uint8_t

#include "Accelerator.h"
#include "../utils/FinnUtils.h"

namespace Finn {

    /**
     * @brief Class to implement the meat of the Finn Driver.
     *
     * @tparam T Datatype in which the data is stored (e.g. uint8_t)
     * @tparam F FINN-Datatype of input data
     * @tparam S FINN-Datatype of output data
     */
    template<typename F, typename S, typename T = uint8_t>
    class BaseDriver {
         public:
        BaseDriver() : accelerator(std::vector<DeviceWrapper>()) {
            readConfigAndInit();  // After this shapeNormal, shapeFolded and shapePacked should be filled.

            // The following line calculates the new innermost dimension needed to represent the previous innermost dimension as type T's
            // It can be expressed as floor(d * b / 8) + 1
            unsigned int calculatedInnermostDimension = static_cast<unsigned int>(F().bitwidth() * FinnUtils::innermostDimension(shapeFolded) / (sizeof(T) * 8)) + 1;

            if (FinnUtils::shapeToElements(shapeNormal) != FinnUtils::shapeToElements(shapeFolded)) {
                FinnUtils::logAndError<std::runtime_error>("Mismatches in shapes! shape_normal and shape_folded should amount to the same number of elements!");
            }

            if (FinnUtils::innermostDimension(shapePacked) != calculatedInnermostDimension) {
                FinnUtils::logAndError<std::runtime_error>("Mismatches in shapes! shape_packed's innermost dimension in " + FinnUtils::shapeToString(shapePacked) + " does not equal the calculated innermost dimension " +
                                                           std::to_string(calculatedInnermostDimension));
            }
        };
        BaseDriver(BaseDriver&&) noexcept = default;
        BaseDriver(const BaseDriver&) noexcept = delete;
        BaseDriver& operator=(BaseDriver&&) = default;
        BaseDriver& operator=(const BaseDriver&) = delete;
        virtual ~BaseDriver() = default;

        template<typename InputIt>
        S infer(InputIt first, InputIt last) {
            // TODO(linusjun): Do inference here
            fold();
            pack();
            // write to accelerators
            // read from accelerators
            unpack();
            unfold();
            return nullptr;
        }

         protected:
        void readConfigAndInit() {
            // TODO(linusjun): Read values from config file and initialize variables
        }

        void fold() {
            // TODO(linusjun): implement. Do we really need this?
        }

        void unfold() {
            // TODO(linusjun): implement. Do we really need this?
        }

        void pack() {
            // TODO(linusjun): implement
        }

        void unpack() {
            // TODO(linusjun): implement
        }

        Accelerator accelerator;
        // TODO(bwintermann): Only one input buffer expected? Make list out of these
        // TODO(linusjun): For multi FPGA support these should probably be packed into objects?
        size_t numbers;       // Numbers of type F: in a shape (1,20) this would be 20
        shape_t shapeNormal;  // Input shape (Type F): (1,20)
        shape_t shapeFolded;  // Folded shape (Type F): (1,2,10)
        shape_t shapePacked;  // Packed shape (Type T): (1,2,3)
    };


}  // namespace Finn

#endif  // BASEDRIVER_H