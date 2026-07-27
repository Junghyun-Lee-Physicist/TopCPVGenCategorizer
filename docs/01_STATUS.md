# 01 — STATUS

> 마지막 갱신: 2026-07-26 (v1.10; NtupleForge D-F rename 반영). NtupleForge와 동일한 문서 구조.

## 현재 상태

- **버전 v1.10** — NtupleForge `topCPVCategorizer` 모듈과 로직 완전 동기
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

1. **실 ROOT 빌드 + 단일 파일 sanity** — EL8 셸에서 `make` 후 ttbar 파일 1개
   `-N` 소량 실행.
2. **13-샘플 GenCatTree 생산** — `condor/submit_all.sh` (datasets.txt 전체).
   주의: 라벨이 구버전(`TTTo*_2017`)에서 바뀌었으므로 출력 디렉토리명도 바뀜.
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
