#ifndef ROOTSTUB_TTREEREADERVALUE_H
#define ROOTSTUB_TTREEREADERVALUE_H
#include "RtypesCore.h"
#include "TTreeReader.h"
template <class T>
class TTreeReaderValue {
public:
    TTreeReaderValue() = default;
    TTreeReaderValue(TTreeReader&, const char*) {}
    T val{};
    T& operator*() { return val; }
    const T& operator*() const { return val; }
    T* Get() { return &val; }
};
#endif
