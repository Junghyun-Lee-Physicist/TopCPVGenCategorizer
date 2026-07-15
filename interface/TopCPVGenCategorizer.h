#ifndef TOPCPVGENCATEGORIZER_H
#define TOPCPVGENCATEGORIZER_H

// =============================================================================
//  TopCPVGenCategorizer
//  -----------------------------------------------------------------------------
//  NanoAOD-based replacement for the GenPar()/GenJet()/GenMET()/ghost-B
//  sections of the legacy MiniAOD SSBAnalyzer. Reconstructs a parton-level
//  family tree from the GenPart collection of a CMS NanoAOD file and produces:
//      - 12-slot family tree with channel classification
//      - GenJet and GenMET kinematics
//      - Ghost-B-jet matching (via GenJet_hadronFlavour + ΔR walk)
//      - PSWeight (ISR/FSR shower systematics; approx. replacement for
//        the MiniAOD bfragWgtProducer which is not in NanoAOD)
//
//  Framework compatibility
//  -----------------------
//  - Mirrors the Analysis class pattern of the SSB framework:
//        * TChain + TTreeReader
//        * std::unordered_map<string, unique_ptr<TTreeReaderValue/Array>>
//        * interface/ + src/ separation
//  - Does NOT depend on Analysis.h / SSBCorrections.h / SSBCPVCalc.h.
//    Selection, JEC, and lepton SF are not needed for generator-level
//    categorization, so this class is fully standalone.
//  - Branch list is HARDCODED — NanoAOD format is fixed.
//
//  Author: based on SSBAnalyzer (S. Ha, S. Lee, S. Choi)
// =============================================================================

#include <vector>
#include <array>
#include <string>
#include <memory>
#include <unordered_map>
#include <map>
#include <cstdint>

#include <TChain.h>
#include <TFile.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <TTreeReaderArray.h>
#include <TLorentzVector.h>

// -----------------------------------------------------------------------------
//  GenPart_statusFlags bit definitions (NanoAODv9 documentation)
// -----------------------------------------------------------------------------
namespace TopCPVGenStatusBit {
    constexpr int isPrompt                          = 1 << 0;
    constexpr int isDecayedLeptonHadron             = 1 << 1;
    constexpr int isTauDecayProduct                 = 1 << 2;
    constexpr int isPromptTauDecayProduct           = 1 << 3;
    constexpr int isDirectTauDecayProduct           = 1 << 4;
    constexpr int isDirectPromptTauDecayProduct     = 1 << 5;
    constexpr int isDirectHadronDecayProduct        = 1 << 6;
    constexpr int isHardProcess                     = 1 << 7;
    constexpr int fromHardProcess                   = 1 << 8;
    constexpr int isHardProcessTauDecayProduct      = 1 << 9;
    constexpr int isDirectHardProcessTauDecayProduct= 1 << 10;
    constexpr int fromHardProcessBeforeFSR          = 1 << 11;
    constexpr int isFirstCopy                       = 1 << 12;
    constexpr int isLastCopy                        = 1 << 13;
    constexpr int isLastCopyBeforeFSR               = 1 << 14;
}

// -----------------------------------------------------------------------------
//  GenCatResult — one struct per event
// -----------------------------------------------------------------------------
struct GenCatResult {
    bool isSignal = false;

    std::array<int, 12> selectedIdx{};

    std::vector<int>   genPar_idx;
    std::vector<int>   genPar_pdgId;
    std::vector<int>   genPar_status;
    std::vector<float> genPar_pt;
    std::vector<float> genPar_eta;
    std::vector<float> genPar_phi;
    std::vector<float> genPar_mass;
    std::vector<float> genPar_energy;
    std::vector<int>   genPar_mom1Idx;
    std::vector<int>   genPar_mom2Idx;
    std::vector<int>   genPar_dau1Idx;
    std::vector<int>   genPar_dau2Idx;
    std::vector<int>   genPar_nMom;
    std::vector<int>   genPar_nDau;
    int                genPar_count = 0;

    float genTop_pt    = -999.f, genTop_eta    = -999.f;
    float genTop_phi   = -999.f, genTop_energy = -999.f;
    float genAnTop_pt  = -999.f, genAnTop_eta  = -999.f;
    float genAnTop_phi = -999.f, genAnTop_energy = -999.f;

    int channel_idx           = 0;
    int channel_idx_final     = 0;
    int channel_lepton        = 0;
    int channel_lepton_final  = 0;
    int channel_jets          = 0;
    int channel_jets_abs      = 0;
    int channel_tau_lepton    = 0;
    int channel_visible_tau   = 0;
    // Additive diagnostic (v1.8, mirrors the NtupleForge module): equals
    // channel_idx for a well-formed selection, -999 when isSignal and any of
    // slots 2-11 is < 0 (the NanoAOD analogue of MiniAOD's
    // "SelectedPar.size() != 12" cerr). channel_idx itself stays
    // MiniAOD-identical; unclassifiable lives ONLY here.
    int channel_idx_expanded  = 0;

