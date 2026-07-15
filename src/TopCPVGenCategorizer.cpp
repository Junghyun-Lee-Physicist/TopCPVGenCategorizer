// =============================================================================
//  TopCPVGenCategorizer.cpp
//  -----------------------------------------------------------------------------
//  Implementation of the NanoAOD-based generator-level categorizer.
//  See interface/TopCPVGenCategorizer.h for class overview.
// =============================================================================

#include "../interface/TopCPVGenCategorizer.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::pair;

// =============================================================================
//  Constructors / destructor
// =============================================================================
TopCPVGenCategorizer::TopCPVGenCategorizer(TChain* chain,
                                     const std::string& outputName,
                                     const std::string& seDirName,
                                     Long64_t maxEvents)
    : ownReader_(true),
      outputName_(outputName),
      seDirName_(seDirName),
      maxEvents_(maxEvents) {

    if (!chain) throw std::runtime_error("TopCPVGenCategorizer: null TChain");

    // Early validation: this tool only processes MC.
    {
        static const std::vector<std::string> kMustExist = {
            "nGenPart", "GenPart_pdgId", "GenPart_statusFlags",
            "GenPart_genPartIdxMother"
        };
        std::vector<std::string> missing;
        for (const auto& b : kMustExist) {
            if (!chain->GetBranch(b.c_str())) missing.push_back(b);
        }
        if (!missing.empty()) {
            std::string msg = "TopCPVGenCategorizer: required GenPart branches not found "
                              "in the input chain. This tool processes MC only. Missing: ";
            for (std::size_t i = 0; i < missing.size(); ++i) {
                msg += missing[i];
                if (i + 1 < missing.size()) msg += ", ";
            }
            throw std::runtime_error(msg);
        }
    }

    reader_ = std::make_unique<TTreeReader>(chain);

    uintSingles_  = &ownUintSingles_;
    floatSingles_ = &ownFloatSingles_;
    floatVectors_ = &ownFloatVectors_;
    intVectors_   = &ownIntVectors_;
    boolVectors_  = &ownBoolVectors_;
    ucharVectors_ = &ownUcharVectors_;

    RegisterBranches();
    BookOutput();
    ClearState();
}

TopCPVGenCategorizer::TopCPVGenCategorizer()
    : ownReader_(false) {
    ClearState();
}

TopCPVGenCategorizer::~TopCPVGenCategorizer() {
    if (fout_) {
        fout_->cd();
        if (outTree_) outTree_->Write();
        fout_->Close();
        delete fout_;
        fout_   = nullptr;
        outTree_ = nullptr;
    }
}

void TopCPVGenCategorizer::AttachExternal(
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderValue<UInt_t>>>*  uintSingles,
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderValue<Float_t>>>* floatSingles,
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Float_t>>>* floatVectors,
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Int_t>>>*   intVectors,
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<Bool_t>>>*  boolVectors,
    std::unordered_map<std::string, std::unique_ptr<TTreeReaderArray<UChar_t>>>* ucharVectors)
{
    uintSingles_  = uintSingles;
    floatSingles_ = floatSingles;
    floatVectors_ = floatVectors;
    intVectors_   = intVectors;
    boolVectors_  = boolVectors;
    ucharVectors_ = ucharVectors;
}

// =============================================================================
//  RegisterBranches
// =============================================================================
void TopCPVGenCategorizer::RegisterBranches() {

    struct BranchSpec {
        const char* name;
        char        dtype;   // 'U' UInt_t, 'F' Float_t, 'I' Int_t, 'B' Bool_t, 'C' UChar_t
        char        vtype;   // 'S' single, 'V' vector
    };

    static constexpr BranchSpec kBranches[] = {
        // GenPart core
        {"nGenPart",                 'U', 'S'},
        {"GenPart_pt",               'F', 'V'},
        {"GenPart_eta",              'F', 'V'},
        {"GenPart_phi",              'F', 'V'},
        {"GenPart_mass",             'F', 'V'},
        {"GenPart_pdgId",            'I', 'V'},
        {"GenPart_status",           'I', 'V'},
        {"GenPart_statusFlags",      'I', 'V'},
        {"GenPart_genPartIdxMother", 'I', 'V'},
        // GenVisTau
        {"nGenVisTau",                'U', 'S'},
        {"GenVisTau_pt",              'F', 'V'},
        {"GenVisTau_eta",             'F', 'V'},
        {"GenVisTau_phi",             'F', 'V'},
        {"GenVisTau_genPartIdxMother",'I', 'V'},
        // GenJet
        {"nGenJet",                   'U', 'S'},
        {"GenJet_pt",                 'F', 'V'},
        {"GenJet_eta",                'F', 'V'},
        {"GenJet_phi",                'F', 'V'},
        {"GenJet_mass",               'F', 'V'},
        {"GenJet_partonFlavour",      'I', 'V'},
        {"GenJet_hadronFlavour",      'C', 'V'},
        // GenMET
        {"GenMET_pt",                 'F', 'S'},
        {"GenMET_phi",                'F', 'S'},
        // PSWeight
        {"nPSWeight",                 'U', 'S'},
        {"PSWeight",                  'F', 'V'},
    };

    for (const auto& b : kBranches) {
        const std::string name = b.name;
        try {
            if (b.dtype == 'U' && b.vtype == 'S') {
                (*uintSingles_)[name] =
                    std::make_unique<TTreeReaderValue<UInt_t>>(*reader_, b.name);
            } else if (b.dtype == 'F' && b.vtype == 'S') {
                (*floatSingles_)[name] =
                    std::make_unique<TTreeReaderValue<Float_t>>(*reader_, b.name);
            } else if (b.dtype == 'F' && b.vtype == 'V') {
                (*floatVectors_)[name] =
                    std::make_unique<TTreeReaderArray<Float_t>>(*reader_, b.name);
            } else if (b.dtype == 'I' && b.vtype == 'V') {
                (*intVectors_)[name] =
                    std::make_unique<TTreeReaderArray<Int_t>>(*reader_, b.name);
            } else if (b.dtype == 'B' && b.vtype == 'V') {
                (*boolVectors_)[name] =
                    std::make_unique<TTreeReaderArray<Bool_t>>(*reader_, b.name);
            } else if (b.dtype == 'C' && b.vtype == 'V') {
                (*ucharVectors_)[name] =
                    std::make_unique<TTreeReaderArray<UChar_t>>(*reader_, b.name);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("TopCPVGenCategorizer: failed to register branch '") +
                b.name + "' — is this a Data sample? "
                "This tool only processes MC. (" + e.what() + ")");
        }
    }

    cout << "[TopCPVGenCategorizer] registered "
         << uintSingles_->size() << " UInt singles + "
         << floatSingles_->size() << " Float singles + "
         << (floatVectors_->size() + intVectors_->size() +
             boolVectors_->size() + ucharVectors_->size())
         << " vector branches" << endl;

    // Event-identifier join key (run, luminosityBlock, event). Optional: if the
    // input lacks these (non-standard ntuple), GenCatTree is still written but
    // without the friend-tree key, and a warning is emitted.
    if (reader_) {
        TTree* t = reader_->GetTree();
        const bool hasRun   = t && t->GetBranch("run");
        const bool hasLumi  = t && t->GetBranch("luminosityBlock");
        const bool hasEvent = t && t->GetBranch("event");
        if (hasRun && hasLumi && hasEvent) {
            evReaderRun_   = std::make_unique<TTreeReaderValue<UInt_t>>(*reader_, "run");
            evReaderLumi_  = std::make_unique<TTreeReaderValue<UInt_t>>(*reader_, "luminosityBlock");
            evReaderEvent_ = std::make_unique<TTreeReaderValue<ULong64_t>>(*reader_, "event");
            haveEventId_ = true;
            cout << "[TopCPVGenCategorizer] event-id join key enabled "
                    "(run, luminosityBlock, event)" << endl;
        } else {
            haveEventId_ = false;
            cout << "[TopCPVGenCategorizer] WARNING: run/luminosityBlock/event not all "
                    "present; GenCatTree will be written without a B-frag friend key"
                 << endl;
        }
    }
}

