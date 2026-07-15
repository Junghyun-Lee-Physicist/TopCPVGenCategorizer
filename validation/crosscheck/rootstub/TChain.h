#ifndef ROOTSTUB_TCHAIN_H
#define ROOTSTUB_TCHAIN_H
#include "TTree.h"
class TChain : public TTree {
public:
    explicit TChain(const char* = "") {}
    int Add(const char*) { return 1; }
};
#endif
