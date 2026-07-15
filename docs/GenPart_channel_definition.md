<!--
  이 문서는 NanoAOD GenPart의 구성 방식과 ttbar channel index 정의를 정리한
  레퍼런스이다. 내용은 CMSSW 공식 소스(genparticles_cff.py, GenStatusFlags.h,
  nanoDQM_cfi.py)와 대조해 검증되었다 (statusFlags 비트 매핑, finalGenParticles
  pruning rule, channel_idx/channel_idx_final 정의 모두 일치 확인).

  이 프로젝트(TopCPVGenCategorizer)와의 매핑:
    - 본문의 channel_idx       ↔  GenCatTree 브랜치  Channel_Idx
    - 본문의 channel_idx_final ↔  GenCatTree 브랜치  Channel_Idx_Final
  구현 세부(코드 알고리즘, 12-slot 배열, FindTopAntiTop, ComputeChannel*)는
  docs/TECHNICAL.md를, statusFlags 비트 활용은 TECHNICAL.md §2.1을 참조.

  주의: §4의 finalGenParticles pruning rule은 CMSSW master 기준 예시이다. 실제
  분석에 쓴 UL NanoAODv9의 정확한 rule은 해당 production의 CMSSW release tag에서
  확인할 것 (§3 참조).
-->

# CMS NanoAOD `GenPart`와 ttbar channel index 정의 정리

작성일: 2026-06-19  
목적: NanoAOD `GenPart`를 이용해 ttbar decay channel, 특히 `W -> tau nu` 이후 tau decay를 어디까지 볼 것인지 정리하고, CMSSW NanoAOD 코드에서 `GenPart`가 어떻게 만들어지는지 확인한다.

---

## 1. 한 줄 요약

`GenPart`는 **generator event record 전체가 아니라, NanoAOD에 저장할 가치가 있는 “interesting gen particles”만 추린 collection**이다.  
따라서 `GenPart`는 ttbar의 hard-process decay topology, 예를 들어

```text
t tbar -> W+ b W- bbar
W -> e nu / mu nu / tau nu / q q'
tau -> e/mu/hadrons + neutrinos
```

를 확인하기에는 보통 충분하다.  
하지만 hadronization 이후 detector simulation 직전의 모든 stable hadron, 예를 들어 일반 QCD jet 안의 모든 pion/kaon/proton constituent를 다 보려는 용도로는 `GenPart`만으로는 부족하다. 그런 목적이면 MiniAOD의 `packedGenParticles`, `prunedGenParticles`, 또는 GEN/SIM level 정보가 필요하다.

---

## 2. 핵심 용어 구분

### 2.1 LHE level

LHE는 보통 matrix element / hard process record에 가까운 정보이다. Parton shower와 hadronization 이전의 parton-level hard scattering 정보를 담는다.

NanoAOD에서는 `LHEPart_*`, `LHE_*` 계열 branch가 이 범주에 가깝다.

중요한 점:

```text
GenPart final != LHE final
```

`GenPart`의 `status == 1`, `isLastCopy`, `fromHardProcess` 같은 개념은 generator particle record 안에서의 상태 또는 copy 정보를 말하는 것이지, LHE 그 자체를 의미하지 않는다.

### 2.2 `GenPart` final / last copy

`GenPart_status == 1`은 CMSSW NanoAOD table 정의에서 “stable” particle status로 설명되어 있다.  
`GenPart_statusFlags`의 `isLastCopy`는 같은 particle이 shower/FSR/decay record에서 여러 copy로 반복될 때 마지막 copy를 고르기 위한 flag이다.

따라서 `GenPart`에서 말하는 final 또는 last copy는 보통 다음 중 하나를 의미한다.

```text
status == 1
  -> generator-level stable particle

statusFlags().isLastCopy()
  -> 같은 particle copy chain에서 마지막 copy

fromHardProcess / isHardProcess
  -> hard-process와 관련된 particle인지 표시
```

### 2.3 Hadronization 이후 정보

Hadronization 이후에는 quark/gluon이 직접 detector에 들어가는 것이 아니라 많은 hadron으로 바뀐다.