// =============================================================================
//  BookOutput
// =============================================================================
void TopCPVGenCategorizer::BookOutput() {

    if (outputName_.empty()) return;

    string fullPath = seDirName_.empty()
        ? outputName_
        : (seDirName_ + "/" + outputName_);

    fout_ = TFile::Open(fullPath.c_str(), "RECREATE");
    if (!fout_ || fout_->IsZombie())
        throw std::runtime_error("TopCPVGenCategorizer: cannot open output " + fullPath);

    outTree_ = new TTree("GenCatTree", "Parton-level family tree (NanoAOD GenPar replacement)");

    // Event identifier — written first so it can serve as the join key for an
    // external B-fragmentation weight friend tree. Only meaningful if the input
    // provided run/luminosityBlock/event (haveEventId_).
    outTree_->Branch("run",             &out_run_,   "run/i");
    outTree_->Branch("luminosityBlock", &out_lumi_,  "luminosityBlock/i");
    outTree_->Branch("event",           &out_event_, "event/l");

    outTree_->Branch("isSignal",          &out_isSignal_,         "isSignal/O");
    outTree_->Branch("SelectedIdx",        out_selectedIdx_,      "SelectedIdx[12]/I");

    outTree_->Branch("GenPar_Count",      &out_genPar_count_,     "GenPar_Count/I");
    outTree_->Branch("GenPar_Idx",        &out_genPar_idx_);
    outTree_->Branch("GenPar_pdgId",      &out_genPar_pdgId_);
    outTree_->Branch("GenPar_Status",     &out_genPar_status_);
    outTree_->Branch("GenPar_pt",         &out_genPar_pt_);
    outTree_->Branch("GenPar_eta",        &out_genPar_eta_);
    outTree_->Branch("GenPar_phi",        &out_genPar_phi_);
    outTree_->Branch("GenPar_mass",       &out_genPar_mass_);
    outTree_->Branch("GenPar_energy",     &out_genPar_energy_);
    outTree_->Branch("GenPar_Mom1_Idx",   &out_genPar_mom1Idx_);
    outTree_->Branch("GenPar_Mom2_Idx",   &out_genPar_mom2Idx_);
    outTree_->Branch("GenPar_Dau1_Idx",   &out_genPar_dau1Idx_);
    outTree_->Branch("GenPar_Dau2_Idx",   &out_genPar_dau2Idx_);
    outTree_->Branch("GenPar_Mom_Counter",&out_genPar_nMom_);
    outTree_->Branch("GenPar_Dau_Counter",&out_genPar_nDau_);

    outTree_->Branch("GenTop_pt",     &out_genTop_pt_,     "GenTop_pt/F");
    outTree_->Branch("GenTop_eta",    &out_genTop_eta_,    "GenTop_eta/F");
    outTree_->Branch("GenTop_phi",    &out_genTop_phi_,    "GenTop_phi/F");
    outTree_->Branch("GenTop_energy", &out_genTop_energy_, "GenTop_energy/F");
    outTree_->Branch("GenAnTop_pt",   &out_genAnTop_pt_,   "GenAnTop_pt/F");
    outTree_->Branch("GenAnTop_eta",  &out_genAnTop_eta_,  "GenAnTop_eta/F");
    outTree_->Branch("GenAnTop_phi",  &out_genAnTop_phi_,  "GenAnTop_phi/F");
    outTree_->Branch("GenAnTop_energy", &out_genAnTop_energy_, "GenAnTop_energy/F");

    outTree_->Branch("Channel_Idx",                &out_channel_idx_,          "Channel_Idx/I");
    outTree_->Branch("Channel_Idx_Final",          &out_channel_idx_final_,    "Channel_Idx_Final/I");
    outTree_->Branch("Channel_Lepton_Count",       &out_channel_lepton_,       "Channel_Lepton_Count/I");
    outTree_->Branch("Channel_Lepton_Count_Final", &out_channel_lepton_final_, "Channel_Lepton_Count_Final/I");
    outTree_->Branch("Channel_Jets",               &out_channel_jets_,         "Channel_Jets/I");
    outTree_->Branch("Channel_Jets_Abs",           &out_channel_jets_abs_,     "Channel_Jets_Abs/I");
    outTree_->Branch("Channel_Tau_Lepton",         &out_channel_tau_lepton_,   "Channel_Tau_Lepton/I");
    outTree_->Branch("Channel_Visible_Tau",        &out_channel_visible_tau_,  "Channel_Visible_Tau/I");
    outTree_->Branch("Channel_Idx_Expanded",       &out_channel_idx_expanded_, "Channel_Idx_Expanded/I");

    outTree_->Branch("GenJet_Count",         &out_genJet_count_,         "GenJet_Count/I");
    outTree_->Branch("GenJet_pt",            &out_genJet_pt_);
    outTree_->Branch("GenJet_eta",           &out_genJet_eta_);
    outTree_->Branch("GenJet_phi",           &out_genJet_phi_);
    outTree_->Branch("GenJet_mass",          &out_genJet_mass_);
    outTree_->Branch("GenJet_energy",        &out_genJet_energy_);
    outTree_->Branch("GenJet_PartonFlavour", &out_genJet_partonFlavour_);
    outTree_->Branch("GenJet_HadronFlavour", &out_genJet_hadronFlavour_);

    outTree_->Branch("GenMET_pt",  &out_genMET_pt_,  "GenMET_pt/F");
    outTree_->Branch("GenMET_phi", &out_genMET_phi_, "GenMET_phi/F");

    outTree_->Branch("GenBJet_Count",  &out_genBJet_count_, "GenBJet_Count/I");
    outTree_->Branch("GenBHad_Count",  &out_genBHad_count_, "GenBHad_Count/I");
    outTree_->Branch("GenBJet_pt",     &out_genBJet_pt_);
    outTree_->Branch("GenBJet_eta",    &out_genBJet_eta_);
    outTree_->Branch("GenBJet_phi",    &out_genBJet_phi_);
    outTree_->Branch("GenBJet_energy", &out_genBJet_energy_);
    outTree_->Branch("GenBHad_pt",     &out_genBHad_pt_);
    outTree_->Branch("GenBHad_eta",    &out_genBHad_eta_);
    outTree_->Branch("GenBHad_phi",    &out_genBHad_phi_);
    outTree_->Branch("GenBHad_energy", &out_genBHad_energy_);
    outTree_->Branch("GenBHad_FromTopWeakDecay", &out_genBHad_fromTopWeakDecay_);
    outTree_->Branch("GenBHad_Flavour",          &out_genBHad_flavour_);

    outTree_->Branch("PSWeight_n",        &out_psWeight_n_,        "PSWeight_n/I");
    outTree_->Branch("PSWeight_ISR_Up",   &out_psWeight_ISR_up_,   "PSWeight_ISR_Up/F");
    outTree_->Branch("PSWeight_FSR_Up",   &out_psWeight_FSR_up_,   "PSWeight_FSR_Up/F");
    outTree_->Branch("PSWeight_ISR_Down", &out_psWeight_ISR_down_, "PSWeight_ISR_Down/F");
    outTree_->Branch("PSWeight_FSR_Down", &out_psWeight_FSR_down_, "PSWeight_FSR_Down/F");
}

