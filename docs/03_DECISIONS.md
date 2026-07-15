# 03 — DECISIONS

> 형식은 NtupleForge `docs/03_DECISIONS.md`를 따른다. 이 패키지 고유 결정만
> 기록하고, 양쪽 공통 로직 결정(예: background 재구축)은 NtupleForge 쪽
> 결정을 원본으로 참조한다.

## D-2026-07-15-datasets-labels — 캠페인 datasets 라벨 = NtupleForge YAML 키

`condor/datasets.txt`의 short_name을 NtupleForge 제출 YAML의 dataset 키와
1:1로 맞춘다 (예: `TTTo2L2Nu_2017` → `TTTo2L2Nu_TuneCP5_powheg`). 이유:
validate 캠페인에서 filelists/출력 디렉토리/리포트가 양쪽에서 같은 이름으로
정렬되어 대조 부기가 자명해짐. 트레이드오프: 구 라벨로 만든 condor 출력
디렉토리와 이름이 달라짐 — v1.10 이전 산출물을 섞어 쓰지 말 것.

## D-2026-07-11-rename-scope — 패키지 rename 시 외부 이름 보존

`SSBGenCategorizer` → `TopCPVGenCategorizer` rename(v1.9)은 우리 산출물의
식별자만 대상. MiniAOD 원본 프레임워크의 실제 코드명(`SSBAnalyzer`,
`SSBTree`, `SSBCorrections`, `SSBCPVCalc`)과 그 파일 예시(`ssb_miniaod*.root`)는
레퍼런스 추적성을 위해 verbatim 유지. (NtupleForge
D-2026-07-01-rename-topcpv와 동일 원칙.)

## D-2026-07-10-background-sync — 로직 변경은 항상 양쪽 동시

카테고리 로직(선택/채널/에너지 규칙) 변경은 NtupleForge 모듈과 이 standalone에
**동시에** 적용하고 교차 검증 하니스로 등가를 증명한 뒤 릴리스한다.
`validate_topcpvcat.py`가 증명하는 것은 "모듈 ≡ standalone"이므로, 한쪽만
고치면 검증 자체가 무의미해짐. 원본 결정: NtupleForge
D-2026-07-10-background-hardprocess, A14.

## D-name-stability — GenCatTree 출력 포맷 불변 보장

트리명 `GenCatTree`, 브랜치명, `(run, luminosityBlock, event)` join key는
v1.5 이후 고정. 패키지/클래스 rename(v1.9)에서도 유지했고, 앞으로의 변경은
`validate_topcpvcat.py`·`plotGenCat.py`·`makeBfragFriend.py`와 기존 산출물
호환성을 깨므로 CHANGELOG에 breaking으로 명시하고 major 버전을 올린다.
