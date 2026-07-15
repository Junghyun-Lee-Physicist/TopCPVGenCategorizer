// Cross-validation harness: run TopCPVGenCategorizer v1.8 (plug-in mode, stub
// ROOT) on the SAME three synthetic events as the NtupleForge Python
// framework test (/tmp/fw_test.py) and assert identical derived values.
//   E1: ttbar-like all-hadronic signal
//   E2: explicit-Z Z->tautau background (audit §2b risk (a): must be -30, not -60)
//   E3: boson-less ME mumu background   (audit §2b risk (b): must be 26, not 0)
#include "TopCPVGenCategorizer/interface/TopCPVGenCategorizer.h"  // build from the parent dir (see README.md)

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

template <class T>
using VMap = std::unordered_map<std::string, std::unique_ptr<TTreeReaderValue<T>>>;
template <class T>
using AMap = std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<T>>>;

static VMap<UInt_t>  uintS;
static VMap<Float_t> floatS;
static AMap<Float_t> floatV;
static AMap<Int_t>   intV;
static AMap<Bool_t>  boolV;
static AMap<UChar_t> ucharV;

static int nFail = 0;
#define CHECK_EQ(what, got, want)                                              \
    do {                                                                       \
        const long long g = (long long)(got), w = (long long)(want);           \
        if (g != w) {                                                          \
            std::printf("  FAIL %-28s got=%lld want=%lld\n", what, g, w);      \
            ++nFail;                                                           \
        } else {                                                               \
            std::printf("  ok   %-28s %lld\n", what, g);                       \
        }                                                                      \
    } while (0)

static void ensureRegistered() {
    auto U = [](const char* n) { uintS[n]  = std::make_unique<TTreeReaderValue<UInt_t>>(); };
    auto F = [](const char* n) { floatV[n] = std::make_unique<TTreeReaderArray<Float_t>>(); };
    auto I = [](const char* n) { intV[n]   = std::make_unique<TTreeReaderArray<Int_t>>(); };
    auto C = [](const char* n) { ucharV[n] = std::make_unique<TTreeReaderArray<UChar_t>>(); };
    U("nGenPart"); U("nGenJet"); U("nGenVisTau");
    I("GenPart_pdgId"); I("GenPart_statusFlags");
    I("GenPart_genPartIdxMother"); I("GenPart_status");
    F("GenPart_pt"); F("GenPart_eta"); F("GenPart_phi"); F("GenPart_mass");
    F("GenJet_pt"); F("GenJet_eta"); F("GenJet_phi"); F("GenJet_mass");
    I("GenJet_partonFlavour"); C("GenJet_hadronFlavour");
    // GenMET_* / PSWeight left unregistered on purpose: optional-branch guards.
}

struct Ev {
    UInt_t nGenPart = 0, nGenJet = 0, nGenVisTau = 0;
    std::vector<Int_t>   pdg, flg, mom, sta;
    std::vector<Float_t> pt, eta, phi, mass;
    std::vector<Float_t> jpt, jeta, jphi, jmass;
    std::vector<Int_t>   jpf;
    std::vector<UChar_t> jhf;
};

static void loadEvent(const Ev& e) {
    uintS["nGenPart"]->val   = e.nGenPart;
    uintS["nGenJet"]->val    = e.nGenJet;
    uintS["nGenVisTau"]->val = e.nGenVisTau;
    intV["GenPart_pdgId"]->v            = e.pdg;
    intV["GenPart_statusFlags"]->v      = e.flg;
    intV["GenPart_genPartIdxMother"]->v = e.mom;
    intV["GenPart_status"]->v           = e.sta;
    floatV["GenPart_pt"]->v   = e.pt;
    floatV["GenPart_eta"]->v  = e.eta;
    floatV["GenPart_phi"]->v  = e.phi;
    floatV["GenPart_mass"]->v = e.mass;
    floatV["GenJet_pt"]->v   = e.jpt;
    floatV["GenJet_eta"]->v  = e.jeta;
    floatV["GenJet_phi"]->v  = e.jphi;
    floatV["GenJet_mass"]->v = e.jmass;
    intV["GenJet_partonFlavour"]->v  = e.jpf;
    ucharV["GenJet_hadronFlavour"]->v = e.jhf;
}