// =============================================================================
//  Loop
// =============================================================================
void TopCPVGenCategorizer::Loop() {

    if (!ownReader_)
        throw std::runtime_error("TopCPVGenCategorizer::Loop called in plug-in mode");

    Long64_t nProcessed = 0;
    Long64_t nSignal    = 0;
    std::unordered_map<int, Long64_t> channelHist;

    while (reader_->Next()) {
        if (maxEvents_ > 0 && nProcessed >= maxEvents_) break;
        ++nProcessed;

        const GenCatResult& r = ProcessEvent();
        if (r.isSignal) ++nSignal;
        ++channelHist[r.channel_idx];

        if (outTree_) outTree_->Fill();

        if (nProcessed % 50000 == 0)
            cout << "  processed " << nProcessed << " events" << endl;
    }

    cout << "\n========================================\n";
    cout << "TopCPVGenCategorizer summary\n";
    cout << "  total events  : " << nProcessed << "\n";
    cout << "  signal (ttbar): " << nSignal
         << "  (" << (nProcessed ? (100.0 * nSignal / nProcessed) : 0.0) << "%)\n";
    cout << "  Channel_Idx distribution:\n";
    cout << "    0   all-hadronic     : " << channelHist[0]  << "\n";
    cout << "    11  l+jets (e)       : " << channelHist[11] << "\n";
    cout << "    13  l+jets (mu)      : " << channelHist[13] << "\n";
    cout << "    22  dilepton ee      : " << channelHist[22] << "\n";
    cout << "    24  dilepton emu     : " << channelHist[24] << "\n";
    cout << "    26  dilepton mumu    : " << channelHist[26] << "\n";
    Long64_t nTau = 0;
    for (auto& kv : channelHist) if (kv.first < 0) nTau += kv.second;
    cout << "    <0  tau-involved     : " << nTau << "\n";
    cout << "  unclassifiable (Channel_Idx_Expanded==-999): " << nUnclassifiable_
         << "  (" << (nSignal ? (100.0 * nUnclassifiable_ / nSignal) : 0.0) << "% of signal)\n";
    cout << "========================================\n";
}

// =============================================================================
//  ProcessEvent
// =============================================================================
const GenCatResult& TopCPVGenCategorizer::ProcessEvent() {

    ClearState();
    BuildDaughterMap();
    result_.isSignal = FindTopAntiTop();

    if (result_.isSignal)  FillSignalSelection();
    else                   FillBackgroundSelection();

    ComputeChannelDirect();
    ComputeChannelFinal();
    ComputeChannelJets();

    // Additive diagnostic: -999 when isSignal and the 12-slot build is
    // incomplete (NanoAOD analogue of MiniAOD's "SelectedPar.size() != 12"
    // cerr). Channel_Idx itself stays MiniAOD-identical.
    bool slotMissing = false;
    if (result_.isSignal)
        for (int sIdx = 2; sIdx < 12; ++sIdx)
            if (result_.selectedIdx[sIdx] < 0) { slotMissing = true; break; }
    if (result_.isSignal && slotMissing) {
        result_.channel_idx_expanded = -999;
        ++nUnclassifiable_;
    } else {
        result_.channel_idx_expanded = result_.channel_idx;
    }

    ProcessGenJets();
    ProcessGenMET();
    ProcessGenBHadrons();
    ProcessPSWeights();

    if (outTree_) SyncOutputBuffers();
    return result_;
}

// =============================================================================
//  Branch-access helpers
// =============================================================================
const TTreeReaderValue<UInt_t>* TopCPVGenCategorizer::GetUIntSingle(const std::string& name) const {
    if (!uintSingles_) return nullptr;
    auto it = uintSingles_->find(name);
    return (it == uintSingles_->end()) ? nullptr : it->second.get();
}
const TTreeReaderValue<Float_t>* TopCPVGenCategorizer::GetFloatSingle(const std::string& name) const {
    if (!floatSingles_) return nullptr;
    auto it = floatSingles_->find(name);
    return (it == floatSingles_->end()) ? nullptr : it->second.get();
}
const TTreeReaderArray<Float_t>* TopCPVGenCategorizer::GetFloatVector(const std::string& name) const {
    if (!floatVectors_) return nullptr;
    auto it = floatVectors_->find(name);
    return (it == floatVectors_->end()) ? nullptr : it->second.get();
}
const TTreeReaderArray<Int_t>* TopCPVGenCategorizer::GetIntVector(const std::string& name) const {
    if (!intVectors_) return nullptr;
    auto it = intVectors_->find(name);
    return (it == intVectors_->end()) ? nullptr : it->second.get();
}
const TTreeReaderArray<Bool_t>* TopCPVGenCategorizer::GetBoolVector(const std::string& name) const {
    if (!boolVectors_) return nullptr;
    auto it = boolVectors_->find(name);
    return (it == boolVectors_->end()) ? nullptr : it->second.get();
}
const TTreeReaderArray<UChar_t>* TopCPVGenCategorizer::GetUCharVector(const std::string& name) const {
    if (!ucharVectors_) return nullptr;
    auto it = ucharVectors_->find(name);
    return (it == ucharVectors_->end()) ? nullptr : it->second.get();
}

// =============================================================================
//  BuildDaughterMap
// =============================================================================
void TopCPVGenCategorizer::BuildDaughterMap() {

    auto nGenPart = GetUIntSingle("nGenPart");
    auto momArr   = GetIntVector ("GenPart_genPartIdxMother");
    if (!nGenPart || !momArr) {
        throw std::runtime_error(
            "TopCPVGenCategorizer::BuildDaughterMap: required GenPart branches missing. "
            "This tool only processes MC samples.");
    }

    const UInt_t n = **const_cast<TTreeReaderValue<UInt_t>*>(nGenPart);
    daughters_.assign(n, {});

    auto& momArrRef = *const_cast<TTreeReaderArray<Int_t>*>(momArr);
    for (UInt_t i = 0; i < n; ++i) {
        const Int_t mom = momArrRef[i];
        if (mom >= 0 && static_cast<UInt_t>(mom) < n)
            daughters_[mom].push_back(static_cast<int>(i));
    }
}

