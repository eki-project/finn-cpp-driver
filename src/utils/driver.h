#ifndef DRIVER_HPP
#define DRIVER_HPP

enum class DRIVER_MODE { EXECUTE = 0, THROUGHPUT_TEST = 1 };

enum class SHAPE_TYPE { NORMAL = 0, FOLDED = 1, PACKED = 2 };

template<typename T>
struct MemoryMap {
    T* map;
    std::size_t size;
    std::initializer_list<unsigned int> dims;
    SHAPE_TYPE shapeType;
};

#endif