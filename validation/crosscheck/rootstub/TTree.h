#ifndef ROOTSTUB_TTREE_H
#define ROOTSTUB_TTREE_H
#include <vector>
#include "RtypesCore.h"
class TBranch {};
class TTree {
public:
    TTree(const char* = "", const char* = "") {}
    virtual ~TTree() = default;
    template <class T> int Branch(const char*, T*, const char*) { return 0; }
    template <class T> int Branch(const char*, std::vector<T>*) { return 0; }
    TBranch* GetBranch(const char*) { return nullptr; }
    Long64_t GetEntries() const { return 0; }
    int Fill() { return 0; }
    int Write(const char* = nullptr, int = 0, int = 0) { return 0; }
};
#endif