// =============================================================================
//  FindTopAntiTop
// =============================================================================
bool TopCPVGenCategorizer::FindTopAntiTop() {

    auto nGenPart = GetUIntSingle("nGenPart");
    auto pdgArr   = GetIntVector ("GenPart_pdgId");
    auto flgArr   = GetIntVector ("GenPart_statusFlags");
    auto ptArr    = GetFloatVector("GenPart_pt");
    auto etaArr   = GetFloatVector("GenPart_eta");
    auto phiArr   = GetFloatVector("GenPart_phi");
    auto massArr  = GetFloatVector("GenPart_mass");

    if (!nGenPart || !pdgArr || !flgArr) return false;

    auto& pdgs  = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);
    auto& flgs  = *const_cast<TTreeReaderArray<Int_t>*>(flgArr);
    const UInt_t n = **const_cast<TTreeReaderValue<UInt_t>*>(nGenPart);

    int tIdx = -1, tbarIdx = -1;
    for (UInt_t i = 0; i < n; ++i) {
        if (!(flgs[i] & TopCPVGenStatusBit::isLastCopy)) continue;
        const int pdg = pdgs[i];
        if (pdg == 6  && tIdx    < 0) tIdx    = static_cast<int>(i);
        if (pdg == -6 && tbarIdx < 0) tbarIdx = static_cast<int>(i);
        if (tIdx >= 0 && tbarIdx >= 0) break;
    }

    result_.selectedIdx[2] = tIdx;
    result_.selectedIdx[3] = tbarIdx;

    auto fillKin = [&](int idx, float& pt, float& eta, float& phi, float& energy) {
        if (idx < 0 || !ptArr || !etaArr || !phiArr || !massArr) return;
        auto& pts   = *const_cast<TTreeReaderArray<Float_t>*>(ptArr);
        auto& etas  = *const_cast<TTreeReaderArray<Float_t>*>(etaArr);
        auto& phis  = *const_cast<TTreeReaderArray<Float_t>*>(phiArr);
        auto& mass  = *const_cast<TTreeReaderArray<Float_t>*>(massArr);
        pt  = pts[idx];
        eta = etas[idx];
        phi = phis[idx];
        const float ch = std::cosh(eta);
        energy = std::sqrt(pt*pt*ch*ch + mass[idx]*mass[idx]);
    };
    fillKin(tIdx,    result_.genTop_pt,   result_.genTop_eta,
                     result_.genTop_phi,  result_.genTop_energy);
    fillKin(tbarIdx, result_.genAnTop_pt, result_.genAnTop_eta,
                     result_.genAnTop_phi, result_.genAnTop_energy);

    return (tIdx >= 0 && tbarIdx >= 0);
}

// =============================================================================
//  FindLastDaughter
// =============================================================================
int TopCPVGenCategorizer::FindLastDaughter(int parent, int targetPdg) const {

    if (parent < 0 || parent >= static_cast<int>(daughters_.size())) return -1;
    auto pdgArr = GetIntVector("GenPart_pdgId");
    if (!pdgArr) return -1;
    auto& pdgs = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);

    int hit = -1;
    for (int d : daughters_[parent])
        if (pdgs[d] == targetPdg) { hit = d; break; }
    if (hit < 0) return -1;

    int cur = hit;
    while (true) {
        int next = -1;
        for (int dd : daughters_[cur])
            if (pdgs[dd] == targetPdg) { next = dd; break; }
        if (next < 0) return cur;
        cur = next;
    }
}

// =============================================================================
//  WDaughters
// =============================================================================
pair<int,int> TopCPVGenCategorizer::WDaughters(int wIdx) const {

    if (wIdx < 0 || wIdx >= static_cast<int>(daughters_.size())) return {-1, -1};
    auto pdgArr = GetIntVector("GenPart_pdgId");
    if (!pdgArr) return {-1, -1};
    auto& pdgs = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);

    int d1 = -1, d2 = -1;
    for (int d : daughters_[wIdx]) {
        if (std::abs(pdgs[d]) == 24) continue;
        if      (d1 < 0) d1 = d;
        else if (d2 < 0) { d2 = d; break; }
    }
    return {d1, d2};
}

// =============================================================================
//  FillSignalSelection
// =============================================================================
void TopCPVGenCategorizer::FillSignalSelection() {

    const int tIdx    = result_.selectedIdx[2];
    const int tbarIdx = result_.selectedIdx[3];

    const int Wp_idx   = FindLastDaughter(tIdx,    24);
    const int b_idx    = FindLastDaughter(tIdx,     5);
    const int Wm_idx   = FindLastDaughter(tbarIdx, -24);
    const int bbar_idx = FindLastDaughter(tbarIdx, -5);

    result_.selectedIdx[4] = Wp_idx;
    result_.selectedIdx[5] = b_idx;
    result_.selectedIdx[6] = Wm_idx;
    result_.selectedIdx[7] = bbar_idx;

    auto [Wp_d1, Wp_d2] = WDaughters(Wp_idx);
    auto [Wm_d1, Wm_d2] = WDaughters(Wm_idx);
    result_.selectedIdx[8]  = Wp_d1;
    result_.selectedIdx[9]  = Wp_d2;
    result_.selectedIdx[10] = Wm_d1;
    result_.selectedIdx[11] = Wm_d2;

    result_.selectedIdx[0] = -1;
    result_.selectedIdx[1] = -1;

    static constexpr int parentSlot[12] = {-1,-1,-1,-1, 2,2,3,3, 4,4,6,6};
    static constexpr int dau1Slot[12]   = {-1,-1, 4, 6, 8,-1,10,-1, -1,-1,-1,-1};
    static constexpr int dau2Slot[12]   = {-1,-1, 5, 7, 9,-1,11,-1, -1,-1,-1,-1};

    for (int slot = 0; slot < 12; ++slot) {
        const int idx = result_.selectedIdx[slot];
        const int momIdx  = (parentSlot[slot] >= 0) ? result_.selectedIdx[parentSlot[slot]] : -1;
        const int dau1Idx = (dau1Slot[slot]   >= 0) ? result_.selectedIdx[dau1Slot[slot]]   : -1;
        const int dau2Idx = (dau2Slot[slot]   >= 0) ? result_.selectedIdx[dau2Slot[slot]]   : -1;
        PushGenPar(idx, momIdx, -1, dau1Idx, dau2Idx);
    }
}

// =============================================================================
//  FillBackgroundSelection
// =============================================================================
void TopCPVGenCategorizer::FillBackgroundSelection() {

    auto nGenPart = GetUIntSingle("nGenPart");
    auto pdgArr   = GetIntVector ("GenPart_pdgId");
    auto flgArr   = GetIntVector ("GenPart_statusFlags");
    auto momArr   = GetIntVector ("GenPart_genPartIdxMother");
    if (!nGenPart || !pdgArr || !flgArr || !momArr) return;

    auto& pdgs = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);
    auto& flgs = *const_cast<TTreeReaderArray<Int_t>*>(flgArr);
    auto& moms = *const_cast<TTreeReaderArray<Int_t>*>(momArr);
    const UInt_t n = **const_cast<TTreeReaderValue<UInt_t>*>(nGenPart);

    auto statArr = GetIntVector("GenPart_status");
    if (!statArr) return;
    auto& stats = *const_cast<TTreeReaderArray<Int_t>*>(statArr);

    // MiniAOD SSBAnalyzer background SelectedPar (03_miniaod_origin.md Sec.1.6):
    //   beam protons (0,1) + EVERY status-21..23 particle (the whole hard
    //   process, non-boson partons included) + status-1/2 leptons whose DIRECT
    //   mother is a top/Z/W/H.
    // NanoAOD translation (v1.8, synchronized with the NtupleForge module):
    //   * protons are pruned from NanoAOD -> unrecoverable, no rows (unlike the
    //     signal branch there is no fixed 12-slot layout to preserve);
    //   * "status 21-23" == statusFlags isHardProcess (copy-specific bit;
    //     hadronizer-independent, so the HERWIG branch collapses too);
    //   * boson-daughter finals are appended AFTER the base set, both scanned
    //     in ascending GenPart index — matching MiniAOD's TreePar-then-moved-
    //     FinalPar ordering.
    // This replaces the pre-v1.8 heuristic (last-copy bosons + one-level
    // daughters + a hard-process-tau rescue), which (a) double-counted taus
    // when both the hard-process and last-copy tau survive NanoAOD pruning and
    // (b) missed e/mu in records without an explicit boson row. See NtupleForge
    // docs/TopCPV/02_faithfulness_vs_miniaod.md Sec.2b.
    vector<int> picked;
    picked.reserve(24);

    for (UInt_t i = 0; i < n; ++i)
        if (flgs[i] & TopCPVGenStatusBit::isHardProcess)
            picked.push_back(static_cast<int>(i));

    for (UInt_t i = 0; i < n; ++i) {
        const int st = stats[i];
        if (st != 1 && st != 2) continue;
        const int a = std::abs(pdgs[i]);
        if (a < 11 || a > 16) continue;                       // FinalPar: l/nu only
        const int m = moms[i];
        if (m < 0 || static_cast<UInt_t>(m) >= n) continue;
        const int am = std::abs(pdgs[m]);
        if (am == 6 || am == 23 || am == 24 || am == 25)      // direct boson mother
            picked.push_back(static_cast<int>(i));
    }

    for (int idx : picked) {
        const int momIdx = moms[idx];
        int dau1 = -1, dau2 = -1;
        if (idx >= 0 && idx < static_cast<int>(daughters_.size())) {
            if (daughters_[idx].size() > 0) dau1 = daughters_[idx][0];
            if (daughters_[idx].size() > 1) dau2 = daughters_[idx][1];
        }
        PushGenPar(idx, momIdx, -1, dau1, dau2);
    }
}

