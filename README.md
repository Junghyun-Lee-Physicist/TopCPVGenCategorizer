# TopCPVGenCategorizer

NanoAOD 기반 generator-level categorizer. MiniAOD `SSBAnalyzer`의 `GenPar()` / `GenJet()` / `GenMET()` / ghost-B 섹션을 모두 NanoAOD에서 수행합니다.

## 디렉토리 구조

```
TopCPVGenCategorizer/                     ← /afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/ 아래 배치
├── interface/TopCPVGenCategorizer.h      # 클래스 선언
├── src/TopCPVGenCategorizer.cpp          # 구현
├── main_gencat.cpp                    # standalone 진입점
├── Makefile                           # → 바이너리 이름: TopCPVGenCategorizer
├── input/                             # filelist 둘 곳 (.txt)
├── validation/checkGenIndex.py        # gen index 검증
├── docs/                              # 기술 문서 + PPT
├── condor/                            # HTCondor submission infrastructure
│   ├── datasets.txt
│   ├── makeFilelists.py
│   ├── makeCondorIndex.py
│   ├── runJob.sh
│   ├── submit.jdl
│   ├── submit_all.sh
│   ├── resubmit_failed.sh
│   └── README.md
└── README.md (이 파일)
```

## 빌드 & 실행 (lxplus)

코드는 CMSSW release 영역에 배치하여 사용:
`/afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer`

CMSSW_14_2_1은 **EL8 빌드**이므로 lxplus(EL9)에서는 빌드 시 컨테이너 진입 필요:

```bash
# 빌드 (1회, EL8 컨테이너 안에서)
cmssw-el8
cd /afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer
cmsenv
make clean && make
exit

# 단일 dataset 테스트 (sanity check, 역시 el8 셸에서)
cmssw-el8
cd /afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer && cmsenv
./TopCPVGenCategorizer ttbar_files.txt sanity.root "" 1000
```

Condor 대량 처리 시에는 worker가 `MY.WantOS = "el8"`로 자동 EL8 환경에서 실행되므로
컨테이너 wrapper가 불필요합니다 (condor/README.md 참고).

## HTCondor mass submission

전체 Run-2 ttbar MC 처리:

```bash
cd condor/
./submit_all.sh             # build + proxy + filelists + submit 한 번에
condor_q $USER              # 모니터링
```

자세한 사용법은 `condor/README.md` 참고.

## 출력 — GenCatTree

다음 branch group이 모두 NanoAOD에서 생성됩니다:

- **GenPar (12-slot 가계도)**: pt, eta, phi, mass, energy, pdgId, status, mother/daughter index
- **GenTop / GenAnTop**: 4-momentum
- **Channel_***: Idx, Idx_Final, Lepton_Count(_Final), Jets, Jets_Abs, Tau_Lepton, Visible_Tau
- **GenJet**: pt/eta/phi/mass/energy + PartonFlavour + HadronFlavour
- **GenMET**: pt, phi
- **GenBJet / GenBHad**: kinematics + FromTopWeakDecay + Flavour
- **PSWeight**: ISR_Up/Down, FSR_Up/Down (NanoAOD의 shower systematic)

## 두 가지 동작 모드

### Mode A — Standalone (이 main_gencat.cpp)
독립 실행파일. selection/correction 없이 generator categorization만 수행.

### Mode B — Analysis 클래스에 plug-in

```cpp
// Analysis.h
TopCPVGenCategorizer* genCat_ = nullptr;

// Analysis 생성자 (MC에서만)
if (!isData) {
    genCat_ = new TopCPVGenCategorizer();
    genCat_->AttachExternal(&uintSingles, &floatSingles,
                            &floatVectors, &intVectors,
                            &boolVectors, &ucharVectors);
}

// Analysis::Loop() 안
if (genCat_) {
    const GenCatResult& r = genCat_->ProcessEvent();
    if (r.channel_idx != 24) continue;
    if (genCat_->HasTauSecondaryLepton()) continue;
}
```

Mode B에서는 host Analysis의 branchlist에 GenPart_*, GenDressedLepton_*, GenVisTau_*, GenJet_*, GenMET_*, PSWeight 등이 포함되어 있어야 합니다.