int main() {
    ensureRegistered();
    TopCPVGenCategorizer cat;  // plug-in mode: default ctor, no ROOT I/O
    cat.AttachExternal(&uintS, &floatS, &floatV, &intV, &boolV, &ucharV);

    const int IHP  = TopCPVGenStatusBit::isHardProcess;
    const int FHP  = TopCPVGenStatusBit::fromHardProcess;
    const int LAST = TopCPVGenStatusBit::isLastCopy;

    // ---------------- E1: ttbar all-hadronic signal --------------------------
    Ev e1;
    e1.nGenPart = 10; e1.nGenJet = 1; e1.nGenVisTau = 0;
    e1.pdg  = {6, -6, 24, 5, -24, -5, 2, -1, 1, -2};
    e1.mom  = {-1, -1, 0, 0, 1, 1, 2, 2, 4, 4};
    e1.flg  = std::vector<Int_t>(10, LAST);
    e1.sta  = {22, 22, 22, 23, 22, 23, 23, 23, 23, 23};
    e1.pt   = {180.f, 175.f, 90.f, 60.f, 88.f, 58.f, 40.f, 35.f, 42.f, 33.f};
    e1.eta  = {0.4f, -0.3f, 0.2f, 0.5f, -0.1f, 0.6f, 0.25f, 0.15f, -0.2f, -0.05f};
    e1.phi  = {0.1f, 3.0f, 0.3f, 0.8f, 2.9f, 2.2f, 0.35f, 0.28f, 3.05f, 2.8f};
    e1.mass = {172.5f, 172.5f, 80.4f, 4.7f, 80.4f, 4.7f, 0.f, 0.f, 0.f, 0.f};
    e1.jpt = {61.f}; e1.jeta = {0.52f}; e1.jphi = {0.81f}; e1.jmass = {8.f};
    e1.jpf = {5}; e1.jhf = {5};

    std::printf("== E1 ttbar signal ==\n");
    loadEvent(e1);
    {
        const GenCatResult& r = cat.ProcessEvent();
        CHECK_EQ("isSignal",                 r.isSignal ? 1 : 0, 1);
        CHECK_EQ("GenPar_Count",             r.genPar_count, 12);
        CHECK_EQ("GenBJet_Count",            r.genBJet_count, 1);
        CHECK_EQ("Channel_Idx",              r.channel_idx, 0);
        CHECK_EQ("Channel_Idx_Final",        r.channel_idx_final, 0);
        CHECK_EQ("Channel_Lepton_Count",     r.channel_lepton, 0);
        CHECK_EQ("Channel_Jets",             r.channel_jets, 2112);
        CHECK_EQ("Channel_Jets_Abs",         r.channel_jets_abs, 1212);
        CHECK_EQ("Channel_Idx_Expanded",     r.channel_idx_expanded, 0);
    }

    // ---------------- E2: explicit-Z Z->tautau background --------------------
    Ev e2;
    e2.nGenPart = 10; e2.nGenJet = 0; e2.nGenVisTau = 2;
    e2.pdg  = {23, 15, -15, 15, -15, 13, -14, 16, 211, -16};
    e2.mom  = {-1, 0, 0, 1, 2, 3, 3, 3, 4, 4};
    e2.flg  = {IHP, IHP, IHP, LAST | FHP, LAST | FHP, 0, 0, 0, 0, 0};
    e2.sta  = {22, 23, 23, 2, 2, 1, 1, 1, 1, 1};
    e2.pt   = {50.f, 45.f, 44.f, 44.5f, 43.5f, 20.f, 12.f, 11.f, 15.f, 9.f};
    e2.eta  = std::vector<Float_t>(10, 0.1f);
    e2.phi  = std::vector<Float_t>(10, 0.5f);
    e2.mass = std::vector<Float_t>(10, 0.f);

    std::printf("== E2 Z->tautau background ==\n");
    loadEvent(e2);
    {
        const GenCatResult& r = cat.ProcessEvent();
        CHECK_EQ("isSignal",                 r.isSignal ? 1 : 0, 0);
        CHECK_EQ("GenPar_Count",             r.genPar_count, 4);
        CHECK_EQ("Channel_Idx",              r.channel_idx, -30);
        CHECK_EQ("Channel_Lepton_Count",     r.channel_lepton, 2);
        CHECK_EQ("Channel_Idx_Final",        r.channel_idx_final, 13);
        CHECK_EQ("Channel_Lepton_Final",     r.channel_lepton_final, 1);
        CHECK_EQ("Channel_Tau_Lepton",       r.channel_tau_lepton, 1);
        CHECK_EQ("Channel_Visible_Tau",      r.channel_visible_tau, 2);
        CHECK_EQ("Channel_Idx_Expanded",     r.channel_idx_expanded, -30);
        CHECK_EQ("GenPar_pdgId[0]",          r.genPar_pdgId[0], 23);
        CHECK_EQ("GenPar_pdgId[3] (app mu)", r.genPar_pdgId[3], 13);
        CHECK_EQ("GenPar_Mom1[3] (=tau)",    r.genPar_mom1Idx[3], 1);
    }

    // ---------------- E3: boson-less ME mumu background ----------------------
    Ev e3;
    e3.nGenPart = 6; e3.nGenJet = 0; e3.nGenVisTau = 0;
    e3.pdg  = {2, -2, 13, -13, 13, -13};
    e3.mom  = {-1, -1, 0, 0, 2, 3};
    e3.flg  = {IHP, IHP, IHP, IHP, LAST, LAST};
    e3.sta  = {21, 21, 23, 23, 1, 1};
    e3.pt   = {70.f, 68.f, 35.f, 33.f, 34.8f, 32.9f};
    e3.eta  = std::vector<Float_t>(6, 0.2f);
    e3.phi  = std::vector<Float_t>(6, 1.0f);
    e3.mass = std::vector<Float_t>(6, 0.f);

    std::printf("== E3 boson-less mumu background ==\n");
    loadEvent(e3);
    {
        const GenCatResult& r = cat.ProcessEvent();
        CHECK_EQ("isSignal",                 r.isSignal ? 1 : 0, 0);
        CHECK_EQ("GenPar_Count",             r.genPar_count, 4);
        CHECK_EQ("Channel_Idx",              r.channel_idx, 26);
        CHECK_EQ("Channel_Lepton_Count",     r.channel_lepton, 2);
        CHECK_EQ("Channel_Idx_Final",        r.channel_idx_final, 26);
        CHECK_EQ("Channel_Lepton_Final",     r.channel_lepton_final, 2);
        CHECK_EQ("Channel_Tau_Lepton",       r.channel_tau_lepton, 0);
        CHECK_EQ("Channel_Idx_Expanded",     r.channel_idx_expanded, 26);
    }

    if (nFail) { std::printf("\nCROSS-VALIDATION FAILED (%d)\n", nFail); return 1; }
    std::printf("\nC++ CROSS-VALIDATION PASSED — identical to the Python module\n");
    return 0;
}