// =============================================================================
//  PushGenPar
// =============================================================================
void TopCPVGenCategorizer::PushGenPar(int idx, int mom1, int mom2, int dau1, int dau2) {

    auto pdgArr  = GetIntVector  ("GenPart_pdgId");
    auto statArr = GetIntVector  ("GenPart_status");
    auto ptArr   = GetFloatVector("GenPart_pt");
    auto etaArr  = GetFloatVector("GenPart_eta");
    auto phiArr  = GetFloatVector("GenPart_phi");
    auto massArr = GetFloatVector("GenPart_mass");

    if (idx < 0) {
        result_.genPar_idx.push_back(-1);
        result_.genPar_pdgId.push_back(0);
        result_.genPar_status.push_back(0);
        result_.genPar_pt.push_back(-999.f);
        result_.genPar_eta.push_back(-999.f);
        result_.genPar_phi.push_back(-999.f);
        result_.genPar_mass.push_back(-999.f);
        result_.genPar_energy.push_back(-999.f);
    } else {
        const float pt   = (*const_cast<TTreeReaderArray<Float_t>*>(ptArr))[idx];
        const float eta  = (*const_cast<TTreeReaderArray<Float_t>*>(etaArr))[idx];
        const float phi  = (*const_cast<TTreeReaderArray<Float_t>*>(phiArr))[idx];
        const float mass = (*const_cast<TTreeReaderArray<Float_t>*>(massArr))[idx];
        const float ch     = std::cosh(eta);
        const float energy = std::sqrt(pt * pt * ch * ch + mass * mass);

        result_.genPar_idx.push_back(idx);
        result_.genPar_pdgId.push_back((*const_cast<TTreeReaderArray<Int_t>*>(pdgArr))[idx]);
        result_.genPar_status.push_back((*const_cast<TTreeReaderArray<Int_t>*>(statArr))[idx]);
        result_.genPar_pt.push_back(pt);
        result_.genPar_eta.push_back(eta);
        result_.genPar_phi.push_back(phi);
        result_.genPar_mass.push_back(mass);
        result_.genPar_energy.push_back(energy);
    }

    int nMo = 2, nDa = 2;
    if (mom1 < 0) {
        --nMo;
        if (mom2 < 0) --nMo;
        else mom1 = mom2;
    } else if (mom2 < 0) {
        --nMo; mom2 = mom1;
    } else if (mom1 == mom2) {
        --nMo;
    }
    if (dau1 < 0) {
        --nDa;
        if (dau2 < 0) --nDa;
        else dau1 = dau2;
    } else if (dau2 < 0) {
        --nDa; dau2 = dau1;
    } else if (dau1 == dau2) {
        --nDa;
    }

    result_.genPar_mom1Idx.push_back(mom1);
    result_.genPar_mom2Idx.push_back(mom2);
    result_.genPar_dau1Idx.push_back(dau1);
    result_.genPar_dau2Idx.push_back(dau2);
    result_.genPar_nMom.push_back(nMo);
    result_.genPar_nDau.push_back(nDa);
    ++result_.genPar_count;
}

// =============================================================================
//  ComputeChannelDirect
// =============================================================================
void TopCPVGenCategorizer::ComputeChannelDirect() {

    // MiniAOD Sec.2.1: the lepton sum runs over the FULL selected-particle list
    // (the pushed GenPar rows ARE SelectedPar: signal -> the 12 slots, of which
    // only 8-11 can hold leptons; background -> the picked list). Background is
    // therefore NOT forced to 0 any more (pre-v1.8 regression removed;
    // synchronized with the NtupleForge module).
    tauDesc_.clear();
    auto pdgArr  = GetIntVector("GenPart_pdgId");
    auto statArr = GetIntVector("GenPart_status");
    if (!pdgArr || !statArr) return;
    auto& pdgs  = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);
    auto& stats = *const_cast<TTreeReaderArray<Int_t>*>(statArr);
    const int n = static_cast<int>(daughters_.size());

    // FinalPar (MiniAOD Sec.1.1): status 1/2, 11 <= |pdg| <= 16, ascending index.
    std::vector<int> finalPar;
    finalPar.reserve(16);
    for (int i = 0; i < n; ++i) {
        const int st = stats[i];
        if (st != 1 && st != 2) continue;
        const int a = std::abs(pdgs[i]);
        if (a > 10 && a < 17) finalPar.push_back(i);
    }

    int chIdx = 0, chLep = 0;
    for (const int idx : result_.genPar_idx) {
        if (idx < 0) continue;                 // placeholder slots carry no lepton
        const int a = std::abs(pdgs[idx]);
        if (a == 11 || a == 13 || a == 15) {
            ++chLep;
            chIdx += (a == 15) ? -a : a;       // tau negated (MiniAOD convention)
            if (a == 15) {                     // collect its FinalPar descendants
                const std::vector<char> seen = ReachableFrom(idx);
                std::vector<int>& v = tauDesc_[idx];
                for (const int f : finalPar)
                    if (seen[f]) v.push_back(f);
            }
        }
    }
    result_.channel_idx    = chIdx;
    result_.channel_lepton = chLep;
}

