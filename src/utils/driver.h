#ifndef DRIVER_HPP
#define DRIVER_HPP

enum DRIVER_MODE {
    EXECUTE,
    THROUGHPUT_TEST
};

enum SHAPE_TYPE {
    NORMAL,
    FOLDED,
    PACKED
};

template<typename T>
__attribute__((packed)) struct MemoryMap {
    T* map;
    size_t size;
    std::initializer_list<unsigned int> dims;
    SHAPE_TYPE shapeType;
};

#endif