```text
q/g -> parton shower -> hadronization -> pi, K, p, n, gamma, ...
```

하지만 NanoAOD `GenPart`가 이 hadron들을 전부 저장하는 것은 아니다. `GenPart`는 pruned collection이며, tau decay chain, leptons, neutrinos, B/C hadrons, hard-process summary, high-pT partons before hadronization 등 일부 중요한 정보만 남긴다.

---

## 3. CMSSW NanoAOD 코드에서 `GenPart`가 만들어지는 방식

주요 코드 위치:

- CMSSW NanoAOD gen particle 설정  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/NanoAOD/python/genparticles_cff.py

주의: 위 링크는 `master` branch이다. 실제 분석에서 사용한 NanoAOD 버전에 정확히 맞추려면 해당 production에 사용된 CMSSW release tag를 확인해서 같은 파일을 보아야 한다. 예를 들어 UL NanoAODv9이면 해당 UL production release에 맞는 CMSSW tag를 확인하는 것이 가장 안전하다.

---

## 4. `finalGenParticles`: `GenPart`의 입력 collection

CMSSW NanoAOD 설정에서 `GenPart` table의 입력은 `finalGenParticles`이다.

관련 코드:

```python
finalGenParticles = cms.EDProducer("GenParticlePruner",
    src = cms.InputTag("prunedGenParticles"),
    select = cms.vstring(
        "drop *",
        "keep++ abs(pdgId) == 15 & (pt > 15 || isPromptDecayed() )",
        "keep+ abs(pdgId) == 15 ",
        "+keep pdgId == 22 && status == 1 && (pt > 10 || isPromptFinalState())",
        "+keep abs(pdgId) == 11 || abs(pdgId) == 13 || abs(pdgId) == 15",
        "keep (400 < abs(pdgId) < 600) || (4000 < abs(pdgId) < 6000)",
        "keep abs(pdgId) == 12 || abs(pdgId) == 14 || abs(pdgId) == 16",
        "keep status == 3 || (status > 20 && status < 30)",
        "keep isHardProcess() || fromHardProcessDecayed() || fromHardProcessFinalState() || (statusFlags().fromHardProcess() && statusFlags().isLastCopy())",
        "keep (status > 70 && status < 80 && pt > 15) ",
        "keep abs(pdgId) == 23 || abs(pdgId) == 24 || abs(pdgId) == 25 || abs(pdgId) == 37 ",
    )
)
```

코드 링크:

- `finalGenParticles` 정의 및 keep/drop rule:  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/NanoAOD/python/genparticles_cff.py#L426-L459

핵심 내용:

1. 처음에 `drop *`를 한다.  
   즉 모든 gen particle을 그대로 저장하는 것이 아니다.

2. 그 뒤 특정 조건에 해당하는 particle만 다시 keep한다.

3. tau는 특별히 중요하게 취급된다.

   코드 주석에 다음 의미가 들어 있다.

   ```text
   keep full tau decay chain for some taus
   keep first gen decay product for all tau
   ```

   따라서 `W -> tau nu`, `tau -> pi nu`, `tau -> e nu nu`, `tau -> mu nu nu` 같은 chain을 보존하기 위한 정보가 `GenPart`에 남을 수 있다.

4. leptons, neutrinos, B/C hadrons, matrix-element summary, hard-process summary, high-pT partons before hadronization, W/Z/H 같은 boson을 선택적으로 keep한다.

---

## 5. `GenPart` table 정의

`GenPart` branch는 `genParticleTable`에서 만들어진다.

관련 코드:

```python
genParticleTable = simpleGenParticleFlatTableProducer.clone(
    src = cms.InputTag("finalGenParticles"),
    name = cms.string("GenPart"),
    doc = cms.string("interesting gen particles "),
    variables = cms.PSet(
        pt = Var("pt", float, precision=8),
        phi = Var("phi", float, precision=8),
        eta = Var("eta", float, precision=8),
        pdgId = Var("pdgId", int, doc="PDG id"),
        status = Var("status", int, doc="Particle status. 1=stable"),
        genPartIdxMother = Var("?numberOfMothers>0?motherRef(0).key():-1", "int16", doc="index of the mother particle"),
        statusFlags = ...
    )
)
```