// =============================================================================
//  ComputeChannelFinal
// =============================================================================
void TopCPVGenCategorizer::ComputeChannelFinal() {

    // MiniAOD Sec.2.2 (v1.8, replaces the GenDressedLepton shortcut): start from
    // the DIRECT channel and walk each selected tau down the gen daughter map.
    // The first non-same-pdg descendant removes the tau from the count (this
    // includes neutrinos -> a hadronic tau is removed but not replaced); a
    // charged-lepton daughter is appended to GenPar (FillGenPar equivalent) and
    // added to the count with the MiniAOD <14/>14 sign rules. Intermediate
    // same-pdg tau copies enter the selected set but neither fill GenPar nor
    // touch the channel. std::map iteration == ascending tau index; per-tau
    // descendant order == FinalPar order == ascending GenPart index — both
    // match MiniAOD, so the GenPar appends are order-exact.
    // GenDressedLepton is no longer read: it is FSR-dressed and carries an
    // implicit pt/acceptance floor the MiniAOD gen-tree walk never had.
    int chIdxF   = result_.channel_idx;
    int chLepF   = result_.channel_lepton;
    int chTauLep = 0;

    auto pdgArr = GetIntVector("GenPart_pdgId");
    if (pdgArr) {
        auto& pdgs = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);

        std::unordered_map<int, char> selected;   // grows like MiniAOD's SelectedPar
        selected.reserve(result_.genPar_idx.size() * 2);
        for (const int idx : result_.genPar_idx)
            if (idx >= 0) selected[idx] = 1;

        for (const auto& kv : tauDesc_) {                    // ascending tau index
            const int tauIdx = kv.first;
            const int momPdg = std::abs(pdgs[tauIdx]);       // 15
            int momFlag = 0;
            for (const int dau : kv.second) {                // ascending index
                if (selected.count(dau)) continue;
                selected[dau] = 1;
                const int dauPdg = std::abs(pdgs[dau]);
                if (dauPdg == momPdg) continue;              // intermediate tau copy
                if (momFlag == 0) {                          // remove the tau once
                    momFlag = 1;
                    --chLepF;
                    if (momPdg < 14) chIdxF -= momPdg;
                    else             chIdxF += momPdg;
                }
                if (dauPdg == 11 || dauPdg == 13 || dauPdg == 15) {
                    PushGenPar(dau, tauIdx, -1, -1, -1);     // MiniAOD FillGenPar append
                    ++chLepF;
                    if (dauPdg < 14) { chIdxF += dauPdg; ++chTauLep; }  // tau -> e/mu
                    else             { chIdxF -= dauPdg; }
                }
            }
        }
    }

    result_.channel_idx_final    = chIdxF;
    result_.channel_lepton_final = chLepF;
    result_.channel_tau_lepton   = chTauLep;

    auto nVT = GetUIntSingle("nGenVisTau");
    result_.channel_visible_tau = nVT
        ? static_cast<int>(**const_cast<TTreeReaderValue<UInt_t>*>(nVT))
        : 0;
}

// =============================================================================
//  ComputeChannelJets
// =============================================================================
void TopCPVGenCategorizer::ComputeChannelJets() {

    result_.channel_jets     = 0;
    result_.channel_jets_abs = 0;
    if (!result_.isSignal) return;

    auto pdgArr = GetIntVector("GenPart_pdgId");
    if (!pdgArr) return;
    auto& pdgs = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);

    int chJets = 0;
    const int wChargeArr[2] = { +1, -1 };
    for (int k = 0; k < 2; ++k) {
        const int d1 = (k == 0) ? result_.selectedIdx[8]  : result_.selectedIdx[10];
        const int d2 = (k == 0) ? result_.selectedIdx[9]  : result_.selectedIdx[11];
        if (d1 < 0 || d2 < 0) continue;

        const int p1 = std::abs(pdgs[d1]);
        const int p2 = std::abs(pdgs[d2]);
        if (p1 >= 10 || p2 >= 10) continue;

        int code;
        if (wChargeArr[k] > 0)
            code = (p1 % 2 == 0) ? (10 * p1 + p2) : (p1 + 10 * p2);
        else
            code = (p1 % 2 == 1) ? (10 * p1 + p2) : (p1 + 10 * p2);

        chJets = (chJets == 0) ? code : (100 * chJets + code);
    }
    result_.channel_jets = chJets;

    int absCj = chJets;
    if (absCj > 0) {
        if ((absCj / 10) % 10 > absCj % 10)
            absCj = 100 * (absCj / 100) + 10 * (absCj % 10) + (absCj / 10) % 10;
        if (absCj / 1000 > (absCj / 100) % 10)
            absCj = 1000 * ((absCj / 100) % 10) + 100 * (absCj / 1000) + (absCj % 100);
        if (absCj / 100 > absCj % 100)
            absCj = 100 * (absCj % 100) + absCj / 100;
    }
    result_.channel_jets_abs = absCj;
}

// =============================================================================
//  ProcessGenJets — mirrors SSBAnalyzer::GenJet()
// =============================================================================
void TopCPVGenCategorizer::ProcessGenJets() {

    auto nGJ      = GetUIntSingle ("nGenJet");
    auto ptArr    = GetFloatVector("GenJet_pt");
    auto etaArr   = GetFloatVector("GenJet_eta");
    auto phiArr   = GetFloatVector("GenJet_phi");
    auto massArr  = GetFloatVector("GenJet_mass");
    auto pflavArr = GetIntVector  ("GenJet_partonFlavour");
    auto hflavArr = GetUCharVector("GenJet_hadronFlavour");

    if (!nGJ || !ptArr || !etaArr || !phiArr || !massArr) return;

    const UInt_t n = **const_cast<TTreeReaderValue<UInt_t>*>(nGJ);
    auto& pts  = *const_cast<TTreeReaderArray<Float_t>*>(ptArr);
    auto& etas = *const_cast<TTreeReaderArray<Float_t>*>(etaArr);
    auto& phis = *const_cast<TTreeReaderArray<Float_t>*>(phiArr);
    auto& mass = *const_cast<TTreeReaderArray<Float_t>*>(massArr);

    for (UInt_t i = 0; i < n; ++i) {
        const float pt   = pts[i];
        const float eta  = etas[i];
        const float phi  = phis[i];
        const float m    = mass[i];
        const float ch   = std::cosh(eta);
        const float energy = std::sqrt(pt*pt*ch*ch + m*m);

        result_.genJet_pt.push_back(pt);
        result_.genJet_eta.push_back(eta);
        result_.genJet_phi.push_back(phi);
        result_.genJet_mass.push_back(m);
        result_.genJet_energy.push_back(energy);
        result_.genJet_partonFlavour.push_back(
            pflavArr ? (*const_cast<TTreeReaderArray<Int_t>*>(pflavArr))[i] : 0);
        result_.genJet_hadronFlavour.push_back(
            hflavArr ? static_cast<int>((*const_cast<TTreeReaderArray<UChar_t>*>(hflavArr))[i]) : 0);
    }
    result_.genJet_count = static_cast<int>(n);
}

// =============================================================================
//  ProcessGenMET
// =============================================================================
void TopCPVGenCategorizer::ProcessGenMET() {

    auto ptVal  = GetFloatSingle("GenMET_pt");
    auto phiVal = GetFloatSingle("GenMET_phi");

    if (ptVal)  result_.genMET_pt  = **const_cast<TTreeReaderValue<Float_t>*>(ptVal);
    if (phiVal) result_.genMET_phi = **const_cast<TTreeReaderValue<Float_t>*>(phiVal);
}

