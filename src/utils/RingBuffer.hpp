
#include <boost/circular_buffer.hpp>
#include <magic_enum.hpp>

#include "FinnDatatypes.hpp"
#include "Types.h"
#include "Logger.h"

    template<typename T>
    class RingBuffer {
        boost::circular_buffer<T> buffer;
        std::vector<bool> validParts;
        size_t parts;
        size_t elementsPerPart;
        index_t headPart;

        RingBuffer(const size_t pParts, const size_t pElementsPerPart) : 
            buffer(boost::circular_buffer<T>(pElementsPerPart * pParts)),
            validParts(std::vector<T>(pParts)),
            parts(pParts),
            elementsPerPart(pElementsPerPart)
            headPart(0) 
        {
            for (size_t i = 0; i < validParts.size(); i++) {
                validParts[i] = false;
            }
        }

        private:
        index_t elementIndex(index_t partIndex, index_t offset) {
            return (partIndex * elementsPerPart + offset) % buffer.size();
        }

        void setPartValidity(index_t partIndex, bool validity) {
            if (partIndex > parts) {
                FinnUtils::logAndError<std::length_error>("Tried setting validity for an index that is too large.");
            }
            validParts[partIndex] = validity;
        }


        public:
        size_t size(SIZE_SPECIFIER ss) {
            if (ss == SIZE_SPECIFIER::ELEMENTS) {
                return buffer.size();
            } else if (ss == SIZE_SPECIFIER::BYTES) {
                return buffer.size() * sizeof(T);
            } else if (ss == SIZE_SPECIFIER::PARTS) {
                return parts;
            } else {
                FinnUtils::logAndError<std::runtime_error>("Unknown size specifier!");
            }
        }

        unsigned int countValidParts() {
            unsigned int tmp = 0;
            for (auto val : ringBufferValidParts) {
                if (val) { tmp++; }
            }
            return tmp;
        }

        void cycleHeadPart() {
            headPart = (headPart + 1) % parts;
        }

        void setHeadPart(index_t partIndex) {
            if (partIndex >= parts) {
                FinnUtils::logAndError<std::length_error>("Couldnt set head index manually, value too high!");
            }
            headPart = partIndex;
        }

        bool isPartValid(index_t partIndex) {
            return partIndex < parts && validParts[partIndex];
        }

        bool isPartValid() {
            return validParts[partIndex];
        }

        bool isFull() {
            return countValidParts() == parts;
        }

        void store(const std::vector<T>& vec) {
            if (vec.size() != elementsPerPart) {
                FinnUtils::logAndError<std::length_error>("Size mismatch when storing vector in Ring Buffer!");
            }
            for (size_t i = 0; i < vec.size(); i++) {
                buffer[elementIndex(headPart, i)] = vec[i];
            }
            setPartValidity(headPart, true);
            cycleHeadPart();
        }

        void store(const T& arr, const size_t arrSize) {
            if (arrSize != elementsPerPart) {
                FinnUtils::logAndError<std::length_error>("Size mismatch when storing array in Ring Buffer!");
            }
            for (size_t i = 0; i < arrSize; i++) {
                buffer[elementIndex(headPart, i)] = arr[i];
            }
            setPartValidity(headPart, true);
            cycleHeadPart();
        }

        void setPart(const std::vector<T>& vec, index_t partIndex, bool validity) {
            if (vec.size() != elementsPerPart) {
                FinnUtils::logAndError<std::length_error>("Size mismatch when storing vector in Ring Buffer!");
            }
            for (size_t i = 0; i < vec.size(); i++) {
                buffer[elementIndex(partIndex, i)] = vec[i];
            }
            setPartValidity(partIndex, validity);
        }

        void setPart(const T& arr, const size_t arrSize, index_t partIndex, bool validity) {
            if (arrSize != elementsPerPart) {
                FinnUtils::logAndError<std::length_error>("Size mismatch when storing array in Ring Buffer!");
            }
            for (size_t i = 0; i < arrSize; i++) {
                buffer[elementIndex(partIndex, i)] = arr[i];
            }
            setPartValidity(partIndex, validity);
        }

        std::vector<T> read() {
            std::vector<T> temp;
            for (size_t i = 0; i < elementsPerPart; i++) {
                temp.push_back(buffer[elementIndex(headPart, i)]);
            }
            setPartValidity(headPart, false);
            return temp;
        }

        void read(T& targetArr, size_t arrSize) {
            if (arrSize != elementsPerPart) {
                FinnUtils::logAndError<std::length_error>("Size mismatching when trying to read ring buffer data into an array!");
            }
            for (size_t i = 0; i < elementsPerPart; i++) {
                targetArr[i] = buffer[elementIndex(headPart, i)];
            }
            setPartValidity(headPart, false);
        }

        std::vector<T> getPart(const index_t partIndex, const bool validity) {
            std::vector<T> temp;
            for (size_t i = 0; i < elementsPerPart; i++) {
                temp.push_back(buffer[elementIndex(partIndex, i)]);
            }
            setPartValidity(partIndex, validity);
            return temp;
        }

        void getPart(T& arr, const size_t arrSize, index_t partIndex, bool validity) {
            if (arrSize != elementsPerPart) {
                FinnUtils::logAndError<std::length_error>("Size mismatching when trying to read ring buffer data into an array!");
            }
            for (size_t i = 0; i < elementsPerPart; i++) {
                targetArr[i] = buffer[elementIndex(partIndex, i)];
            }
            setPartValidity(partIndex, validity);
        }
    };