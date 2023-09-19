#ifndef BASEDRIVER_HPP
#define BASEDRIVER_HPP

#include <cinttypes>  // for uint8_t
#include <filesystem>
#include <memory>

#include "../utils/FinnUtils.h"
#include "Accelerator.h"

#include "../utils/Types.h"
#include "../utils/FinnDatatypes.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace Finn {

    /**
     * @brief Class to implement the meat of the Finn Driver.
     *
     * @tparam T Datatype in which the data is stored (e.g. uint8_t)
     * @tparam F FINN-Datatype of input data
     * @tparam S FINN-Datatype of output data
     */
    // TODO(bwintermann,linusjun): Add templating back in, if benchmarks show that it is too slow without!
    template<typename F, typename S, typename T = uint8_t>
    class BaseDriver {
         private:
        Accelerator accelerator;
         public:
        BaseDriver(const std::filesystem::path& configPath) {
            auto devWrappers = readConfigAndInit(configPath);  // After this shapeNormal, shapeFolded and shapePacked should be filled.
            accelerator = Accelerator(devWrappers);

        };
        BaseDriver(BaseDriver&&) noexcept = default;
        BaseDriver(const BaseDriver&) noexcept = delete;
        BaseDriver& operator=(BaseDriver&&) noexcept = default;
        BaseDriver& operator=(const BaseDriver&) = delete;
        virtual ~BaseDriver() = default;

         private:
        /**
         * 
         * @brief Does checks on the passed shapes, to see if they are mathematically correctly usable and convertible 
         * 
         */
        void checkShapes(const shape_t& pShapeNormal, const shape_t& pShapeFolded, const shape_t& pShapePacked) {
            // The following line calculates the new innermost dimension needed to represent the previous innermost dimension as type T's
            // It can be expressed as floor(d * b / 8) + 1
            // TODO(bwintermann): Add checks for output shapes
            unsigned int calculatedInnermostDimension = static_cast<unsigned int>(F().bitwidth() * FinnUtils::innermostDimension(pShapeFolded) / (sizeof(T) * 8)) + 1;
            if (FinnUtils::shapeToElements(pShapeNormal) != FinnUtils::shapeToElements(pShapeFolded)) {
                FinnUtils::logAndError<std::runtime_error>("Mismatches in shapes! shape_normal and shape_folded should amount to the same number of elements!");
            }
            if (FinnUtils::innermostDimension(pShapePacked) != calculatedInnermostDimension) {
                FinnUtils::logAndError<std::runtime_error>("Mismatches in shapes! shape_packed's innermost dimension in " + FinnUtils::shapeToString(pShapePacked) + " does not equal the calculated innermost dimension " +
                                                           std::to_string(calculatedInnermostDimension));
            }
        }

         public:
        template<typename InputIt>
        void infer(InputIt first, InputIt last) {
            // TODO(linusjun): Do inference here
            fold();
            pack();
            // write to accelerators
            // this should somehow work for multi fpga accelerators
            accelerator.write(first, last);
            // read from accelerators
            unpack();
            unfold();
            // return nullptr;
        }

         protected:
        /**
         * @brief Read the JSON from a configuration file and initialize the needed DeviceWrapper objects 
         * 
         * @param configPath 
         * @return std::vector<DeviceWrapper> 
         */
        std::vector<DeviceWrapper> readConfigAndInit(const std::filesystem::path& configPath) {
            std::ifstream f(configPath.string());
            json dataJson = json::parse(f);
            std::vector<DeviceWrapper> devs;
            for (auto& fpgaDevice : dataJson) {
                DeviceWrapper devWrap;
                from_json(fpgaDevice, devWrap);

                // Check every shape for validity
                for (auto& elem : devWrap.idmas) {
                    auto ebd = std::dynamic_pointer_cast<ExtendedBufferDescriptor>(elem);
                    checkShapes(ebd->normalShape, ebd->foldedShape, ebd->packedShape);
                }
                devs.emplace_back(devWrap);
            }
            return devs;
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

    };


}  // namespace Finn

#endif  // BASEDRIVER_H