// =============================================================================
//  ProcessGenBHadrons
// =============================================================================
void TopCPVGenCategorizer::ProcessGenBHadrons() {

    if (result_.genJet_count == 0) return;

    for (int i = 0; i < result_.genJet_count; ++i) {
        if (result_.genJet_hadronFlavour[i] != 5) continue;

        const float jpt  = result_.genJet_pt[i];
        const float jeta = result_.genJet_eta[i];
        const float jphi = result_.genJet_phi[i];
        const float jen  = result_.genJet_energy[i];

        const int bIdx = FindNearestBQuark(jeta, jphi, 0.4f);

        result_.genBJet_pt.push_back(jpt);
        result_.genBJet_eta.push_back(jeta);
        result_.genBJet_phi.push_back(jphi);
        result_.genBJet_energy.push_back(jen);

        if (bIdx >= 0) {
            auto ptArr   = GetFloatVector("GenPart_pt");
            auto etaArr  = GetFloatVector("GenPart_eta");
            auto phiArr  = GetFloatVector("GenPart_phi");
            auto massArr = GetFloatVector("GenPart_mass");
            auto pdgArr  = GetIntVector  ("GenPart_pdgId");

            const float bpt   = (*const_cast<TTreeReaderArray<Float_t>*>(ptArr))[bIdx];
            const float beta  = (*const_cast<TTreeReaderArray<Float_t>*>(etaArr))[bIdx];
            const float bphi  = (*const_cast<TTreeReaderArray<Float_t>*>(phiArr))[bIdx];
            const float bmass = (*const_cast<TTreeReaderArray<Float_t>*>(massArr))[bIdx];
            const int   bpdg  = (*const_cast<TTreeReaderArray<Int_t>*>(pdgArr))[bIdx];
            const float bch   = std::cosh(beta);
            const float ben   = std::sqrt(bpt*bpt*bch*bch + bmass*bmass);

            result_.genBHad_pt.push_back(bpt);
            result_.genBHad_eta.push_back(beta);
            result_.genBHad_phi.push_back(bphi);
            result_.genBHad_energy.push_back(ben);
            result_.genBHad_flavour.push_back(bpdg);
            result_.genBHad_fromTopWeakDecay.push_back(IsAncestorTop(bIdx) ? 1 : 0);
        } else {
            result_.genBHad_pt.push_back(-999.f);
            result_.genBHad_eta.push_back(-999.f);
            result_.genBHad_phi.push_back(-999.f);
            result_.genBHad_energy.push_back(-999.f);
            result_.genBHad_flavour.push_back(0);
            result_.genBHad_fromTopWeakDecay.push_back(0);
        }
        ++result_.genBJet_count;
        ++result_.genBHad_count;
    }
}

// =============================================================================
//  ProcessPSWeights
// =============================================================================
void TopCPVGenCategorizer::ProcessPSWeights() {

    auto nPS  = GetUIntSingle ("nPSWeight");
    auto wArr = GetFloatVector("PSWeight");
    if (!nPS || !wArr) return;

    const UInt_t n = **const_cast<TTreeReaderValue<UInt_t>*>(nPS);
    auto& ws = *const_cast<TTreeReaderArray<Float_t>*>(wArr);

    result_.psWeight_n = static_cast<int>(n);
    if (n >= 1) result_.psWeight_ISR_up   = ws[0];
    if (n >= 2) result_.psWeight_FSR_up   = ws[1];
    if (n >= 3) result_.psWeight_ISR_down = ws[2];
    if (n >= 4) result_.psWeight_FSR_down = ws[3];
}

// =============================================================================
//  IsAncestorTop
// =============================================================================
std::vector<char> TopCPVGenCategorizer::ReachableFrom(int start) const {
    // GenPart indices reachable from `start` via the daughter map, EXCLUDING
    // start itself — mirrors MiniAOD's IndexLinker(AllParDau, tau, 0, final)
    // membership test (the tau itself was already erased from FinalPar there).
    std::vector<char> seen(daughters_.size(), 0);
    if (start < 0 || start >= static_cast<int>(daughters_.size())) return seen;
    std::vector<int> stack(daughters_[start].begin(), daughters_[start].end());
    while (!stack.empty()) {
        const int x = stack.back();
        stack.pop_back();
        if (x < 0 || x >= static_cast<int>(seen.size()) || seen[x]) continue;
        seen[x] = 1;
        stack.insert(stack.end(), daughters_[x].begin(), daughters_[x].end());
    }
    return seen;
}

bool TopCPVGenCategorizer::IsAncestorTop(int idx, int maxDepth) const {

    auto pdgArr = GetIntVector("GenPart_pdgId");
    auto momArr = GetIntVector("GenPart_genPartIdxMother");
    if (!pdgArr || !momArr) return false;
    auto& pdgs = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);
    auto& moms = *const_cast<TTreeReaderArray<Int_t>*>(momArr);

    int cur = idx;
    int depth = 0;
    while (cur >= 0 && depth++ < maxDepth) {
        if (std::abs(pdgs[cur]) == 6) return true;
        cur = moms[cur];
    }
    return false;
}

// =============================================================================
//  FindNearestBQuark
// =============================================================================
int TopCPVGenCategorizer::FindNearestBQuark(float eta, float phi, float maxDR) const {

    auto nGP    = GetUIntSingle ("nGenPart");
    auto pdgArr = GetIntVector  ("GenPart_pdgId");
    auto flgArr = GetIntVector  ("GenPart_statusFlags");
    auto etaArr = GetFloatVector("GenPart_eta");
    auto phiArr = GetFloatVector("GenPart_phi");
    if (!nGP || !pdgArr || !flgArr || !etaArr || !phiArr) return -1;

    const UInt_t n = **const_cast<TTreeReaderValue<UInt_t>*>(nGP);
    auto& pdgs = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);
    auto& flgs = *const_cast<TTreeReaderArray<Int_t>*>(flgArr);
    auto& etas = *const_cast<TTreeReaderArray<Float_t>*>(etaArr);
    auto& phis = *const_cast<TTreeReaderArray<Float_t>*>(phiArr);

    int bestIdx = -1;
    float bestDR2 = maxDR * maxDR;

    for (UInt_t i = 0; i < n; ++i) {
        if (std::abs(pdgs[i]) != 5) continue;
        if (!(flgs[i] & TopCPVGenStatusBit::isLastCopy)) continue;
        const float deta = etas[i] - eta;
        float dphi = phis[i] - phi;
        while (dphi >  static_cast<float>(M_PI)) dphi -= 2.0f * static_cast<float>(M_PI);
        while (dphi < -static_cast<float>(M_PI)) dphi += 2.0f * static_cast<float>(M_PI);
        const float dr2 = deta*deta + dphi*dphi;
        if (dr2 < bestDR2) {
            bestDR2 = dr2;
            bestIdx = static_cast<int>(i);
        }
    }
    return bestIdx;
}

// =============================================================================
//  Query helpers
// =============================================================================
bool TopCPVGenCategorizer::HasTauSecondaryLepton() const {
    return result_.channel_tau_lepton > 0;
}

std::vector<int> TopCPVGenCategorizer::PromptChargedLeptonsFromW() const {
    std::vector<int> out;
    if (!result_.isSignal) return out;
    auto pdgArr = GetIntVector("GenPart_pdgId");
    if (!pdgArr) return out;
    auto& pdgs = *const_cast<TTreeReaderArray<Int_t>*>(pdgArr);
    for (int slot = 8; slot <= 11; ++slot) {
        const int idx = result_.selectedIdx[slot];
        if (idx < 0) continue;
        const int absPdg = std::abs(pdgs[idx]);
        if (absPdg == 11 || absPdg == 13) out.push_back(idx);
    }
    return out;
}