## B-Fragmentation 노트 (중요)

원본 MiniAOD `SSBAnalyzer`는 custom CMSSW producer `bfragWgtProducer`를 통해 B-fragmentation systematic weight들을 저장했습니다:
- `Frag_Cen_Weight`, `Frag_Up_Weight`, `Frag_Down_Weight`, `Frag_Peterson_Weight`
- `Semilep_BrUp_Weight`, `Semilep_BrDown_Weight`

**이것들은 NanoAOD에 없습니다.** NanoAOD가 제공하는 가장 가까운 변수는 `PSWeight[0..3]` (ISR/FSR up/down)인데 이는 parton-shower 변동으로 fragmentation systematic과는 다른 systematic입니다.

CPV 분석에서 정확한 B-fragmentation systematic이 필요하면 3가지 옵션이 있습니다:
1. PSWeight를 근사 대체로 사용 (지금 코드가 자동 저장)
2. NanoAOD 재생산 (bfragWgtProducer 포함된 custom production)
3. MiniAOD friend tree와 event ID로 join

## 의존성

- ROOT (5/6, C++17 컴파일러)
- correctionlib, RoccoR, boost **불필요** — 순수 generator 정보만 사용

## 검증

1000-event sanity check on `TTToSemiLeptonic_TuneCP5_13TeV-powheg-pythia8`:

```
TopCPVGenCategorizer summary
  total events  : 1000
  signal (ttbar): 1000  (100%)
  Channel_Idx distribution:
    11  l+jets (e)       : 344
    13  l+jets (mu)      : 330
    <0  tau-involved     : 326
    (dilepton/all-had 0 — SemiLeptonic filter)
```

비율이 PDG ttbar branching ratio와 일치하므로 코드가 의도대로 작동합니다.

## 문서

- `docs/TECHNICAL.md` — **전체 기술 문서**: 원본 MiniAOD 로직, NanoAOD 구현 알고리즘, 설계 결정, 변수 매핑, 한계점. 코드 로직 재현에 필요한 모든 정보.
- `docs/GenPart_channel_definition.md` — NanoAOD `GenPart` 구성 방식(`finalGenParticles` pruning)과 `Channel_Idx`/`Channel_Idx_Final`(W-decay level vs τ 분해) 정의 레퍼런스. CMSSW 공식 소스로 검증됨.
- `docs/GenIndex_validation.md` — GenPar_Idx / SelectedIdx 검증 가이드
- `validation/plotGenCat.py` — GenCatTree 검증 플로터 (RDataFrame, overlay PDF 기본). 사용법은 `README_plotGenCat.md`, 변경 이력은 `docs/plotGenCat_update_notes.md` 참조.
- `validation/makeBfragFriend.py` — B-fragmentation weight friend tree 빌더 (아래 참조)
- `validation/checkGenIndex.py.bk` — (구) gen index 검증 스크립트. plotGenCat.py로 대체됨.
- `condor/README.md` — HTCondor 대량 처리 가이드


## B-fragmentation weight 복구 (friend tree)

B-frag weight는 표준 NanoAOD에 없으므로 MiniAOD 산출물에서 (run, lumi, event)로
join해서 가져온다. TopCPVGenCategorizer는 standalone 출력 GenCatTree에 event-id
join key를 자동으로 기록한다. 이후:

```bash
python validation/makeBfragFriend.py \
    --nano gencat_output.root --mini ssb_miniaod.root --out bfrag_friend.root
# 분석에서: t->AddFriend("BfragFriend", "bfrag_friend.root");
```

자세한 내용은 `docs/TECHNICAL.md` §8.3 참조.

## 버전 기록

