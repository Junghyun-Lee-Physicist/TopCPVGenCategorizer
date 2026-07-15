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

## 의존성


- ROOT (5/6, C++17 컴파일러)
- correctionlib, RoccoR, boost **불필요** — 순수 generator 정보만 사용

## 문서

패키지 문서는 NtupleForge와 동일한 번호 구조를 따른다:

| 문서 | 내용 |
|---|---|
| [docs/01_STATUS.md](docs/01_STATUS.md) | 현재 상태, OPEN 항목, 검증 캠페인 체크리스트 |
| [docs/02_CHANGELOG.md](docs/02_CHANGELOG.md) | 버전 기록 (v1.3 → 현재) |
| [docs/03_DECISIONS.md](docs/03_DECISIONS.md) | 설계 결정 기록 |
| [docs/04_TECHNICAL.md](docs/04_TECHNICAL.md) | 클래스 설계, 동작 모드, B-frag, 검증 상세 |
| [docs/05_GenPart_channel_definition.md](docs/05_GenPart_channel_definition.md) | GenPar/channel 정의 (검증된 레퍼런스) |
| [docs/06_GenIndex_validation.md](docs/06_GenIndex_validation.md) | Gen index 검증 노트 |
| [docs/07_plotGenCat_update_notes.md](docs/07_plotGenCat_update_notes.md) | 플로터 변경 노트 |

컴포넌트별: [condor/README.md](condor/README.md) (mass submission),
[validation/crosscheck/README.md](validation/crosscheck/README.md) (stub-ROOT
교차 검증 하니스), [README_plotGenCat.md](README_plotGenCat.md) (플로터 사용법).
