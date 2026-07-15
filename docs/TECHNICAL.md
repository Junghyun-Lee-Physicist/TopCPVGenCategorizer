# TopCPVGenCategorizer — 기술 문서 (Technical Documentation)

> **목적**: 이 문서만 읽고도 다른 사용자가 코드의 로직과 설계 결정을 완전히 재현할 수 있도록 작성됨.
>
> **프로젝트**: CMS ttbar CP-violation 분석을 위한 MiniAOD `SSBAnalyzer` generator 섹션의 NanoAOD 마이그레이션
>
> **작성 배경**: 기존 Run-2 분석 코드는 MiniAOD + CMSSW EDAnalyzer 기반이었음. NanoAOD 기반의 새 standalone analyzer 프레임워크(`Analysis` 클래스, TTreeReader 기반)로 이전하면서, generator-level truth 정보(가계도, 채널 분류, GenJet, GenMET, ghost-B 매칭)를 재현하는 독립 클래스가 필요했음.

---

## 목차

1. [원본 MiniAOD 코드가 하던 일](#1-원본-miniaod-코드가-하던-일)
2. [NanoAOD 포맷과의 차이 및 핵심 아이디어](#2-nanoaod-포맷과의-차이-및-핵심-아이디어)
3. [TopCPVGenCategorizer 클래스 설계](#3-topcpvgencategorizer-클래스-설계)
4. [알고리즘 상세 — 함수별 로직](#4-알고리즘-상세--함수별-로직)
5. [출력 branch 명세](#5-출력-branch-명세)
6. [GenPar_Idx / SelectedIdx — "gen index"의 의미와 검증법](#6-genpar_idx--selectedidx--gen-index의-의미와-검증법)
7. [MiniAOD ↔ NanoAOD 변수 매핑 테이블](#7-miniaod--nanoaod-변수-매핑-테이블)
8. [알려진 한계 (B-fragmentation 등)](#8-알려진-한계)
9. [검증 기록](#9-검증-기록)
10. [HTCondor 대량 처리](#10-htcondor-대량-처리)

---

## 1. 원본 MiniAOD 코드가 하던 일

원본 `SSBAnalyzer.cc` (CMSSW EDAnalyzer, ~1100줄의 generator 관련 코드)는 5개 섹션으로 구성:

| # | 섹션 | 입력 (CMSSW token) | 출력 branch |
|---|---|---|---|
| 1 | `GenPar()` | GenEventInfoProduct, GenParticleCollection | GenPar_*, GenTop, GenAnTop, Channel_* |
| 2 | `GenJet()` | GenJetCollection | GenJet, GenJet_HCalEnergy, GenJet_ECalEnergy |
| 3 | `GenMET()` | GenMETCollection | GenMET, GenMET_Count |
| 4 | Ghost B-hadron | genBHadIndex, genBHadJetIndex, genBHadFromTopWeakDecay, genBHadFlavour | GenBJet, GenBHad, GenBHad_FromTopWeakDecay, GenBHad_Flavour |
| 5 | B-fragmentation | bfragWgtProducer:{up,central,down,Peterson}Frag, semilepbr{Up,Down} | Frag_*_Weight, Semilep_Br*_Weight |

### 1.1 GenPar()의 5단계 알고리즘 (원본)

```
[Step 1] Hadronizer 판별 (moduleName "Pythia" vs "PEGHadronizer")
[Step 2] 모든 GenParticle 순회 → AllParMom / AllParDau / AllParInfo 맵 구축
[Step 3] 후보 분류:
         - TreePar:  hard process 후보 (PYTHIA status 21-23, HERWIG status 11)
         - FinalPar: final state lepton (status 1,2 & |pdgId| 11-16)
[Step 4] ttbar 신호 판별: SelectedpdgId에 +6과 -6이 모두 있으면 isSignal=true
         Signal이면 IndexLinker() 재귀로 12-slot SelectedPar 배열 구성
[Step 5] Channel index 계산 (τ leptonic decay 추적 포함)
```

### 1.2 12-slot 컨벤션 (분석 전체에서 사용되는 핵심 규약)

```
slot 0, 1   beam protons
slot 2      t   (physics top, 마지막 복사본)
slot 3      t̄
slot 4      W+  (t의 딸)
slot 5      b   (t의 딸)
slot 6      W-  (t̄의 딸)
slot 7      b̄
slot 8, 9   W+의 두 daughter (ℓ+ν 또는 quark pair)
slot 10, 11 W-의 두 daughter
```

**CPV 분석에서 이 규약이 중요한 이유**: slot 위치만으로 "어느 lepton이 t에서 왔는지 / t̄에서 왔는지"가 보장된다. 예를 들어 slot 8이 ℓ+이면 그것은 반드시 t→W+→ℓ+ 경로다. Triple-product CP 관측량 계산 시 (top, antitop, ℓ+, ℓ-)의 4-vector를 모호함 없이 가져올 수 있다.

### 1.3 Channel 코드 체계

직접 W→ℓ decay의 |pdgId|를 합산 (τ는 부호 반전):

| Channel_Idx | 채널 |
|---|---|
| 0 | all-hadronic |
| 11 | ℓ+jets (e) |
| 13 | ℓ+jets (μ) |
| 22 | dilepton ee |
| 24 | dilepton eμ |
| 26 | dilepton μμ |
| 음수 | τ가 개입됨 (예: -4 = e+τ, -2 = μ+τ, -30 = ττ) |

`Channel_Idx_Final`은 τ→ℓνν leptonic decay까지 따라가서 "검출기가 실제로 보는" lepton 기준으로 다시 계산한 값.

`Channel_Jets`: hadronic W의 quark pair 코드 (예: W+→ud̄ = 12, W+→cs̄ = 34). 두 W 모두 hadronic이면 100단위로 연결.

> **정의 상세**: `Channel_Idx`(W-decay level, τ는 후속 붕괴 미분해)와 `Channel_Idx_Final`(τ→e/μ/had 분해)의 물리적 정의·경계 케이스·τ 마이그레이션 패턴은 `docs/GenPart_channel_definition.md` §8–§9, §13 참조. 그 문서는 CMSSW 공식 소스로 검증됨.

---

## 2. NanoAOD 포맷과의 차이 및 핵심 아이디어

### 2.1 NanoAOD가 제공하는 결정적 단순화: GenPart_statusFlags

NanoAOD의 `GenPart_statusFlags`는 15-bit 비트마스크로, **generator(PYTHIA/HERWIG)에 무관하게 통일된 의미**를 가진다:

```
bit  0  isPrompt
bit  1  isDecayedLeptonHadron
bit  2  isTauDecayProduct
bit  3  isPromptTauDecayProduct
bit  4  isDirectTauDecayProduct
bit  5  isDirectPromptTauDecayProduct
bit  6  isDirectHadronDecayProduct
bit  7  isHardProcess          ← MiniAOD의 status 21-23 (PYTHIA) / 11 (HERWIG) 대체
bit  8  fromHardProcess
bit  9  isHardProcessTauDecayProduct
bit 10  isDirectHardProcessTauDecayProduct
bit 11  fromHardProcessBeforeFSR
bit 12  isFirstCopy
bit 13  isLastCopy             ← "physics top" 식별의 핵심
bit 14  isLastCopyBeforeFSR
```

이로 인해:
- **Hadronizer 분기 전체 삭제** — PYTHIA/HERWIG 별도 코드 경로(~600줄)가 불필요
- **IndexLinker 재귀 삭제** — `isLastCopy` 비트가 이미 "이 입자가 radiation chain의 최종본"임을 표시

> **배경**: `GenPart`는 generator record 전체가 아니라 `finalGenParticles`(GenParticlePruner)로 추려낸 "interesting gen particles" collection이다. keep/drop rule, τ decay chain 보존 방식, "GenPart에는 hadronization 후 모든 stable particle이 있는 게 아니다"라는 한계는 `docs/GenPart_channel_definition.md` §3–§6, §10 참조.

### 2.2 가계도 정보의 차이

| | MiniAOD | NanoAOD |
|---|---|---|
| Mother | `numberOfMothers()` ≥ 2 가능 | `genPartIdxMother` 단일 int |
| Daughter | `daughter(N)` 직접 접근 | **없음** — mother 인덱스를 역산해야 함 |
| Beam proton | 컬렉션에 존재 | pruning으로 제거됨 |

→ **Daughter 맵은 mother 인덱스를 한 번 순회해서 역산 (O(N))**:

```cpp
daughters_.assign(n, {});
for (UInt_t i = 0; i < n; ++i) {
    const Int_t mom = GenPart_genPartIdxMother[i];
    if (mom >= 0) daughters_[mom].push_back(i);
}
```

### 2.3 τ 추적의 단순화

MiniAOD에서는 `SelParDau` 맵을 만들어 τ daughter를 재귀적으로 추적했지만, NanoAOD의 `GenDressedLepton_hasTauAnc` 비트 하나가 그 정보를 직접 제공한다. `GenDressedLepton`은 FSR 광자가 합쳐진(dressed) final-state lepton 컬렉션이므로 "검출기가 보는 lepton"에 가장 가깝다.

### 2.4 Ghost-B 매칭의 단순화

MiniAOD에서는 별도 CMSSW 모듈(`matchGenBHadron`)이 B-hadron을 ghost particle로 jet에 재클러스터링한 결과를 5개 토큰으로 받았다. NanoAOD에서는 그 결과가 이미 `GenJet_hadronFlavour` branch에 들어있다 (5=b, 4=c, 0=light).

---

## 3. TopCPVGenCategorizer 클래스 설계

### 3.1 설계 원칙

1. **기존 프레임워크와 컨벤션 일치**: `Analysis` 클래스와 동일하게 TTreeReader + `unordered_map<string, unique_ptr<TTreeReaderValue/Array<T>>>` 패턴, `interface/` + `src/` 분리. (클래스 접두사는 v1.9에서 `SSBGen` → `TopCPVGen`으로 변경 — 원본 MiniAOD 프레임워크의 `SSBAnalyzer` 등 외부 이름 인용은 그대로 유지.)
2. **의존성 최소화**: ROOT만 필요. correctionlib / RoccoR / boost / TextReader config 불필요 (gen categorization은 generator truth만으로 결정됨).
3. **Branch 목록 하드코딩**: NanoAOD 포맷이 정한 31개 branch를 `RegisterBranches()`의 `constexpr BranchSpec` 배열에 명시. 외부 branchlist 파일 없음 — 포맷이 바뀌면 코드 한 곳만 수정.
4. **MC 전용, fail-fast**: 생성자에서 `TChain::GetBranch()`로 GenPart core branch 존재를 검사하고, 없으면(=Data 샘플) 즉시 `std::runtime_error`. Silent 0 출력 금지.
5. **출력 branch 이름은 MiniAOD와 동일**: downstream 분석 코드 재사용 가능.

### 3.2 Dual-mode 아키텍처

**Mode A — Standalone**: 자체 TTreeReader, 자체 출력 파일. `main_gencat.cpp`가 진입점.

```cpp
TopCPVGenCategorizer gen(chain, "output.root", seDir, maxEvents);
gen.Loop();
```

**Mode B — Plug-in**: 기존 `Analysis` 인스턴스의 branch map에 연결. host의 이벤트 루프 안에서 `ProcessEvent()` 호출.

```cpp
// Analysis 생성자에서 (MC에서만)
genCat_ = new TopCPVGenCategorizer();
genCat_->AttachExternal(&uintSingles, &floatSingles, &floatVectors,
                        &intVectors, &boolVectors, &ucharVectors);

// Analysis::Loop() 안에서
const GenCatResult& r = genCat_->ProcessEvent();
if (r.channel_idx != 24) continue;                  // eμ 채널만
if (genCat_->HasTauSecondaryLepton()) continue;     // τ-오염 이벤트 제거
```

Mode B에서는 host의 branchlist에 GenPart_*, GenDressedLepton_*, GenVisTau_*, GenJet_*, GenMET_*, PSWeight branch가 포함되어 있어야 한다.

### 3.3 소비하는 NanoAOD branch (31개, 하드코딩)

```
GenPart core (9):
    nGenPart, GenPart_{pt,eta,phi,mass,pdgId,status,statusFlags,genPartIdxMother}
GenDressedLepton (6):
    nGenDressedLepton, GenDressedLepton_{pt,eta,phi,pdgId,hasTauAnc}
GenVisTau (5):
    nGenVisTau, GenVisTau_{pt,eta,phi,genPartIdxMother}
GenJet (7):
    nGenJet, GenJet_{pt,eta,phi,mass,partonFlavour,hadronFlavour}
    ※ GenJet_hadronFlavour는 UChar_t 타입 주의
GenMET (2):
    GenMET_pt, GenMET_phi (single, vector 아님)
PSWeight (2):
    nPSWeight, PSWeight
```

---

## 4. 알고리즘 상세 — 함수별 로직

`ProcessEvent()`가 per-event로 호출하는 순서:

```
ClearState()
  → BuildDaughterMap()        // mother 인덱스 역산, O(N)
  → FindTopAntiTop()          // isLastCopy(±6) 검색 → isSignal 결정
  → FillSignalSelection()     // (signal일 때) 12-slot 채우기
    or FillBackgroundSelection()
  → ComputeChannelDirect()    // W daughter slot 8-11에서 Channel_Idx
  → ComputeChannelFinal()     // GenDressedLepton에서 Channel_Idx_Final
  → ComputeChannelJets()      // hadronic W quark pair 코드
  → ProcessGenJets()          // GenJet 컬렉션 + flavour 저장
  → ProcessGenMET()           // GenMET pt/phi 저장
  → ProcessGenBHadrons()      // b-jet ↔ b-quark ΔR 매칭 + FromTopWeakDecay
  → ProcessPSWeights()        // PSWeight[0..3] 저장
  → SyncOutputBuffers()       // result_ → TTree 버퍼 복사 (outTree_가 있을 때)
```

### 4.1 FindTopAntiTop()

```cpp
for (i in GenPart) {
    if (!(statusFlags[i] & isLastCopy)) continue;
    if (pdgId[i] == +6 && tIdx    < 0) tIdx    = i;
    if (pdgId[i] == -6 && tbarIdx < 0) tbarIdx = i;
}
isSignal = (tIdx >= 0 && tbarIdx >= 0);
```

- `isLastCopy` 비트가 radiation chain의 최종 top을 보장하므로, 이벤트당 t와 t̄가 각각 정확히 1개씩 잡힌다.
- top kinematics는 즉시 캐싱: `genTop_pt/eta/phi/energy` (energy는 `sqrt(pt²cosh²η + m²)`로 계산 — NanoAOD는 mass만 저장하므로).

### 4.2 FindLastDaughter(parent, targetPdg)

t→W 체인에서 radiation copy를 건너뛰고 "physics W"를 찾는 함수:

```
parent의 daughter 중 pdgId == targetPdg인 것을 찾고,
그 입자가 같은 pdgId의 daughter를 또 가지면(W → W radiation copy)
chain 끝까지 따라간다.
```

MiniAOD `IndexLinker()` 재귀(조상 방향 backtracking)와 달리, 자손 방향으로 한 방향으로만 내려가므로 단순하고 무한루프 위험이 없다.

### 4.3 FillSignalSelection() — 12-slot 구성

```cpp
selectedIdx[4] = FindLastDaughter(tIdx,    +24);   // W+
selectedIdx[5] = FindLastDaughter(tIdx,    +5);    // b
selectedIdx[6] = FindLastDaughter(tbarIdx, -24);   // W-
selectedIdx[7] = FindLastDaughter(tbarIdx, -5);    // b̄
(selectedIdx[8..11]) = WDaughters(W±)              // W copy 제외 첫 두 daughter
selectedIdx[0] = selectedIdx[1] = -1;              // proton은 NanoAOD에 없음
```

slot 간 가계 관계는 정적 테이블로 인코딩되어 `GenPar_Mom1_Idx`/`GenPar_Dau1_Idx` 등에 기록된다:

```cpp
parentSlot[12] = {-1,-1,-1,-1, 2,2,3,3, 4,4,6,6};
dau1Slot[12]   = {-1,-1, 4, 6, 8,-1,10,-1, -1,-1,-1,-1};
dau2Slot[12]   = {-1,-1, 5, 7, 9,-1,11,-1, -1,-1,-1,-1};
```

### 4.4 FillBackgroundSelection()

ttbar가 아닌 이벤트(DY, W+jets, single top 등): 12-slot 구조 대신 가변 길이 리스트.
1. last-copy 무거운 boson (|pdgId| ∈ {6, 23, 24, 25}) 수집
2. 그 boson들의 직접 daughter (같은 pdgId radiation copy 제외) 수집
3. hard-process 출신 last-copy τ 중 미수집분 추가

### 4.5 ComputeChannelDirect / Final / Jets

- **Direct**: slot 8-11의 |pdgId| ∈ {11,13,15}를 합산. τ는 부호 반전 (`chIdx += (absPdg==15) ? -absPdg : absPdg`).
- **Final**: `GenDressedLepton` 전체를 순회하며 e/μ 카운트. `hasTauAnc==true`인 lepton 수는 `Channel_Tau_Lepton`으로 별도 저장 — **CPV 분석의 τ-오염 제거 필터가 이 값에 기반**.
- **Visible τ**: `nGenVisTau`를 그대로 `Channel_Visible_Tau`로 (hadronic τ 개수). MiniAOD는 이 정보를 제대로 다루지 못했으므로 NanoAOD의 보너스.
- **Jets**: 두 W 각각에 대해 daughter가 모두 quark(|pdgId|<10)이면 코드 생성:
  - W+ (charge>0): `p1`이 짝수(up-type)이면 `10*p1+p2`, 아니면 `p1+10*p2`
  - W- : 반대 조건
  - 둘 다 hadronic이면 `100*code1 + code2`
  - `Channel_Jets_Abs`는 자릿수 정렬로 정규화 (ud=12와 du=21을 같은 값으로)

### 4.6 ProcessGenBHadrons() — ghost-B 등가 구현

```
for (각 GenJet with hadronFlavour == 5):       ← ghost clustering 결과는 이미 branch에
    GenBJet 4-momentum 저장
    bIdx = FindNearestBQuark(jet η, jet φ, ΔR ≤ 0.4)   ← AK4 cone
    if (bIdx 발견):
        GenBHad에 b-quark kinematics + 부호있는 pdgId(±5) 저장
        FromTopWeakDecay = IsAncestorTop(bIdx) ? 1 : 0   ← mother chain walk
    else:
        sentinel(-999) 저장 — 배열 평행성 유지
```

`IsAncestorTop()`: `genPartIdxMother` 체인을 거슬러 올라가며 |pdgId|==6인 조상이 있는지 확인 (maxDepth=100 보호).

`FindNearestBQuark()`: |pdgId|==5 && isLastCopy인 GenPart 중 ΔR이 최소인 것. Δφ는 [-π, π]로 wrap.

**MiniAOD와의 의미 차이**: MiniAOD는 hadronization 후의 *B-hadron* 자체를 식별했지만, NanoAOD pruning은 보통 B-hadron을 제거하고 b-quark를 남긴다. ttbar truth labelling 목적(어느 jet이 t→bW의 b인가?)에서는 둘 다 같은 top을 가리키므로 등가.

### 4.7 ProcessPSWeights()

```
PSWeight[0] = ISR up   (ISR×2,   FSR×1)
PSWeight[1] = FSR up   (ISR×1,   FSR×2)
PSWeight[2] = ISR down (ISR×0.5, FSR×1)
PSWeight[3] = FSR down (ISR×1,   FSR×0.5)
```

이는 **B-fragmentation weight의 대체가 아니라 근사 proxy**다 (8장 참고).

---

## 5. 출력 branch 명세

출력 TTree 이름: `GenCatTree`. 모든 branch는 MiniAOD 출력과 이름 호환.

| Branch | 타입 | 의미 |
|---|---|---|
| `isSignal` | Bool | t와 t̄ last copy가 모두 존재 |
| `SelectedIdx[12]` | Int[12] | 12-slot → GenPart 인덱스 매핑 (-1 = 없음) |
| `GenPar_Count` | Int | 저장된 입자 수 (signal=12, bkg=가변) |
| `GenPar_Idx` | vector<Int> | 각 슬롯이 가리키는 GenPart 인덱스 |
| `GenPar_pdgId`, `GenPar_Status` | vector<Int> | PDG ID, Pythia status |
| `GenPar_pt/eta/phi/mass/energy` | vector<Float> | 4-momentum (energy는 계산값) |
| `GenPar_Mom1_Idx`, `Mom2_Idx`, `Mom_Counter` | vector<Int> | 어머니 인덱스/개수 |
| `GenPar_Dau1_Idx`, `Dau2_Idx`, `Dau_Counter` | vector<Int> | 딸 인덱스/개수 |
| `GenTop_pt/eta/phi/energy` | Float | top 4-momentum |
| `GenAnTop_pt/eta/phi/energy` | Float | anti-top 4-momentum |
| `Channel_Idx` | Int | 직접 W→ℓ 채널 코드 |
| `Channel_Idx_Final` | Int | τ→ℓ 포함 최종 채널 |
| `Channel_Lepton_Count(_Final)` | Int | lepton 개수 |
| `Channel_Jets`, `Channel_Jets_Abs` | Int | hadronic W quark pair 코드 |
| `Channel_Tau_Lepton` | Int | hasTauAnc==true인 dressed lepton 수 |
| `Channel_Visible_Tau` | Int | hadronic τ (GenVisTau) 수 |
| `GenJet_Count`, `GenJet_pt/eta/phi/mass/energy` | — | GenJet 컬렉션 |
| `GenJet_PartonFlavour`, `GenJet_HadronFlavour` | vector<Int> | jet flavour (hadron: 5/4/0) |
| `GenMET_pt`, `GenMET_phi` | Float | true MET |
| `GenBJet_Count`, `GenBJet_pt/eta/phi/energy` | — | hadronFlavour==5 jet |
| `GenBHad_Count`, `GenBHad_pt/eta/phi/energy` | — | 매칭된 b-quark |
| `GenBHad_FromTopWeakDecay` | vector<Int> | 1 = 조상에 top 존재 |
| `GenBHad_Flavour` | vector<Int> | 부호있는 pdgId (±5) |
| `PSWeight_n`, `PSWeight_{ISR,FSR}_{Up,Down}` | — | shower systematic |

---

## 6. GenPar_Idx / SelectedIdx — "gen index"의 의미와 검증법

### 6.1 정의

- **`SelectedIdx[12]`**: 12-slot 가계도 배열. `SelectedIdx[k]` = k번째 슬롯의 입자가 **NanoAOD GenPart 컬렉션에서 몇 번째인지**. MiniAOD의 내부 변수 `SelectedPar`에 대응.
- **`GenPar_Idx`**: 저장된 입자 리스트의 각 항목이 가리키는 GenPart 인덱스 (signal 이벤트에서는 SelectedIdx와 동일한 12개 값).

### 6.2 ⚠️ MiniAOD 분포와 직접 비교하면 안 되는 이유

인덱스는 **컬렉션 내 위치**일 뿐이며, MiniAOD(prunedGenParticles)와 NanoAOD(GenPart)는 pruning 정책이 다르다:

| | MiniAOD prunedGenParticles | NanoAOD GenPart |
|---|---|---|
| beam proton | 포함 (보통 index 0,1) | 제거 |
| 전형적 컬렉션 크기 | 수백 | 수십~백여 개 |
| top last copy 위치 | ~15-30 | ~5-15 |

따라서 **GenPar_Idx의 절대값 히스토그램은 MiniAOD와 다르게 나오는 것이 정상**이다. 상사가 기억하는 MiniAOD 분포와 모양이 달라도 버그가 아니다.

### 6.3 올바른 검증법

인덱스가 가리키는 **물리량**을 비교한다. `validation/checkGenIndex.py` 스크립트가 다음을 자동 수행:

1. **슬롯 무결성**: signal 이벤트에서 slot 2-11이 모두 ≥0인가? slot 2의 pdgId가 +6인가? slot 4가 +24인가? (슬롯별 pdgId 정합성 100%여야 정상)
2. **인덱스 순서 보존**: NanoAOD GenPart는 위상 순서이므로 mother 인덱스 < daughter 인덱스. 즉 `SelectedIdx[2] (t) < SelectedIdx[4] (W+)` 가 모든 이벤트에서 성립해야 한다.
3. **물리 분포 비교**: `GenPart_pt[SelectedIdx[2]]` (top pT) 분포를 MiniAOD의 GenTop pT 분포와 비교 — **이것은 포맷에 무관하게 일치해야 한다**.
4. **인덱스 분포 자체**: `SelectedIdx[k]` 히스토그램을 슬롯별로 그려 모양 파악 — top은 앞쪽(작은 인덱스), W daughter는 뒤쪽 경향. 상사 보고용.

```bash
# 사용 예
python3 validation/checkGenIndex.py gencat_out.root
```

---

## 7. MiniAOD ↔ NanoAOD 변수 매핑 테이블

| MiniAOD branch | NanoAOD 소스 | 재현 상태 | 비고 |
|---|---|---|---|
| GenPar (pt/eta/phi/energy) | GenPart_pt/eta/phi/mass | ✓ exact | energy = √(pt²cosh²η+m²) |
| GenPar_pdgId, Status | GenPart_pdgId, status | ✓ exact | 직접 |
| GenPar_Mom*/Dau* 인덱스 | genPartIdxMother + 역맵 | ✓ exact | 단일 mother 한정 |
| GenTop / GenAnTop | isLastCopy(t/t̄) | ✓ exact | statusFlags bit 13 |
| Channel_Idx 등 | W daughter에서 계산 | ✓ exact | 동일 알고리즘 |
| Channel_Idx_Final | GenDressedLepton_hasTauAnc | ✓ exact | 재귀를 1비트로 대체 |
| GenJet (kinematics) | GenJet_pt/eta/phi/mass | ✓ exact | energy 계산 |
| GenJet_HCalEnergy / ECalEnergy | — | ✗ 불가 | NanoAOD에 미저장 |
| GenMET | GenMET_pt, GenMET_phi | ✓ exact | 이벤트당 1개 |
| GenBJet | GenJet[hadronFlavour==5] | ✓ 등가 | ghost 결과가 branch에 |
| GenBHad (kinematics) | GenPart[|pdgId|=5, lastCopy] ΔR매칭 | ≈ 근사 | B-hadron 대신 b-quark |
| GenBHad_Flavour | 매칭된 GenPart_pdgId | ✓ 등가 | 항상 ±5 |
| GenBHad_FromTopWeakDecay | mother chain → top | ≈ 근사 | genPartIdxMother walk |
| Frag_*_Weight (4종) | — | ✗ 누락 | bfragWgtProducer 필요 |
| Semilep_Br*_Weight | — | ✗ 누락 | 동상 |
| — | PSWeight[0..3] | + 보너스 | NanoAOD 전용 |
| — | GenVisTau (hadronic τ) | + 보너스 | MiniAOD가 못 하던 것 |

---

## 8. 알려진 한계

### 8.1 B-fragmentation weights 부재 (가장 중요)

MiniAOD의 `bfragWgtProducer`는 **central NanoAOD production에 포함되지 않은 커스텀 CMSSW 모듈**이다. 따라서 다음 weight들은 NanoAOD에서 직접 얻을 수 없다:

- `Frag_Cen/Up/Down_Weight` (Bowler-Lund 변동)
- `Frag_Peterson_Weight` (대체 fragmentation function)
- `Semilep_BrUp/Down_Weight` (B→ℓν branching ratio 변동)

**NanoAOD의 PSWeight는 parton-shower(ISR/FSR) 변동이지 fragmentation 변동이 아니다** — 관련은 있으나 동일한 systematic이 아니다.

CPV 분석에서 B-fragmentation systematic이 정식으로 필요할 때의 3가지 옵션:
1. **PSWeight를 근사 proxy로 사용** (현재 코드가 자동 저장 — 1차 분석에는 충분할 수 있음)
2. **커스텀 NanoAOD 재생산** — bfragWgtProducer를 포함한 production 요청
3. **MiniAOD friend tree** — bfragWgtProducer를 MiniAOD에서 돌리고 (run, lumi, event)로 join → **§8.3에 구현됨** (권장)

### 8.2 기타 손실

- `GenJet_HCalEnergy` / `ECalEnergy`: reco::GenJet API 전용 — NanoAOD에 없음. 분석에서 사용하지 않는 것으로 확인됨.
- Two-mother 정보: NanoAOD는 mother가 1개. ttbar 가계도에는 영향 없음.
- Beam proton: slot 0, 1은 항상 -1.

---


### 8.3 B-fragmentation weight 복구 — friend tree 방식 (구현됨, v1.5+)

B-fragmentation weight (`Frag_Central/Up/Down`, `Peterson`, `Semilep_BrUp/Down`)는
MiniAOD `SSBAnalyzer`에서 커스텀 모듈 `bfragWgtProducer`로 생성되며, **표준
NanoAOD에는 없고 NanoAOD 내용만으로 재계산할 수도 없다**. GenCatTree의 `PSWeight`
(ISR/FSR) 가지는 parton-shower proxy일 뿐 fragmentation systematic이 아니다.

이 weight는 이미 MiniAOD 산출물에 존재하므로, **event identifier로 join**하는 것이
정확하고 비용이 낮은 복구 방법이다.

**1단계 — join key 저장 (코드에 내장됨, v1.5+)**: `TopCPVGenCategorizer`는 standalone
모드에서 입력 NanoAOD의 `run` / `luminosityBlock` / `event`를 그대로 GenCatTree에
기록한다 (`run/i`, `luminosityBlock/i`, `event/l`). 입력에 이 가지가 없으면
경고를 내고 key를 0으로 채운다 (friend join 불가 신호). plug-in 모드에서는 host
Analysis가 이미 event-id를 가지므로 기록하지 않는다.

**2단계 — friend tree 생성**: `validation/makeBfragFriend.py`가 MiniAOD 트리에서
(run, lumi, event) + 6개 weight를 읽어 dict로 색인한 뒤, GenCatTree를 그 순서대로
순회하며 key로 weight를 찾아 **동일 엔트리 수·동일 순서**의 friend tree
`BfragFriend`를 만든다. MiniAOD에 없는 event는 기본값(1.0)으로 채운다.

```bash
python validation/makeBfragFriend.py \
    --nano gencat_output.root  --nano-tree GenCatTree \
    --mini ssb_miniaod.root    --mini-tree SSBTree \
    --out  bfrag_friend.root
```

**3단계 — 분석에서 attach**: 엔트리 수와 순서가 같으므로 `AddFriend`가 1:1 정렬한다.

```cpp
TTree* t = (TTree*)TFile::Open("gencat_output.root")->Get("GenCatTree");
t->AddFriend("BfragFriend", "bfrag_friend.root");
t->Draw("Bfrag_Central");   // GenPar_* 등과 함께 바로 사용 가능
```

MiniAOD weight 가지 이름이 버전마다 다르면 `--mini-weight-map`,
event-id 가지 이름이 다르면 `--mini-id-branches`로 재지정한다. GenCatTree는
절대 수정되지 않는다 (friend는 별도 파일).

**검증**: 셔플된 순서·누락 event 혼합 입력으로 1:1 정렬이 정확함을 확인했다
(matched는 원본 weight, unmatched는 기본값, 엔트리 정렬 오차 0).

## 9. 검증 기록

### 9.1 1000-event sanity check (수행 완료)

샘플: `/TTToSemiLeptonic_TuneCP5_13TeV-powheg-pythia8/RunIISummer20UL17NanoAODv9` (xrootd 경유 2 file)

```
total events  : 1000
signal (ttbar): 1000  (100%)        ← inclusive ttbar가 아니라도 t/t̄는 항상 존재
Channel_Idx:
  l+jets (e)  : 344
  l+jets (mu) : 330
  tau-involved: 326
  dilepton/all-hadronic: 0           ← SemiLeptonic generator filter와 정확히 일치
```

**해석**: SemiLeptonic 샘플은 정확히 한 W만 leptonic으로 강제. e:μ:τ ≈ 1:1:1 (344:330:326)이 lepton universality와 일치. dilepton/all-had이 0인 것은 filter가 작동함을 확인.

### 9.2 신규 샘플 첫 실행 시 체크리스트

1. signal fraction — ttbar 샘플이면 ~100%
2. channel 분포가 generator filter와 일치하는가 (inclusive: 약 46% all-had, 30% ℓ+jets(eμ), 5% dilepton(eμ), ~20% τ-involved)
3. `SelectedIdx[2..11]`이 signal 이벤트에서 모두 ≥0
4. `validation/checkGenIndex.py`의 pdgId 정합성 테스트 100%
5. GenTop_pt 분포 — 0-50 GeV 피크, TeV까지 tail

---

## 10. HTCondor 대량 처리

`condor/` 디렉토리가 전체 인프라를 포함. 자세한 것은 `condor/README.md` 참고.

```
대상      : CERN lxplus → HTCondor
환경      : CMSSW_14_2_1 (el8_amd64_gcc12) — AFS release area에서 cmsenv
            /afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer
Worker OS : EL8 — submit.jdl의 MY.WantOS = "el8" 로 직접 지정
            (runJob.sh 안에서 cmssw-el8 컨테이너 wrapper 불필요)
빌드      : lxplus(EL9)에서는 cmssw-el8 컨테이너 진입 후 1회 빌드
입력      : DAS query → xrootd (root://cms-xrd-global.cern.ch/)
출력      : EOS (root://eosuser.cern.ch//eos/user/j/junghyun/ttbar_gencat/2017/)
chunk     : 8 files/job, ~10-30분/job, 2GB RAM
데이터셋  : 22개 (nominal 3 + hdamp 4 + mtop 4 + tune 4 + CR 6 + Herwig 2 — 2017 UL)
```

```bash
# 1회 빌드 (lxplus, EL8 컨테이너 안에서)
cmssw-el8
cd /afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer
cmsenv && make clean && make
exit

# 제출 (일반 EL9 셸에서)
cd condor && ./submit_all.sh        # binary check → proxy → filelists → submit
condor_q $USER                       # 모니터링
./resubmit_failed.sh                 # 실패 chunk만 재제출
```

---

## 부록 A: 빌드 및 실행 빠른 참조

```bash
# 환경 (lxplus) — CMSSW_14_2_1은 EL8 빌드이므로 컨테이너 진입 필요
cmssw-el8
source /cvmfs/cms.cern.ch/cmsset_default.sh
cd /afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer
cmsenv

# 빌드
make

# 테스트 실행 (input/ 아래 filelist, 1000 이벤트)
./TopCPVGenCategorizer ttbar_files.txt sanity.root "" 1000

# 출력 확인
root -l sanity.root
root [0] GenCatTree->Draw("Channel_Idx")
root [1] GenCatTree->Draw("GenTop_pt", "isSignal")
root [2] GenCatTree->Draw("SelectedIdx[2]")        // gen index 분포 (top)
```

## 부록 B: 파일 구조

```
TopCPVGenCategorizer/
├── README.md                       # 간단 사용법
├── Makefile
├── main_gencat.cpp                 # Mode A 진입점
├── interface/TopCPVGenCategorizer.h   # 클래스 선언 + statusFlags 비트 정의
├── src/TopCPVGenCategorizer.cpp       # 구현 (~900줄)
├── input/                          # filelist 위치 (런타임)
├── validation/
│   └── checkGenIndex.py            # gen index 검증 스크립트
├── docs/
│   ├── TECHNICAL.md                # 이 문서
│   └── GenIndex_validation.md      # gen index 전용 가이드
└── condor/                         # HTCondor 인프라 (8 파일)
```
