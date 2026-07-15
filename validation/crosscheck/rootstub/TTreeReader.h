#ifndef ROOTSTUB_TTREEREADER_H
#define ROOTSTUB_TTREEREADER_H
#include "RtypesCore.h"
class TTree;
class TChain;
class TTreeReader {
public:
    TTreeReader() = default;
    explicit TTreeReader(TChain*) {}
    bool Next() { return false; }
    TTree* GetTree() { return nullptr; }
};
#endif