코드 링크:

- `GenPart` table 생성:  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/NanoAOD/python/genparticles_cff.py#L464-L491

여기서 중요한 branch는 다음이다.

| Branch | 의미 | ttbar channel 분류에서의 용도 |
|---|---|---|
| `nGenPart` | event 안의 GenPart 개수 | loop 범위 |
| `GenPart_pdgId` | PDG ID | top/W/b/lepton/quark/tau 식별 |
| `GenPart_pt`, `eta`, `phi`, `mass` | 4-vector 구성 요소 | matching, acceptance, validation |
| `GenPart_status` | generator status. `1=stable` | stable particle 여부 확인 |
| `GenPart_genPartIdxMother` | mother particle의 `GenPart` index | decay chain 추적 |
| `GenPart_statusFlags` | prompt/hard-process/last-copy/tau-decay 관련 bit flags | duplicate copy 제거, hard-process 선택, tau decay product 구분 |

---

## 6. `GenPart_statusFlags` bit 정리

CMSSW 코드에서 `statusFlags`는 bitwise integer로 저장된다.

코드 링크:

- `statusFlags` bit 구성:  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/NanoAOD/python/genparticles_cff.py#L492-L551

CMSSW 코드 기준 bit mapping은 다음과 같다.

| Bit | Flag | 의미 |
|---:|---|---|
| 0 | `isPrompt` | prompt particle |
| 1 | `isDecayedLeptonHadron` | decayed lepton/hadron |
| 2 | `isTauDecayProduct` | tau decay product |
| 3 | `isPromptTauDecayProduct` | prompt tau decay product |
| 4 | `isDirectTauDecayProduct` | direct tau daughter |
| 5 | `isDirectPromptTauDecayProduct` | direct prompt tau daughter |
| 6 | `isDirectHadronDecayProduct` | direct hadron decay product |
| 7 | `isHardProcess` | hard-process particle |
| 8 | `fromHardProcess` | hard-process에서 유래 |
| 9 | `isHardProcessTauDecayProduct` | hard-process tau의 decay product |
| 10 | `isDirectHardProcessTauDecayProduct` | hard-process tau의 direct decay product |
| 11 | `fromHardProcessBeforeFSR` | FSR 이전 hard-process 유래 |
| 12 | `isFirstCopy` | first copy |
| 13 | `isLastCopy` | last copy |
| 14 | `isLastCopyBeforeFSR` | FSR 이전 last copy |

NanoAODTools에서는 `Object.statusflag(flag)` helper로 해당 bit를 확인할 수 있다.

코드 링크:

- NanoAODTools `statusflag` helper:  
  https://github.com/cms-nanoAOD/nanoAOD-tools/blob/master/python/postprocessing/framework/datamodel.py#L719-L723

예시:

```python
# NanoAODTools 스타일
p.statusflag("isLastCopy")
p.statusflag("fromHardProcess")
p.statusflag("isDirectHardProcessTauDecayProduct")
```

직접 bit operation을 할 때는 다음처럼 볼 수 있다.

```cpp
bool isLastCopy = GenPart_statusFlags[i] & (1 << 13);
bool fromHardProcess = GenPart_statusFlags[i] & (1 << 8);
bool isHardProcess = GenPart_statusFlags[i] & (1 << 7);
bool isHardProcessTauDecayProduct = GenPart_statusFlags[i] & (1 << 9);
bool isDirectHardProcessTauDecayProduct = GenPart_statusFlags[i] & (1 << 10);
```

---

## 7. ttbar channel 분류에 `GenPart`가 충분한 이유

분류하려는 것이 다음 수준이라면 `GenPart`로 충분한 경우가 많다.

```text
t -> W b
W -> e nu
W -> mu nu
W -> tau nu
W -> q q'
tau -> e nu nu
tau -> mu nu nu
tau -> hadrons + nu
```

