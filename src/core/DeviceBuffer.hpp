#include <boost/circular_buffer.hpp>
#include <magic_enum.hpp>

#include "../utils/FinnDatatypes.hpp"
#include "../utils/Types.h"
#include "../utils/Logger.h"
#include "../utils/RingBuffer.hpp"

#include "xrt.h"
#include "xrt/xrt_bo.h"


// TODO(bwintermann): Replace this->... with using DeviceBuffer<T,F>::...

namespace Finn {


    template<typename T, typename F>
    class DeviceBuffer {
        const std::string name;
        const size_t numbers;
        const shape_t shape;

        xrt::bo internalBo;
        xrt::kernel& associatedKernel;
        
        T* map;
        const size_t mapSize;

        RingBuffer<T> ringBuffer;

        DeviceBuffer(
            const std::string& pName,
            xrt::device& device,
            xrt::kernel& pAssociatedKernel,
            const shape_t pShape,
            unsigned int ringBufferSizeFactor
        ) : name(pName),
            numbers(FinnUtils::shapeToElements(pShape)),
            shape(pShape),
            mapSize(F().template requiredElements<T>()),
            internalBo(xrt::bo(device, mapSize * sizeof(T), 0)),
            associatedKernel(pAssociatedKernel),
            map(internalBo.map<T*>()),
            ringBuffer(RingBuffer<T>(ringBufferSizeFactor, mapSize)) 
        {

        }

        virtual void sync();
        virtual void execute();

    };

    template<typename T, typename F>
    class DeviceInputBuffer : DeviceBuffer<T,F> {
        using DeviceBuffer<T,F>::DeviceBuffer;
        const IO ioMode = IO::INPUT;
        bool executeAutomatically = false;

        public:
        void setExecuteAutomatically(bool value) {
            executeAutomatically = value;
        }
        
        bool isExecutedAutomatically() {
            return executeAutomatically;
        }

        void sync() {
            this->internalBo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }

        void execute() {
            // TODO(bwintermann): Add arguments for kernel run!
            auto run = associatedKernel(this->internalBo);
            run.start();
            run.wait();
        }

        void loadMap() {
            this->ringBuffer.read(this->map, this->mapSize);
        }

        void preloadInputData(const std::vector<T>& vec) {
            this->ringBuffer.store(vec);
            if (executeAutomatically && this->ringBuffer.isFull()) {
                loadMap();
                sync();
                execute();
            }
        }

        void write() {
            if (this->ringBuffer.isPartValid()) {
                loadMap();
                sync();
                execute();
            }
            this->ringBuffer.cycleHeadPart();
        }

    };


    template<typename T, typename F>
    class DeviceOutputBuffer : DeviceBuffer<T,F> {
        const IO ioMode = IO::OUTPUT;
        std::vector<std::vector<T>> longTermStorage;


        public:    
        void sync() {
            this->internalBo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        }

        void execute() {
            // TODO(bwintermann): Add arguments for kernel run!
            auto run = associatedKernel(this->internalBo);
            run.start();
            run.wait();
        }

        void saveMap() {
            this->ringBuffer.store(this->map, this->mapSize);
        }

        void archiveValidBufferParts() {
            for (index_t i = 0; i < this->ringBuffer.size(SIZE_SPECIFIER::PARTS); i++) {
                if (this->ringBuffer.isPartValid(i)) {
                    longTermStorage.push_back(this->ringBuffer.getPart(i, false));
                }
            }
        }

        void read(unsigned int samples) {
            for (unsigned int i = 0; i < samples; i++) {
                execute();
                sync();
                saveMap();
                if (this->ringBuffer.isFull()) {
                    archiveValidBufferParts();
                }
            }
        }


    };
} // namespace Finn