# 01 — STATUS

> 마지막 갱신: 2026-08-17 (v1.10.3). NtupleForge와 동일한 문서 구조.
>
> **2026-08-17 병합:** 2026-07-15 handoff tar 에만 있던 v1.10.1 / v1.10.2 코드가
> git 에 한 번도 들어간 적이 없어 이번에 되살렸다 (`condor/submit_all.sh`,
> `condor/README.md`, `docs/02_CHANGELOG.md`). 반대로 이 문서의 2026-07-26
> D-F rename 문단은 tar 에 없어 그대로 유지했다. 상세: `02_CHANGELOG.md` v1.10.3.

## 현재 상태

- **버전 v1.10.3** — 물리 로직은 v1.10 과 동일. v1.10.1(condor 제출 가드 +
  `-s <TAG>` submit-only), v1.10.2(`MY.JobBatchName`) 는 제출 glue 만 건드린다.
  NtupleForge `topCPVCategorizer` 모듈과 로직 완전 동기
  (background 선택 = MiniAOD §1.6 등가, τ walk, `Channel_Idx_Expanded`,
  A14 `SafeEnergy` sentinel 포함).
- **교차 검증 그린**: stub-ROOT 하니스(`validation/crosscheck/`)에서 합성
  3이벤트(ttbar signal / explicit-Z Z→ττ / boson-less μμ + beam-parallel legs)의
  파생값이 Python 모듈과 완전 일치. g++ 13 `-Wall -Wextra` 클린.
- **출력 포맷 안정**: `GenCatTree` 트리명·브랜치명·event-id join key는 v1.5
  이후 불변 (D-name-stability).
- **datasets**: `condor/datasets.txt` = 2017UL 검증 캠페인 13 샘플
  (NtupleForge 제출 YAML 키와 라벨 1:1 정렬). ttbar systematics 카탈로그는
  `condor/datasets.txt.full`.

## OPEN (lxplus)

0. **⚠️ 미완 작업 (2026-07-15 유실) — CRAB submission path; 새 세션에서 재개.**
   작업 컨테이너에서 파일이 만들어졌으나 packaging 전 리셋으로 소실되었고, 이후
   git 에도 들어가지 않았다. **2026-08-17 확인: `crab/` 디렉토리는 여전히 없다.**
   (원 기록에 함께 적혀 있던 `LICENSE` 는 저장소에 이미 존재하므로 이 항목에서
   제외했다.)
   - **만들었다가 잃은 것** (내용은 2026-07-15 대화에서 복구 가능 —
     past-chats search: `"TopCPVGenCategorizer crab_script PSet gencatNtuple"`):
     - `crab/PSet.py` — `gencatNtuple.root` 를 선언하는 fake PSet
     - `crab/crab_script.py` — worker wrapper: CRAB 이 수정한 PSet 에서 입력을
       읽고 → sandbox 가 평탄화됐으면 `pkg/{interface,src}` 레이아웃을 재구성 →
       `g++ -O2 -std=c++17 $(root-config --cflags/--libs)` 로 컴파일 →
       `/store/...` 에 xrootd prefix 를 붙여 `input/` 밑에 filelist 생성 →
       `./gencat` 실행 → `gencatNtuple.root` 를 남김
     - `crab/submit_crab.py` — `condor/datasets.txt` 항목당 task 1개;
       requestName 은 `[A-Za-z0-9_-]` 로 sanitize, ≤100자;
       `inputFiles=['interface','src','main_gencat.cpp']`; FileBased splitting;
       기본 site `T3_KR_KNU`; `--status/--kill/--dry-run/--only` 모드
   - **이미 내린 결정 (유지할 것):** compile-on-worker (ABI 안전, sandbox 에
     사전 빌드 바이너리를 넣지 않음); 출력 파일명 `gencatNtuple.root` 는 PSet 과
     **반드시** 일치해야 함 (exit-60302 교훈); datasets/labels 는 condor 경로 및
     NtupleForge validation YAML 과 공유
   - **남은 절차:** 파일 재작성 → `crab/README.md` → 문서 반영
     (`01_STATUS`, `02_CHANGELOG` 를 v1.11 로, `03_DECISIONS` 에 compile-on-worker)
     → gates → v1.11 로 패키징
   - **짝 작업:** NtupleForge 쪽 condor 경로 — 그 저장소 `docs/01_STATUS.md`
     OPEN #0

1. **실 ROOT 빌드 + 단일 파일 sanity** — EL8 셸에서 `make` 후 ttbar 파일 1개
   `-N` 소량 실행.
2. **13-샘플 GenCatTree 생산** — `condor/submit_all.sh` (datasets.txt 전체).
   주의: 라벨이 구버전(`TTTo*_2017`)에서 바뀌었으므로 출력 디렉토리명도 바뀜.
   제출은 **호스트 셸**에서 (컨테이너 안엔 `condor_submit` 없음 — v1.10.1 가드);
   2026-07-15 189-job 렌더본이 남아 있다면 `-s 20260715-080617` 로 이어서 제출.
3. **validate_topcpvcat.py 캠페인** — 샘플별 (NtupleForge `forgedNtuple` ↔
   standalone `GenCatTree`) event-matched 비교. int exact / float ftol 1e-4.
   NtupleForge 쪽 산출물 파일명은 **2026-07-26 rename(D-F) 이후 생산분부터**
   `forgedNtuple_*.root` 이다. 그 이전 생산분(2017UL 전부)은
   `slimmedNtuple_*.root` 다. **rename 후 첫 실제 CRAB 생산 = 2026-07-27 의
   ttHH UL18 full 캠페인**(85 task / 7,466 job, 진행 중)이며 그 산출물은
   `forgedNtuple_*.root` 로 나온다 — 즉 두 이름이 Tier-3 에 공존한다. CPV 쪽
   캠페인은 아직 rename 이후 생산이 없다. filelist 생성기들은 두 prefix 를 **모두**
   매칭하므로 어느 쪽이든 동작한다 (NtupleForge `docs/02_CHANGELOG.md` 2026-07-26 (6),
   2026-07-27).
4. **DY Draw sanity** (NtupleForge audit §2b) — `Channel_Idx`가 ±22/26에 서고
   −60 피크가 없는지 1회 확인.