이유는 다음과 같다.

1. top, W, b, lepton, neutrino, tau는 NanoAOD `GenPart` keep rule에 걸릴 가능성이 높다.
2. hard-process particle은 `isHardProcess`, `fromHardProcess`, `isLastCopy` 등의 flag로 찾을 수 있다.
3. tau decay chain은 별도로 keep rule이 들어가 있다.
4. mother index가 저장되어 있어서 decay chain traversal이 가능하다.

따라서 `ttbar dileptonic / semileptonic / all-hadronic` 같은 channel 분류, 또는 `tau`를 포함한 W decay flavor 분류에는 `GenPart`가 적합하다.

---

## 8. `channel_idx`와 `channel_idx_final`의 차이

### 8.1 `channel_idx`: W-decay-level definition

`channel_idx`를 tau까지만 보는 정의로 둔다면 이는 보통 다음처럼 설명하는 것이 가장 정확하다.

```text
The ttbar decay channel is classified at the W-boson decay level.
Tau leptons from W decays are treated as leptonic W decays,
without resolving the subsequent tau decay.
```

한국어로는:

```text
channel_idx는 W decay level에서의 ttbar channel 정의이다.
즉 W -> tau nu가 나오면 tau의 후속 붕괴를 더 보지 않고
leptonic W decay with tau로 분류한다.
```

예를 들어:

```text
t -> W b
W -> tau nu
tau -> pi nu
```

이 이벤트는 `channel_idx` 기준으로는 다음처럼 분류한다.

```text
W -> tau nu
=> leptonic W decay, tau channel
```

여기서 tau가 hadronically decay했는지는 보지 않는다.

### 8.2 `channel_idx_final`: after tau decay definition

`channel_idx_final`은 tau decay까지 따라간 뒤 최종 visible final state를 기준으로 분류하는 정의로 두면 된다.

```text
The ttbar decay channel is classified after resolving tau decays.
Tau leptons are followed to their decay products,
so tau -> e, tau -> mu, and tau -> hadrons are distinguished.
```

한국어로는:

```text
channel_idx_final은 tau decay를 따라간 뒤의 generator-level final-state 정의이다.
따라서 W -> tau nu, tau -> hadrons + nu는 W decay 기준으로는 leptonic tau이지만,
final-state 기준으로는 hadronic tau로 분류된다.
```

예시:

| Generator chain | `channel_idx` | `channel_idx_final` |
|---|---|---|
| `W -> e nu` | electron | electron |
| `W -> mu nu` | muon | muon |
| `W -> tau nu`, `tau -> e nu nu` | tau | electron-from-tau |
| `W -> tau nu`, `tau -> mu nu nu` | tau | muon-from-tau |
| `W -> tau nu`, `tau -> pi/K/... + nu` | tau | hadronic-tau |
| `W -> q q'` | hadronic W | hadronic W |

---

## 9. 코드가 “여기까지만 본다”를 어떻게 구현하는가

`GenPart` 자체가 “여기까지만 봐라”라고 알려주는 것이 아니다.  
코드가 mother-daughter chain을 어디서 멈출지 정한다.

### 9.1 W decay level에서 멈추는 코드

`channel_idx`는 W daughter까지만 보고 tau에서 멈추면 된다.

```cpp
// channel_idx: W decay level
// W daughter가 tau면 tau decay product를 더 보지 않고 TAU로 분류한다.

int classifyWDaughterLevel(int wIdx) {
    std::vector<int> daughters = findDaughters(wIdx);

    for (int dIdx : daughters) {
        int id = std::abs(GenPart_pdgId[dIdx]);

        if (id == 11) return ELECTRON;
        if (id == 13) return MUON;
        if (id == 15) return TAU;      // 여기서 멈춘다
        if (id >= 1 && id <= 5) return HADRONIC_W;
    }

    return UNKNOWN;
}
```

이 정의에서는 다음 chain을 더 내려가지 않는다.

```text
W -> tau nu
     tau -> pi nu
```

