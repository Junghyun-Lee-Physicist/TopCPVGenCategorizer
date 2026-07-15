# Cross-check harness (v1.8) — ROOT 없이 로직 검증

NtupleForge `topCPVCategorizer`(Python)와 이 standalone C++가 **같은 합성
이벤트에서 같은 파생값**을 내는지 확인하는 하니스. `rootstub/`은 컴파일·실행에
필요한 최소 ROOT 타입 스텁(실제 I/O 없음, plug-in 모드 전용).

```bash
cd <package root>/..   # TopCPVGenCategorizer/ 의 부모 디렉토리에서
g++ -std=c++17 -Wall -Wextra \
    -I TopCPVGenCategorizer/validation/crosscheck/rootstub -I . \
    TopCPVGenCategorizer/src/TopCPVGenCategorizer.cpp \
    TopCPVGenCategorizer/validation/crosscheck/crosscheck_harness.cpp \
    -o crosscheck && ./crosscheck
```

이벤트 3종: E1 ttbar all-hadronic signal(12-slot·Channel_Jets 2112/1212),
E2 explicit-Z Z→ττ(−30, Final 13 — τ 이중카운트 회귀 테스트),
E3 boson-less ME μμ(26 — e/μ 누락 회귀 테스트). Python 쪽 동일 이벤트는
NtupleForge `script/test_reader_lifecycle.py`에 있음.

주의: 진짜 검증(실데이터, byte-identity)은 lxplus에서 실제 ROOT로 빌드한 뒤
`validate_topcpvcat.py`로 수행. 이 하니스는 알고리즘 로직 회귀 테스트용.
