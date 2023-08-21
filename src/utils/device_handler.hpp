#ifndef DEVICE_HANDLER_H
#define DEVICE_HANDLER_H

// Default
#include <string>
#include <vector>

// Helpers
#include "driver.h"

// XRT
#include "experimental/xrt_ip.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

// TODO(bwintermann): Remove, only here because this type might change a few times over next few iterations
using Shapes = std::initializer_list<std::initializer_list<unsigned int>>;
using Bytewidths = std::initializer_list<unsigned int>;


/**
 * @brief This class handles creating a device, it's kernel objects and holds relevant related variables and functions to interact with it, mainly acting as a FINN specific wrapper for xrt::bo and xrt::device objects 
 * 
 * @tparam T The predominant type of how data is input and output 
 */
template <typename T>
class DeviceHandler {
    const int deviceIndex;
    const std::string binaryFile;
    DRIVER_MODE driverMode;
    TRANSFER_MODE transferMode;

    xrt::device device;
    xrt::uuid uuid;
    xrt::ip kernelIp;

    std::vector<xrt::bo> inputBufferObjects;
    std::vector<xrt::bo> outputBufferObjects;
    std::vector<MemoryMap<T>> inputMemoryMaps;
    std::vector<MemoryMap<T>> outputMemoryMaps;

    
    public:
    /**
     * @brief Construct a new Device Handler object. Requires the Bytewidths, dimensions and shape-types of all input and outputs
     * 
     * @param _deviceIndex The device index, usually 0? 
     * @param _binaryFile The path to the .xclbin FINN file 
     * @param _driverMode The mode for which the driver is instantiated (memory_buffered or memory-less streaming)
     * @param inputBytewidths  
     * @param outputBytewidths 
     * @param inputShapes 
     * @param outputShapes 
     * @param inputShapeType 
     * @param outputShapeType 
     */
    DeviceHandler(const int _deviceIndex, const std::string _binaryFile, const DRIVER_MODE _driverMode, const Bytewidths& inputBytewidths, const Bytewidths& outputBytewidths, const Shapes& inputShapes, const Shapes& outputShapes, const SHAPE_TYPE inputShapeType, const SHAPE_TYPE outputShapeType) {
        // TODO(bwintermann): Maybe required to pass multiple shapes (folded, packed, normal etc.) later on?
        deviceIndex = _deviceIndex;
        binaryFile = _binaryFile;
        driverMode = _driverMode;

        initializeDevice();
        initializeBufferObjects(inputBytewidths, inputShapes, outputBytewidths, outputShapes);
        initializeMemoryMaps(inputShapes, inputShapeType, outputShapes, outputShapeType);
    }

    /**
     * @brief Initializes and fill the class members variables for the XRT device, its UUID, and the IP handler 
     * 
     */
    void initializeDevice() {
        device = xrt::device(deviceIndex);
        uuid = device.load_xclbin(binaryFile);
        kernelIp = xrt::ip(device, uuid, "PLACEHOLDER_KERNEL_NAME"); // TODO(bwintermann): Remove kernel placeholder
    }

    /**
     * @brief Create IO XRT BufferObjects from the given bytewidths and dimensions 
     * 
     * @param inBytewidths 
     * @param inDims
     * @param outBytewidths 
     * @param outDims 
     */
    void initializeBufferObjects(const Bytewidths& inBytewidths, const Shapes& inDims, const Bytewidths& outBytewidths, const Shapes& outDims) {
        inputBufferObjects = createIOBuffers(device, inBytewidths, inDims);
        outputBufferObjects = createIOBuffers(device, outBytewidths, outDims);
    }

    /**
     * @brief Create memory maps the template type given by the device handler class. 
     * 
     * @param inputDims 
     * @param inputShapeType 
     * @param outputDims 
     * @param outputShapeType 
     */
    void initializeMemoryMaps(const Shapes& inputDims, const SHAPE_TYPE inputShapeType, const Shapes& outputDims, const SHAPE_TYPE outputShapeType) {
        inputMemoryMaps = createMemoryMaps<T>(inputBufferObjects, inputDims, inputShapeType);
        outputMemoryMaps = createMemoryMaps<T>(outputBufferObjects, outputDims, outputShapeType);
    }
    