즉 `tau -> pi nu`는 무시하고 `W -> tau nu`까지만 본다.

### 9.2 Tau decay까지 따라가는 코드

`channel_idx_final`은 W daughter가 tau일 때 tau daughter를 한 번 더 따라간다.

```cpp
// channel_idx_final: after tau decay

int classifyAfterTauDecay(int wIdx) {
    std::vector<int> daughters = findDaughters(wIdx);

    for (int dIdx : daughters) {
        int id = std::abs(GenPart_pdgId[dIdx]);

        if (id == 11) return ELECTRON;
        if (id == 13) return MUON;

        if (id == 15) {
            return classifyTauDecay(dIdx);  // tau daughter까지 내려간다
        }

        if (id >= 1 && id <= 5) return HADRONIC_W;
    }

    return UNKNOWN;
}

int classifyTauDecay(int tauIdx) {
    std::vector<int> tauDaughters = findDaughtersRecursive(tauIdx);

    bool hasElectron = false;
    bool hasMuon = false;
    bool hasVisibleHadron = false;

    for (int dIdx : tauDaughters) {
        int id = std::abs(GenPart_pdgId[dIdx]);

        if (id == 11) hasElectron = true;
        if (id == 13) hasMuon = true;

        // charged pion, neutral pion, kaon, proton, etc.
        if (isVisibleHadron(id)) hasVisibleHadron = true;
    }

    if (hasElectron) return TAU_TO_E;
    if (hasMuon) return TAU_TO_MU;
    if (hasVisibleHadron) return TAU_TO_HAD;

    return TAU_UNKNOWN;
}
```

이 차이가 곧 `channel_idx`와 `channel_idx_final`의 물리적 의미 차이이다.

---

## 10. Hadronization 이후 particle을 `GenPart`에서 모두 볼 수 있는가?

아니다. 이 점이 가장 중요하다.

`GenPart`에 pion/kaon이 보일 수는 있다. 하지만 모든 pion/kaon/proton이 저장된다는 뜻은 아니다.

정확히는 다음과 같다.

```text
GenPart에 pi/K가 절대 없는 것은 아니다.

tau decay chain 안의 pi/K,
B/C hadron 관련 particle,
특정 keep rule에 걸린 particle은 보일 수 있다.

하지만 generic QCD hadronization에서 나온 모든 soft pion/kaon/proton이
NanoAOD GenPart에 전부 저장되는 것은 아니다.
```

예를 들어 다음 chain에서는 tau decay chain 때문에 pion이 보일 수 있다.

```text
W -> tau nu
tau -> pi nu
```

하지만 다음처럼 light quark가 shower/hadronization을 거쳐 만든 모든 hadron constituent가 `GenPart`에 전부 들어간다고 보면 안 된다.

```text
W -> q q'
q -> shower -> hadronization -> many pi/K/p/...
```

이런 정보는 보통 다음 수준에서 다루어야 한다.

| 목적 | 권장 collection |
|---|---|
| ttbar hard-process decay topology | NanoAOD `GenPart` |
| tau decay mode 확인 | NanoAOD `GenPart`, 또는 `GenVisTau`가 있으면 함께 사용 |
| gen-level jet kinematics | NanoAOD `GenJet` |
| jet flavor 확인 | `GenJet_hadronFlavour`, `GenJet_partonFlavour` |
| jet constituent 수준의 stable particle 확인 | MiniAOD `packedGenParticles` 또는 GEN/SIM |
| 모든 hadronization product 보존 | NanoAOD 기본 branch로는 부족, custom NanoAOD 필요 가능 |

---

## 11. GenJet, hadronFlavour, partonFlavour와의 관계

Hadronization 이후 정보를 완전히 particle-by-particle로 저장하지 않더라도, NanoAOD에는 jet 단위로 요약된 generator 정보가 있다.

대표적으로:

```text
GenJet_*
Jet_genJetIdx
Jet_hadronFlavour
Jet_partonFlavour
```

