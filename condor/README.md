# Condor submission — TopCPVGenCategorizer on lxplus

전체 Run-2 ttbar MC 데이터셋을 HTCondor로 처리하고 결과를 EOS에 stage하는 도구 모음.

## v1.3 주요 변경

1. **중앙 config (`config.sh`)** — EOS 출력 위치, dataset 파일, files-per-chunk, 자원 요청, worker OS 등 모든 설정을 한 파일에서 관리.
2. **per-submission 디렉토리** — 제출할 때마다 `submissions/<TAG>/` 디렉토리를 만들고 그 안에 proxy, 렌더링된 JDL, index, filelists, logs를 모두 격리. 여러 번/동시 제출해도 서로 안 섞임.
3. **dry-run 모드 (`-n`)** — DAS 결과·출력 경로·job 수·렌더링된 JDL을 실제 제출 없이 미리 확인.
4. **JDL은 제출 시점에 렌더링** — 정적 submit.jdl 대신 config + submission 경로로 생성.

## 환경 전략

- **CMSSW_14_2_1 release area에서 cmsenv**: `/afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer`
- **Worker OS는 EL8 고정**: JDL의 `MY.WantOS = "el8"` (config의 `WORKER_OS`). 컨테이너 wrapper 불필요.
- **바이너리는 AFS에서 직접 실행**: transfer 불필요, chunk filelist만 transfer.

## 파일 구조

```
condor/
├── config.sh                 # ★ 모든 설정 (여기만 수정)
├── datasets.txt              # dataset 목록 (short_name  DAS_path)
├── makeFilelists.py          # DAS → per-chunk filelists (--dry-run 지원)
├── makeCondorIndex.py        # filelists → Condor queue index
├── runJob.sh                 # worker wrapper (cmsenv 방식)
├── submit_all.sh             # ★ 진입점 (config-driven, -n dry-run, -t tag)
├── checkOutputs.sh           # chunk 상태 분류 (DONE/RUNNING/BAD)
├── resubmit_failed.sh        # BAD chunk만 재제출 (checkOutputs 위임)
├── .gitignore
└── submissions/<TAG>/        # ← 제출 시 자동 생성
    ├── x509up.proxy
    ├── submit.jdl            # 렌더링된 JDL
    ├── index_for_condor.txt
    ├── filelists/
    └── logs/
```

## 사전 준비 (한 번만)

```bash
ssh lxplus.cern.ch

# 코드 배치
cd /afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src
tar xzf ~/TopCPVGenCategorizer_v1.3.tar.gz     # → src/TopCPVGenCategorizer/

# EL8 컨테이너에서 빌드 (lxplus 로그인 노드는 EL9)
cmssw-el8
cd /afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer
cmsenv && make clean && make                # → TopCPVGenCategorizer 바이너리
exit

# EOS 출력 디렉토리
xrdfs root://eosuser.cern.ch mkdir -p /eos/user/j/junghyun/ttbar_gencat/2017
```

## 설정 (config.sh)

제출 전에 `config.sh`를 열어 본인 환경에 맞게 수정:

```bash
CMSSW_AREA="/afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer"
EOS_OUTBASE="/eos/user/j/junghyun/ttbar_gencat/2017"   # ← EOS 출력 위치
DATASETS_FILE="datasets.txt"                            # ← dataset 목록
FILES_PER_CHUNK=8                                       # ← job당 파일 수
WORKER_OS="el8"
JOB_FLAVOUR="workday"
# ... (자세한 것은 config.sh 주석 참고)
```

## 사용법

### 1단계: dry-run으로 미리 확인

```bash
cd condor
./submit_all.sh -n
```

출력으로 다음을 확인할 수 있음:
- 해석된 모든 config 값 (EOS 출력 경로, files/chunk 등)
- dataset별 DAS 파일 수와 chunk 수
- 총 job 수
- 렌더링될 JDL 전문

### 2단계: 실제 제출

```bash
./submit_all.sh                  # tag는 UTC timestamp 자동 생성
# 또는 tag 지정
./submit_all.sh -t myrun_v1
```

제출 결과는 `submissions/<TAG>/`에 격리되어 보관됨.

### 옵션

```
-t TAG     submission tag (기본: UTC timestamp YYYYmmdd-HHMMSS)
-n         dry-run (제출 안 함)
-c CONFIG  대체 config 파일 (기본: config.sh)
```

## 모니터링

```bash
condor_q $USER
tail -f submissions/<TAG>/logs/*.err
```

## 출력 확인

```bash
xrdfs root://eosuser.cern.ch ls -l /eos/user/j/junghyun/ttbar_gencat/2017/
```

## chunk 상태 확인 (checkOutputs.sh)

어떤 chunk가 부족한지 정확히 알려면 `checkOutputs.sh`를 사용합니다. 세 가지
소스를 교차 검증해서 각 chunk를 **DONE / RUNNING / BAD** 로 분류합니다:

1. **예상 목록** — `submissions/<TAG>/index_for_condor.txt` (있어야 할 전체 chunk)
2. **condor job 상태** — `condor_q`로 아직 돌고 있는 chunk 확인 (RUNNING은 건드리지 않음)
3. **EOS 출력 무결성** — 파일 존재 + 크기 + (옵션) ROOT로 열어 TTree 엔트리 확인

```bash
# 기본 (config의 VERIFY_LEVEL 사용, 기본 root)
./checkOutputs.sh -t myrun_v1

# 검증 수준 직접 지정
./checkOutputs.sh -t myrun_v1 -v exists   # 파일 존재만 (가장 빠름)
./checkOutputs.sh -t myrun_v1 -v size     # 존재 + 크기
./checkOutputs.sh -t myrun_v1 -v root     # 존재 + 크기 + ROOT 엔트리 (가장 엄밀)
```

검증 수준 (config.sh의 `VERIFY_LEVEL`):

| level | 검사 내용 | 속도 |
|---|---|---|
| `exists` | EOS에 .root 파일이 있는가 | 빠름 |
| `size` | 위 + 크기 ≥ `MIN_OUTPUT_SIZE` (기본 100 KB) | 빠름 |
| `root` | 위 + ROOT로 열려서 `GenCatTree` 엔트리 > 0 | 느림 (파일마다 ROOT open) |

출력 예시:
```
  Summary for tag 'myrun_v1'  (total expected: 120)
    DONE     : 115   (valid output)
    RUNNING  : 3     (still active in condor — left alone)
    BAD      : 2     (missing/truncated/unreadable, not running)

  BAD chunk detail:
    TOOSMALL  TTToSemiLeptonic_2017/..._chunk017.root (1024B < 102400B)
    MISSING   TTTo2L2Nu_2017/..._chunk003.root
```

BAD chunk 목록은 `submissions/<TAG>/index_bad.txt`에 기록되어 재제출에 쓰입니다.

**왜 단순 "파일 존재" 체크로 부족한가**: job이 xrdcp 도중 죽으면 0바이트/truncated
파일이 올라가는데, 존재만 보면 성공으로 오판합니다. 또 아직 실행 중인 job을
실패로 오인해 중복 제출할 수 있습니다. checkOutputs.sh는 크기/ROOT 검증과
condor 상태 확인으로 이 두 문제를 모두 막습니다.

## 실패한 job 재제출

`resubmit_failed.sh`는 내부적으로 `checkOutputs.sh`를 호출해 BAD로 분류된
chunk만 재제출합니다 (RUNNING은 자동 제외 → 중복 제출 없음).

```bash
# 어떤 chunk가 재제출될지 먼저 확인
./resubmit_failed.sh -t myrun_v1 -n

# BAD chunk만 재제출
./resubmit_failed.sh -t myrun_v1

# 검증 수준 지정 (예: 빠르게 존재만)
./resubmit_failed.sh -t myrun_v1 -v size
```

## Dataset 범위

`datasets.txt`는 ttbar 계열 22종 (nominal 3 + hdamp/mtop/tune/CR/Herwig systematics).
다른 샘플 추가는 `<short_name>  <DAS_path>` 한 줄씩 추가하면 됨.
DAS 경로 확인: `dasgoclient --query "dataset=/TTTo2L2Nu*UL17NanoAODv9*"`

기존에 그룹에서 쓰는 CPV filelist가 있다면, DAS query 대신 그 파일을 직접
chunk로 나누는 모드를 makeFilelists.py에 추가할 수 있음 (현재는 DAS query 방식).

## 자원 사용량 (예상)

chunk당 ~8 파일(~10 GB read, ~50 MB output), 10-30분, 2 GB RAM, 1 CPU.
22 dataset × 10-100 chunks ≈ 총 500-2000 jobs.

## Troubleshooting

| 증상 | 원인 | 해결 |
|---|---|---|
| `[FATAL] cannot submit without binary` | 바이너리 미빌드 | `cmssw-el8` 진입 후 `cmsenv && make` |
| `dasgoclient: command not found` | CMS 환경 미설정 | `source /cvmfs/cms.cern.ch/cmsset_default.sh` |
| 빌드 시 glibc/ABI 에러 | EL9에서 EL8 release 빌드 | `cmssw-el8` 컨테이너 진입 |
| job idle, 매칭 안 됨 | el8 슬롯 대기 | 정상 대기. 오래면 `condor_q -better-analyze` |
| xrdcp permission denied | EOS 디렉토리 없음 | `xrdfs root://eosuser.cern.ch mkdir -p ...` |
| job held, `Disconnected` | proxy 만료 | `voms-proxy-init -voms cms` 후 `condor_release` |