    /**
     * @brief Write randomized values to a buffer map. Raises an error if a write fails- 
     * 
     * @tparam T The type of the memory map.
     * @tparam U The bitwidth of the FINN datatype that is used to fill the map
     * @tparam D The FINN datatype that is used to fill the map. Must be able to be contained in the datatype T
     * @param mmap The MemoryMap itself
     * @param datatype The FINN Datatype (subclass)
     */
    template<typename T, typename U, IsDatatype<U> D = Datatype<U>>  // This typeparameter should usually be a pointer, which was returned by xrt::bo.map<>()
    static void fillBufferMapRandomized(MemoryMap<T>& mmap, D& datatype) {
        // TODO(bwintermann): Need ability to differentiate between float and int!
        // TODO(bwintermann): Check if the datatype of the map T fits the FINN datatype D that is passed (in bitwidth and fixed/float/int)
        // Integer values
        for (unsigned int i = 0; i < mmap.getElementCount(); i++) {
            BUFFER_OP_RESULT res = mmap.writeSingleElement(std::experimental::randint(datatype.min(), datatype.max()), i); 
            
            if (res == BUFFER_OP_RESULT::OVER_BOUNDS_WRITE) {
                std::runtime_error("Error when trying to fill a memory mapped buffer: The write index exceeded the bounds of the map (tried to write at " + std::to_string(i) + " but bounds of map are 0 and " + std::to_string(mmap.getElementCount()) + ")!");
            }
        }
    }


    // Create buffers, one buffer per given shape, and bytewidth
    /**
     * @brief Create a vector of XRT buffer objects to interact with the fpga. Errors if the number of passed bitwidths does not match the number of shapes given.
     * 
     * @param device The XRT device to map the buffer to 
     * @param widths The bitwidths of the buffers datatypes 
     * @param shape The shapes of the buffer
     * @return std::vector<xrt::bo> 
     */
    static std::vector<xrt::bo> createIOBuffers(const xrt::device& device, const Bytewidths& widths, const Shapes& shape) {
        if (widths.size() != shape.size()) {
            std::length_error("The number of bytewidths passed does not match the number of shape lists passed (" + std::to_string(widths.size()) + ", " + std::to_string(shape.size()) + ")!");
        }

        // FIXME, TODO(bwintermann): Differentiate between BITwidth and BYTEwidth (which gets passed by FINN?)
        std::vector<xrt::bo> buffers = {};
        for (unsigned int i = 0; i < widths.size(); i++) {
            auto elements = static_cast<unsigned int>(std::accumulate(std::begin(shape.begin()[i]), std::end(shape.begin()[i]), 1, std::multiplies<>()));
            buffers.emplace_back(xrt::bo(device, static_cast<size_t>(widths.begin()[i] * elements), xrt::bo::flags::cacheable, 1));  // TODO(bwintermann): Correct memory group setting missing, assuming 1 here
        }
        return buffers;
    }


    /**
     * @brief Create a vector of Memory Map objects from a vector of XRT buffer objects, shapes and shapetypes
     * 
     * @tparam T The datatype of the data map that the buffer should be mapped to  
     * @param buffers The vector of buffers (usually created by createIOBuffers)
     * @param shapes
     * @param shapeType 
     * @return std::vector<MemoryMap<T>> 
     */
    template<typename T>
    static std::vector<MemoryMap<T>> createMemoryMaps(std::vector<xrt::bo>& buffers, Shapes shapes, SHAPE_TYPE shapeType) {
        std::vector<MemoryMap<T>> maps = {};
        unsigned int index = 0;
        for (xrt::bo& buffer : buffers) {
            MemoryMap<T> memmap = {buffer.map<T*>(), buffer.size(), shapes.begin()[index], shapeType};
            maps.emplace_back(memmap);
            index++;
        }
        return maps;
    }

    /**
     * @brief Creates a tensor (stdex::mdspan) from a given memory map (for easier math manipulation)
     * 
     * @tparam T Datatype of the memory map 
     * @param mmap  
     * @return decltype(auto) 
     */
    template<typename T>
    static decltype(auto) createTensorFromMap(MemoryMap<T>& mmap) {
        auto expectedElements = static_cast<unsigned int>(std::accumulate(std::begin(dimensions), std::end(dimensions), 1, std::multiplies<>()));
        if (expectedElements != mmap.size) {
            throw std::length_error("During creation of Tensor (mdspan) from a xrt::bo map: Map has size " + std::to_string(mmap.size) + " but the dimensions array fits " + std::to_string(expectedElements) + " elements!");
        }
        return makeMDSpan(mmap.map, mmap.dims);
    }

    // TODO(bwintermann): Implementation
    void executeBatch();

};

#endif