`hadronFlavour`는 b/c hadron이 ghost association으로 jet 안에 들어갔는지를 기반으로 정의된다. CMSSW `JetFlavourClustering` 코드는 ghost hadron/parton을 jet constituent와 함께 clustering해서 jet flavor를 정한다. 주석상 b ghost hadron이 있으면 `hadronFlavour = 5`, c ghost hadron이 있고 b가 없으면 `hadronFlavour = 4`, b/c hadron이 없으면 light flavor로 둔다.

코드 링크:

- CMSSW `JetFlavourClustering.cc`:  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/JetMCAlgos/plugins/JetFlavourClustering.cc#L1771-L1824

따라서 hadronization 정보를 전부 branch로 펼치지는 않아도, jet flavor 판정에는 hadron-level 정보가 요약되어 들어간다.

---

## 12. ttbar channel index 작성 시 권장 로직

### 12.1 기본 추천

1. `GenPart`에서 top quark 또는 W boson을 찾는다.
2. duplicate copy 문제를 피하기 위해 가능하면 `isLastCopy` 또는 hard-process 관련 flag를 사용한다.
3. W daughter를 찾는다.
4. `channel_idx`는 W daughter가 tau이면 거기서 멈춘다.
5. `channel_idx_final`은 W daughter가 tau이면 tau decay product까지 따라간다.
6. event 안의 두 W decay를 조합해서 ttbar channel을 결정한다.

예시 분류:

```text
W1 = hadronic, W2 = hadronic -> all-hadronic
W1 = e/mu/tau, W2 = hadronic -> semileptonic
W1 = e/mu/tau, W2 = e/mu/tau -> dileptonic
```

`channel_idx_final`에서는 tau decay가 다음처럼 바뀔 수 있다.

```text
tau -> e    -> electron final state
tau -> mu   -> muon final state
tau -> had  -> hadronic tau final state
```

### 12.2 W daughter 탐색 시 주의점

Generator record에는 같은 particle이 여러 copy로 나타날 수 있다. 따라서 단순히 `pdgId == 24`인 W를 모두 loop하면 중복 counting이 생길 수 있다.

권장 방어 로직:

```cpp
bool isLastCopy(int i) {
    return GenPart_statusFlags[i] & (1 << 13);
}

bool fromHardProcess(int i) {
    return GenPart_statusFlags[i] & (1 << 8);
}

bool isHardProcess(int i) {
    return GenPart_statusFlags[i] & (1 << 7);
}

bool isGoodWFromTop(int i) {
    if (std::abs(GenPart_pdgId[i]) != 24) return false;
    if (!isLastCopy(i)) return false;
    // 필요하면 mother chain을 따라 top에서 왔는지 확인
    return true;
}
```

실제 분석 코드에서는 다음도 검증하는 것이 좋다.

```text
- W가 정확히 두 개 선택되는가?
- W daughter 중 neutrino와 charged lepton/quark가 제대로 보이는가?
- tau chain을 따라갈 때 infinite loop 또는 duplicate copy를 피하는가?
- top/W copy 선택 기준을 바꾸어도 event category가 안정적인가?
```

---

## 13. 문서/발표에서 쓰기 좋은 표현

### 13.1 `channel_idx`

```text
The channel_idx is defined at the W-boson decay level. In this definition,
tau leptons from W decays are treated as leptonic W decays, and the subsequent
tau decay is not resolved.
```

한국어:

```text
channel_idx는 W-boson decay level에서 정의한다. 이 정의에서는 W에서 나온 tau를
leptonic W decay로 취급하며, tau의 후속 붕괴는 분해하지 않는다.
```

### 13.2 `channel_idx_final`

```text
The channel_idx_final is defined after resolving tau decays at generator level.
Tau leptons from W decays are followed to their decay products, so tau -> e,
tau -> mu, and tau -> hadrons are distinguished.
```

한국어:

```text
channel_idx_final은 generator level에서 tau decay를 따라간 뒤 정의한다. 따라서
W에서 나온 tau가 electron, muon, 또는 hadronic tau로 붕괴했는지를 구분한다.
```

### 13.3 `GenPart`의 한계

