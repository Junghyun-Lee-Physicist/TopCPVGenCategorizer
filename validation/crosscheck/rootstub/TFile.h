#ifndef ROOTSTUB_TFILE_H
#define ROOTSTUB_TFILE_H
#include "RtypesCore.h"
class TFile {
public:
    static TFile* Open(const char*, const char* = "") { return nullptr; }
    bool IsZombie() const { return true; }
    void cd() {}
    void Close() {}
};
#endif