- **v1.9** — **패키지명 변경: `SSBGenCategorizer` → `TopCPVGenCategorizer`.** 클래스·파일·디렉토리·include guard·`TopCPVGenStatusBit` 네임스페이스·condor 스크립트·문서 접두어 일괄 변경. 원본 MiniAOD 프레임워크의 외부 이름 인용(`SSBAnalyzer`, `SSBTree`, `SSBCorrections`, `SSBCPVCalc`, `ssb_miniaod*.root` 예시)은 실제 코드명이므로 그대로 유지. 출력 포맷은 **불변** — 트리명 `GenCatTree`, 브랜치명, event-id join key 모두 동일하므로 `validate_topcpvcat.py`·`plotGenCat.py`·`makeBfragFriend.py` 사용법과 기존 산출물 호환성에 영향 없음. lxplus에서는 디렉토리명 변경에 따라 새 경로에 배포 후 재컴파일 필요.
- **v1.8** — **MiniAOD 충실도 복원 + NtupleForge 모듈과 동기화** (기준 = MiniAOD `SSBAnalyzer`; NtupleForge `docs/TopCPV/02_faithfulness_vs_miniaod.md` audit 참조). ① `Channel_Idx`를 **전체 selected list**에 대해 합산 — background가 더 이상 0으로 강제되지 않음(MiniAOD §2.1). ② `Channel_Idx_Final`을 `GenDressedLepton` 지름길 대신 **GenPart daughter-map walk**로 재구현(MiniAOD §2.2): τ의 leptonic daughter를 GenPar에 append, `<14/>14` 부호 규칙, ν도 τ 제거를 트리거(hadronic τ는 제거만 되고 대체 없음), dressing/pt 하한 제거. `GenDressedLepton` 브랜치 등록 삭제. ③ `FillBackgroundSelection`을 MiniAOD §1.6 등가로 재구축: base set = `isHardProcess` 입자 전체 + **직접** boson(t/Z/W/H) mother를 가진 status-1/2 lepton — 기존 휴리스틱(last-copy boson + 1단계 딸 + hard-process τ rescue)이 갖던 두 결함(explicit-Z→ττ에서 τ 이중 카운트 −60, boson row 없는 ME ℓℓ에서 e/μ 누락 → 0) 제거. ④ 진단 브랜치 `Channel_Idx_Expanded` 추가(= `Channel_Idx`, 단 isSignal이고 slot 2–11 중 결손 시 −999; `Channel_Idx` 자체는 MiniAOD와 bit-동일 유지) + Loop 요약에 unclassifiable 카운트. ⑤ stub-ROOT 교차 검증 하니스(`validation/crosscheck/`) 추가 — 합성 3이벤트(ttbar signal / Z→ττ / boson-less μμ)에서 C++와 NtupleForge Python 모듈의 파생값 완전 일치 확인(ROOT 불필요, `g++ -std=c++17`만으로 실행).
- **v1.7** — 검증된 `GenPart`/channel 정의 문서(`docs/GenPart_channel_definition.md`) 추가, TECHNICAL.md에 교차참조. 플로터에 top/anti-top의 나머지 4-momentum 성분 추가: `phi`, `energy`, 그리고 계산된 `mass`(m_t=172.5 GeV 피크로 4-vector 조립 검증). `GenPar_role.pptx`의 channel 정의 슬라이드를 W-decay-level vs τ-resolved 구분에 맞게 수정.
- **v1.6** — 검증 플로터(`plotGenCat.py`) 재작성: 개별 플롯 PDF 저장, overlay 기본 / stack은 `--draw-stack` 시에만, `Channel_Idx`·`Channel_Idx_Final`의 named + numeric 두 버전, `--only` 관측량 선택, overlay에도 σ·BR·lumi/N 정규화 적용. 플로터 사용/변경 문서(`README_plotGenCat.md`, `docs/plotGenCat_update_notes.md`) 추가. `datasets.txt`(nominal 3종) / `datasets.txt.full`(전체) 분리. v1.5의 event-id join key + `makeBfragFriend.py`(B-frag friend tree) 포함.
- **v1.5** — GenCatTree에 (run, luminosityBlock, event) join key 기록(standalone). `makeBfragFriend.py`로 MiniAOD B-frag weight를 friend tree로 복구. (§8.3)
- **v1.4** — HTCondor 출력 검증/재제출 (`checkOutputs.sh`, `resubmit_failed.sh`).
- **v1.3** — NanoAOD standalone 전 generator 블록 재현 (GenPar/GenJet/GenMET/ghost-B/channel), HTCondor 대량 처리.
