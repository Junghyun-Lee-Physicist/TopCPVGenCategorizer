/////////////////////////////////////////////////
//  main_gencat.cpp                            //
//  Standalone driver for TopCPVGenCategorizer    //
/////////////////////////////////////////////////

#include <iostream>
#include <string>
#include <cstdio>

#include <TROOT.h>
#include <TChain.h>
#include <TString.h>

#include "./interface/TopCPVGenCategorizer.h"

using namespace std;

TROOT root("GenCat", "NanoAOD Generator-level Categorizer");

int main(int argc, char** argv)
{
    printf("The number of options is: %i\n", argc - 1);

    if (argc < 3) {
        printf("Usage: %s <filelist> <output.root> [seDir] [maxEvents]\n", argv[0]);
        printf("\n");
        printf("  1. filelist    : text file under ./input/ listing NanoAOD .root files\n");
        printf("  2. output.root : output ROOT filename\n");
        printf("  3. seDir       : (optional) output directory prefix\n");
        printf("  4. maxEvents   : (optional) max events to process (-1 = all)\n");
        return 1;
    }

    for (int i = 0; i < argc; ++i)
        printf("  Option %i = %s\n", i, argv[i]);

    const string filelistName = argv[1];
    const string outname      = argv[2];
    const string seDir        = (argc > 3) ? argv[3] : "";
    const Long64_t maxEvt     = (argc > 4) ? std::stoll(argv[4]) : -1;

    const string filelistDir  = "./input/";
    const string filelistPath = filelistDir + filelistName;

    FILE* fl = fopen(filelistPath.c_str(), "r");
    if (!fl) {
        cerr << "[ERROR] cannot open filelist: " << filelistPath << endl;
        return 2;
    }

    TChain* ch = new TChain("Events");
    char buf[1024];
    while (fscanf(fl, "%1023s", buf) != EOF) {
        cout << "  adding: " << buf << endl;
        ch->Add(buf, 0);
    }
    fclose(fl);

    cout << "Total entries after merging: " << ch->GetEntries() << endl;

    try {
        TopCPVGenCategorizer gen(ch, outname, seDir, maxEvt);
        gen.Loop();
    }
    catch (const std::exception& e) {
        cerr << "[FATAL] " << e.what() << endl;
        delete ch;
        return 3;
    }

    delete ch;
    cout << "Done." << endl;
    return 0;
}