    // GenJet (mirrors SSBAnalyzer::GenJet())
    std::vector<float> genJet_pt;
    std::vector<float> genJet_eta;
    std::vector<float> genJet_phi;
    std::vector<float> genJet_mass;
    std::vector<float> genJet_energy;
    std::vector<int>   genJet_partonFlavour;
    std::vector<int>   genJet_hadronFlavour;
    int                genJet_count = 0;

    // GenMET (mirrors SSBAnalyzer::GenMET())
    float genMET_pt  = -999.f;
    float genMET_phi = -999.f;

    // Ghost B-jet / B-hadron
    std::vector<float> genBJet_pt;
    std::vector<float> genBJet_eta;
    std::vector<float> genBJet_phi;
    std::vector<float> genBJet_energy;
    std::vector<float> genBHad_pt;
    std::vector<float> genBHad_eta;
    std::vector<float> genBHad_phi;
    std::vector<float> genBHad_energy;
    std::vector<int>   genBHad_fromTopWeakDecay;
    std::vector<int>   genBHad_flavour;
    int                genBJet_count = 0;
    int                genBHad_count = 0;

    // PSWeight (4 variations from NanoAOD)
    float psWeight_ISR_up   = 1.f;
    float psWeight_FSR_up   = 1.f;
    float psWeight_ISR_down = 1.f;
    float psWeight_FSR_down = 1.f;
    int   psWeight_n        = 0;
};

// -----------------------------------------------------------------------------
//  TopCPVGenCategorizer
// -----------------------------------------------------------------------------
class TopCPVGenCategorizer {
public:
    // Mode A: standalone driver
    TopCPVGenCategorizer(TChain* chain,
                      const std::string& outputName,
                      const std::string& seDirName = "",
                      Long64_t maxEvents = -1);

    // Mode B: plug-in (no own reader)
    TopCPVGenCategorizer();

    ~TopCPVGenCategorizer();

    TopCPVGenCategorizer(const TopCPVGenCategorizer&) = delete;
    TopCPVGenCategorizer& operator=(const TopCPVGenCategorizer&) = delete;

    void AttachExternal(
        std::unordered_map<std::string, std::unique_ptr<TTreeReaderValue<UInt_t>>>*  uintSingles,
        std::unordered_map<std::string, std::unique_ptr<TTreeReaderValue<Float_t>>>* floatSingles,
        std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Float_t>>>* floatVectors,
        std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Int_t>>>*   intVectors,
        std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Bool_t>>>*  boolVectors,
        std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<UChar_t>>>* ucharVectors);

    void BookOutput();
    void Loop();
    const GenCatResult& ProcessEvent();
    const GenCatResult& Result() const { return result_; }

    bool HasTauSecondaryLepton() const;
    std::vector<int> PromptChargedLeptonsFromW() const;
    void DumpEvent(std::ostream& os) const;

private:
    bool ownReader_ = false;
    std::unique_ptr<TTreeReader> reader_;

    std::unordered_map<std::string, std::unique_ptr<TTreeReaderValue<UInt_t>>>*  uintSingles_  = nullptr;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderValue<Float_t>>>* floatSingles_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Float_t>>>* floatVectors_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Int_t>>>*   intVectors_   = nullptr;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Bool_t>>>*  boolVectors_  = nullptr;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<UChar_t>>>* ucharVectors_ = nullptr;

    std::unordered_map<std::string, std::unique_ptr<TTreeReaderValue<UInt_t>>>   ownUintSingles_;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderValue<Float_t>>>  ownFloatSingles_;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Float_t>>>  ownFloatVectors_;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Int_t>>>    ownIntVectors_;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Bool_t>>>   ownBoolVectors_;
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<UChar_t>>>  ownUcharVectors_;

    // Dedicated event-identifier readers, used only in standalone mode to write
    // a (run, luminosityBlock, event) join key into GenCatTree. This key lets an
    // external B-fragmentation weight tree (produced from MiniAOD) be attached as
    // a ROOT friend (AddFriend) and joined event-by-event. Kept separate from the
    // generic branch maps because `event` is ULong64_t, a type not used elsewhere.
    std::unique_ptr<TTreeReaderValue<UInt_t>>    evReaderRun_;
    std::unique_ptr<TTreeReaderValue<UInt_t>>    evReaderLumi_;
    std::unique_ptr<TTreeReaderValue<ULong64_t>> evReaderEvent_;
    bool haveEventId_ = false;

    void RegisterBranches();

    GenCatResult result_;
    std::vector<std::vector<int>> daughters_;
    // Selected-tau -> its FinalPar descendants (index order), collected in
    // ComputeChannelDirect and consumed by ComputeChannelFinal — the NanoAOD
    // equivalent of MiniAOD's SelParDau map (std::map => ascending tau index,
    // matching MiniAOD's iteration order).
    std::map<int, std::vector<int>> tauDesc_;
    Long64_t nUnclassifiable_ = 0;   // isSignal && incomplete 12-slot build
    std::vector<char> ReachableFrom(int start) const;