```text
NanoAOD GenPart is sufficient for classifying the hard-process ttbar decay
topology, including W-boson daughters and tau decay chains. However, it is a
pruned collection of interesting generator particles, not the complete list of
stable particles after hadronization. Therefore, it should not be interpreted
as the full set of particles entering detector simulation.
```

한국어:

```text
NanoAOD GenPart는 W daughter와 tau decay chain을 포함한 ttbar hard-process decay
topology를 분류하기에는 충분하다. 그러나 GenPart는 interesting generator particles만
남긴 pruned collection이지, hadronization 이후 detector simulation에 들어가는 모든
stable particle의 전체 목록은 아니다.
```

---

## 14. 실전 체크 명령어

### 14.1 NanoAOD branch 확인

```bash
edmDumpEventContent input.root | grep GenPart
```

또는 ROOT/uproot에서:

```python
import uproot
f = uproot.open("input.root")
events = f["Events"]
for k in events.keys():
    if "GenPart" in k:
        print(k)
```

### 14.2 CMSSW release 확인

```bash
echo $CMSSW_VERSION
echo $SCRAM_ARCH
```

### 14.3 설치 가능한 CMSSW 확인

```bash
scram list CMSSW
```

### 14.4 현재 release의 NanoAOD 설정 보기

CMSSW area 안에서:

```bash
cd $CMSSW_BASE/src
ls $CMSSW_RELEASE_BASE/src/PhysicsTools/NanoAOD/python/genparticles_cff.py
```

또는 직접 grep:

```bash
grep -n "finalGenParticles" $CMSSW_RELEASE_BASE/src/PhysicsTools/NanoAOD/python/genparticles_cff.py
grep -n "genParticleTable" $CMSSW_RELEASE_BASE/src/PhysicsTools/NanoAOD/python/genparticles_cff.py
grep -n "statusFlags" $CMSSW_RELEASE_BASE/src/PhysicsTools/NanoAOD/python/genparticles_cff.py
```

---

## 15. 결론

1. `GenPart`는 ttbar decay channel 분류에는 적합하다.
2. `channel_idx`는 W decay level로 정의하는 것이 좋다.
3. `channel_idx_final`은 tau decay를 따라간 generator-level final-state definition으로 정의하는 것이 좋다.
4. `GenPart final`은 LHE final이 아니다.
5. `GenPart`는 hadronization 이후 모든 particle을 담지 않는다.
6. 일반 QCD jet의 모든 pion/kaon/proton constituent를 보려면 MiniAOD `packedGenParticles` 또는 custom NanoAOD가 필요하다.
7. 하지만 tau decay chain, B/C hadron, hard-process summary, W/Z/H/top/lepton/neutrino 같은 분석상 중요한 gen-level particle은 NanoAOD `GenPart`에 보존되도록 설계되어 있다.

---

## 16. 참고 링크

- CMSSW NanoAOD `genparticles_cff.py`  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/NanoAOD/python/genparticles_cff.py

- `finalGenParticles` pruning rule  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/NanoAOD/python/genparticles_cff.py#L426-L459

- `GenPart` table definition  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/NanoAOD/python/genparticles_cff.py#L464-L491

- `GenPart_statusFlags` bit mapping  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/NanoAOD/python/genparticles_cff.py#L492-L551

- NanoAODTools `statusflag` helper  
  https://github.com/cms-nanoAOD/nanoAOD-tools/blob/master/python/postprocessing/framework/datamodel.py#L719-L723

- CMSSW `JetFlavourClustering.cc`  
  https://github.com/cms-sw/cmssw/blob/master/PhysicsTools/JetMCAlgos/plugins/JetFlavourClustering.cc#L1771-L1824

- CMS Open Data NanoAOD/MiniAOD 설명  
  https://cms-opendata-workshop.github.io/workshop2024-lesson-exploring-cms-nanoaod/aio.html

- CMS Open Data Jets and MET 설명  
  https://cms-opendata-workshop.github.io/workshop2024-lesson-physics-objects/04-jetmet.html
