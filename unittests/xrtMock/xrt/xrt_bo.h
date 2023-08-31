#ifndef XRT_BO_H
#define XRT_BO_H

#include "../../../src/utils/Logger.h"
#include "../xrt.h"
#include "xrt_device.h"

namespace xrt {
    class bo {
        private:
        xrt::device device;
        size_t byteSize;
        unsigned int group;

        void* memmap = nullptr;
        
        logger_type& logger;

        public:
        bo(xrt::device pDevice, size_t pBytesize, unsigned int pGroup) : 
        device(pDevice),
        byteSize(pBytesize),
        group(pGroup),
        logger(Logger::getLogger()) {
            //FINN_LOG(logger, loglevel::debug) << "(xrtMock) xrt::bo object created!\n";
        }
        

        void sync(XCL_SYNC_MODES);
        ~bo();

        /**
         * @brief Create a T and return it - T should be a pointer type. Additionally save copy of it as a void pointer for freeing in destructor 
         * @attention This class takes care of freeing the map, this is evil pointer stuff :< 
         * 
         * @tparam T 
         * @return T 
         */
        template<typename T>
        T map() {
            //FINN_LOG(logger, loglevel::debug) << "(xrtMock) Map created from xrt::bo with byte size " << byteSize << "!\n";
            T createdMap = static_cast<T>(malloc(byteSize));
            memmap = createdMap;
            return createdMap;
        }

    };
}

#endif