void TopCPVGenCategorizer::DumpEvent(std::ostream& os) const {
    os << "GenCat:"
       << " sig=" << (result_.isSignal ? 1 : 0)
       << " chIdx=" << result_.channel_idx
       << " chIdxFinal=" << result_.channel_idx_final
       << " chJets=" << result_.channel_jets
       << " tauLep=" << result_.channel_tau_lepton
       << " visTau=" << result_.channel_visible_tau;
    if (result_.isSignal)
        os << " topPt=" << result_.genTop_pt
           << " tbarPt=" << result_.genAnTop_pt;
    os << "\n";
}

// =============================================================================
//  ClearState
// =============================================================================
void TopCPVGenCategorizer::ClearState() {
    result_.isSignal = false;
    result_.selectedIdx.fill(-1);
    result_.genPar_idx.clear();
    result_.genPar_pdgId.clear();
    result_.genPar_status.clear();
    result_.genPar_pt.clear();
    result_.genPar_eta.clear();
    result_.genPar_phi.clear();
    result_.genPar_mass.clear();
    result_.genPar_energy.clear();
    result_.genPar_mom1Idx.clear();
    result_.genPar_mom2Idx.clear();
    result_.genPar_dau1Idx.clear();
    result_.genPar_dau2Idx.clear();
    result_.genPar_nMom.clear();
    result_.genPar_nDau.clear();
    result_.genPar_count = 0;
    result_.genTop_pt   = result_.genTop_eta   = result_.genTop_phi   = result_.genTop_energy   = -999.f;
    result_.genAnTop_pt = result_.genAnTop_eta = result_.genAnTop_phi = result_.genAnTop_energy = -999.f;
    result_.channel_idx          = 0;
    result_.channel_idx_final    = 0;
    result_.channel_lepton       = 0;
    result_.channel_lepton_final = 0;
    result_.channel_jets         = 0;
    result_.channel_jets_abs     = 0;
    result_.channel_tau_lepton   = 0;
    result_.channel_visible_tau  = 0;
    result_.channel_idx_expanded = 0;
    tauDesc_.clear();

    result_.genJet_pt.clear();
    result_.genJet_eta.clear();
    result_.genJet_phi.clear();
    result_.genJet_mass.clear();
    result_.genJet_energy.clear();
    result_.genJet_partonFlavour.clear();
    result_.genJet_hadronFlavour.clear();
    result_.genJet_count = 0;

    result_.genMET_pt  = -999.f;
    result_.genMET_phi = -999.f;

    result_.genBJet_pt.clear();
    result_.genBJet_eta.clear();
    result_.genBJet_phi.clear();
    result_.genBJet_energy.clear();
    result_.genBHad_pt.clear();
    result_.genBHad_eta.clear();
    result_.genBHad_phi.clear();
    result_.genBHad_energy.clear();
    result_.genBHad_fromTopWeakDecay.clear();
    result_.genBHad_flavour.clear();
    result_.genBJet_count = 0;
    result_.genBHad_count = 0;

    result_.psWeight_ISR_up   = 1.f;
    result_.psWeight_FSR_up   = 1.f;
    result_.psWeight_ISR_down = 1.f;
    result_.psWeight_FSR_down = 1.f;
    result_.psWeight_n        = 0;
}

// =============================================================================
//  SyncOutputBuffers
// =============================================================================
void TopCPVGenCategorizer::SyncOutputBuffers() {
    // Event identifier (friend-tree join key). When the input lacks these
    // branches the key is written as zero, signalling "no valid key".
    if (haveEventId_) {
        out_run_   = **evReaderRun_;
        out_lumi_  = **evReaderLumi_;
        out_event_ = **evReaderEvent_;
    } else {
        out_run_ = 0; out_lumi_ = 0; out_event_ = 0;
    }

    out_isSignal_ = result_.isSignal;
    for (int i = 0; i < 12; ++i) out_selectedIdx_[i] = result_.selectedIdx[i];
    out_genPar_count_  = result_.genPar_count;
    out_genPar_idx_    = result_.genPar_idx;
    out_genPar_pdgId_  = result_.genPar_pdgId;
    out_genPar_status_ = result_.genPar_status;
    out_genPar_pt_     = result_.genPar_pt;
    out_genPar_eta_    = result_.genPar_eta;
    out_genPar_phi_    = result_.genPar_phi;
    out_genPar_mass_   = result_.genPar_mass;
    out_genPar_energy_ = result_.genPar_energy;
    out_genPar_mom1Idx_= result_.genPar_mom1Idx;
    out_genPar_mom2Idx_= result_.genPar_mom2Idx;
    out_genPar_dau1Idx_= result_.genPar_dau1Idx;
    out_genPar_dau2Idx_= result_.genPar_dau2Idx;
    out_genPar_nMom_   = result_.genPar_nMom;
    out_genPar_nDau_   = result_.genPar_nDau;
    out_genTop_pt_     = result_.genTop_pt;
    out_genTop_eta_    = result_.genTop_eta;
    out_genTop_phi_    = result_.genTop_phi;
    out_genTop_energy_ = result_.genTop_energy;
    out_genAnTop_pt_     = result_.genAnTop_pt;
    out_genAnTop_eta_    = result_.genAnTop_eta;
    out_genAnTop_phi_    = result_.genAnTop_phi;
    out_genAnTop_energy_ = result_.genAnTop_energy;
    out_channel_idx_          = result_.channel_idx;
    out_channel_idx_final_    = result_.channel_idx_final;
    out_channel_lepton_       = result_.channel_lepton;
    out_channel_lepton_final_ = result_.channel_lepton_final;
    out_channel_jets_         = result_.channel_jets;
    out_channel_jets_abs_     = result_.channel_jets_abs;
    out_channel_tau_lepton_   = result_.channel_tau_lepton;
    out_channel_visible_tau_  = result_.channel_visible_tau;
    out_channel_idx_expanded_ = result_.channel_idx_expanded;

    out_genJet_count_         = result_.genJet_count;
    out_genJet_pt_            = result_.genJet_pt;
    out_genJet_eta_           = result_.genJet_eta;
    out_genJet_phi_           = result_.genJet_phi;
    out_genJet_mass_          = result_.genJet_mass;
    out_genJet_energy_        = result_.genJet_energy;
    out_genJet_partonFlavour_ = result_.genJet_partonFlavour;
    out_genJet_hadronFlavour_ = result_.genJet_hadronFlavour;

    out_genMET_pt_  = result_.genMET_pt;
    out_genMET_phi_ = result_.genMET_phi;

    out_genBJet_count_           = result_.genBJet_count;
    out_genBHad_count_           = result_.genBHad_count;
    out_genBJet_pt_              = result_.genBJet_pt;
    out_genBJet_eta_             = result_.genBJet_eta;
    out_genBJet_phi_             = result_.genBJet_phi;
    out_genBJet_energy_          = result_.genBJet_energy;
    out_genBHad_pt_              = result_.genBHad_pt;
    out_genBHad_eta_             = result_.genBHad_eta;
    out_genBHad_phi_             = result_.genBHad_phi;
    out_genBHad_energy_          = result_.genBHad_energy;
    out_genBHad_fromTopWeakDecay_= result_.genBHad_fromTopWeakDecay;
    out_genBHad_flavour_         = result_.genBHad_flavour;

    out_psWeight_n_        = result_.psWeight_n;
    out_psWeight_ISR_up_   = result_.psWeight_ISR_up;
    out_psWeight_FSR_up_   = result_.psWeight_FSR_up;
    out_psWeight_ISR_down_ = result_.psWeight_ISR_down;
    out_psWeight_FSR_down_ = result_.psWeight_FSR_down;
}
