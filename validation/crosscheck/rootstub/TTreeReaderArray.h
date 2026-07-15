#ifndef ROOTSTUB_TTREEREADERARRAY_H
#define ROOTSTUB_TTREEREADERARRAY_H
#include <vector>
#include <cstddef>
#include "RtypesCore.h"
#include "TTreeReader.h"
template <class T>
class TTreeReaderArray {
public:
    TTreeReaderArray() = default;
    TTreeReaderArray(TTreeReader&, const char*) {}
    std::vector<T> v;
    T& operator[](std::size_t i) { return v[i]; }
    const T& operator[](std::size_t i) const { return v[i]; }
    std::size_t GetSize() const { return v.size(); }
};
#endif