    void ClearState();
    void BuildDaughterMap();
    bool FindTopAntiTop();
    int  FindLastDaughter(int parent, int targetPdg) const;
    std::pair<int,int> WDaughters(int wIdx) const;
    void FillSignalSelection();
    void FillBackgroundSelection();
    void PushGenPar(int idx, int mom1, int mom2, int dau1, int dau2);
    void ComputeChannelDirect();
    void ComputeChannelFinal();
    void ComputeChannelJets();
    void ProcessGenJets();
    void ProcessGenMET();
    void ProcessGenBHadrons();
    void ProcessPSWeights();

    bool IsAncestorTop(int idx, int maxDepth = 100) const;
    int  FindNearestBQuark(float eta, float phi, float maxDR = 0.4f) const;

    std::string outputName_;
    std::string seDirName_;
    Long64_t    maxEvents_ = -1;
    TFile*  fout_   = nullptr;
    TTree*  outTree_ = nullptr;

    // Output buffers
    // Event identifier (join key for an external B-fragmentation weight friend
    // tree). Written in standalone mode only. NanoAOD types: run/luminosityBlock
    // are UInt_t (/i), event is ULong64_t (/l).
    UInt_t     out_run_;
    UInt_t     out_lumi_;
    ULong64_t  out_event_;

    Bool_t   out_isSignal_;
    Int_t    out_selectedIdx_[12];
    Int_t    out_genPar_count_;
    std::vector<Int_t>   out_genPar_idx_;
    std::vector<Int_t>   out_genPar_pdgId_;
    std::vector<Int_t>   out_genPar_status_;
    std::vector<Float_t> out_genPar_pt_;
    std::vector<Float_t> out_genPar_eta_;
    std::vector<Float_t> out_genPar_phi_;
    std::vector<Float_t> out_genPar_mass_;
    std::vector<Float_t> out_genPar_energy_;
    std::vector<Int_t>   out_genPar_mom1Idx_;
    std::vector<Int_t>   out_genPar_mom2Idx_;
    std::vector<Int_t>   out_genPar_dau1Idx_;
    std::vector<Int_t>   out_genPar_dau2Idx_;
    std::vector<Int_t>   out_genPar_nMom_;
    std::vector<Int_t>   out_genPar_nDau_;
    Float_t out_genTop_pt_, out_genTop_eta_, out_genTop_phi_, out_genTop_energy_;
    Float_t out_genAnTop_pt_, out_genAnTop_eta_, out_genAnTop_phi_, out_genAnTop_energy_;
    Int_t   out_channel_idx_, out_channel_idx_final_;
    Int_t   out_channel_lepton_, out_channel_lepton_final_;
    Int_t   out_channel_jets_, out_channel_jets_abs_;
    Int_t   out_channel_tau_lepton_, out_channel_visible_tau_;
    Int_t   out_channel_idx_expanded_;

    Int_t                out_genJet_count_;
    std::vector<Float_t> out_genJet_pt_;
    std::vector<Float_t> out_genJet_eta_;
    std::vector<Float_t> out_genJet_phi_;
    std::vector<Float_t> out_genJet_mass_;
    std::vector<Float_t> out_genJet_energy_;
    std::vector<Int_t>   out_genJet_partonFlavour_;
    std::vector<Int_t>   out_genJet_hadronFlavour_;

    Float_t out_genMET_pt_;
    Float_t out_genMET_phi_;

    Int_t                out_genBJet_count_;
    Int_t                out_genBHad_count_;
    std::vector<Float_t> out_genBJet_pt_;
    std::vector<Float_t> out_genBJet_eta_;
    std::vector<Float_t> out_genBJet_phi_;
    std::vector<Float_t> out_genBJet_energy_;
    std::vector<Float_t> out_genBHad_pt_;
    std::vector<Float_t> out_genBHad_eta_;
    std::vector<Float_t> out_genBHad_phi_;
    std::vector<Float_t> out_genBHad_energy_;
    std::vector<Int_t>   out_genBHad_fromTopWeakDecay_;
    std::vector<Int_t>   out_genBHad_flavour_;

    Float_t out_psWeight_ISR_up_;
    Float_t out_psWeight_FSR_up_;
    Float_t out_psWeight_ISR_down_;
    Float_t out_psWeight_FSR_down_;
    Int_t   out_psWeight_n_;

    void SyncOutputBuffers();

    const TTreeReaderValue<UInt_t>*  GetUIntSingle (const std::string& name) const;
    const TTreeReaderValue<Float_t>* GetFloatSingle(const std::string& name) const;
    const TTreeReaderArray<Float_t>* GetFloatVector(const std::string& name) const;
    const TTreeReaderArray<Int_t>*   GetIntVector  (const std::string& name) const;
    const TTreeReaderArray<Bool_t>*  GetBoolVector (const std::string& name) const;
    const TTreeReaderArray<UChar_t>* GetUCharVector(const std::string& name) const;
};

#endif // TOPCPVGENCATEGORIZER_H
