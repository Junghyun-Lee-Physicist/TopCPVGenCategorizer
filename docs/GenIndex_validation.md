# Gen Index (GenPar_Idx / SelectedIdx) 검증 가이드

> 상사가 요청한 "gen index 분포 확인"에 대한 전용 문서.

## 1. "Gen index"가 무엇인가

MiniAOD `SSBAnalyzer::GenPar()`의 핵심 출력 두 가지:

1. **`SelectedPar` (내부 12-slot 배열)** → 우리 코드의 `SelectedIdx[12]` branch
   - 각 슬롯이 GenParticle 컬렉션의 몇 번째 입자인지 기록
   - 슬롯 의미: `[p, p, t, t̄, W+, b, W-, b̄, W+d1, W+d2, W-d1, W-d2]`

2. **`GenPar_Idx` (vector branch)** → 우리 코드도 동일 이름으로 출력
   - 저장된 입자 리스트의 각 항목이 가리키는 컬렉션 인덱스
   - Signal 이벤트에서는 SelectedIdx와 동일한 12개 값

상사가 MiniAOD 단계에서 보던 분포는 이 인덱스들의 히스토그램일 가능성이 높다.

## 2. ⚠️ 반드시 알아야 할 것: 분포 모양이 MiniAOD와 다른 게 정상

인덱스는 단순히 **컬렉션 내 위치**다. MiniAOD(prunedGenParticles)와 NanoAOD(GenPart)는 입자 선별(pruning) 정책이 다르므로:

| 항목 | MiniAOD | NanoAOD |
|---|---|---|
| beam proton (index 0,1) | 있음 | **삭제됨** |
| radiation 중간 카피 | 더 많이 보존 | 더 공격적으로 삭제 |
| 전형적 컬렉션 크기 | 수백 개 | 수십~150개 |
| top last copy의 전형적 위치 | ~15-30 | ~5-15 |

**따라서 GenPar_Idx 히스토그램의 절대 위치/모양은 MiniAOD와 다르게 나온다. 이것은 버그가 아니다.**

같아야 하는 것은 인덱스가 *가리키는 물리량*이다:
- 슬롯별 pdgId (slot 2는 반드시 +6, slot 4는 +24, ...)
- top pT, W daughter pT 등 kinematics 분포
- channel 분류 비율

## 3. 검증 절차 (validation/checkGenIndex.py)

```bash
# 출력 파일에 대해 실행
python3 validation/checkGenIndex.py gencat_out.root

# 또는 ROOT 프롬프트에서 수동으로
root -l gencat_out.root
```

스크립트가 검사하는 항목:

### Test 1 — 슬롯 pdgId 정합성 (must be 100%)
```
slot 2  → pdgId +6   (top)
slot 3  → pdgId -6   (anti-top)
slot 4  → pdgId +24  (W+)
slot 5  → pdgId +5   (b)
slot 6  → pdgId -24  (W-)
slot 7  → pdgId -5   (b̄)
```
하나라도 어긋나면 FindLastDaughter 로직 버그.

### Test 2 — 위상 순서 (must be 100%)
NanoAOD GenPart는 위상(topological) 순서로 저장되므로 mother 인덱스 < daughter 인덱스:
```
SelectedIdx[2] (t)  <  SelectedIdx[4] (W+)  <  SelectedIdx[8] (W+ daughter)
```

### Test 3 — 인덱스 분포 히스토그램 (보고용)
슬롯별 `SelectedIdx[k]` 분포를 출력. 기대 패턴:
- slot 2,3 (t, t̄): 작은 인덱스에 집중 (컬렉션 앞부분)
- slot 8-11 (W daughters): 더 큰 인덱스로 퍼짐

### Test 4 — 물리량 비교 (MiniAOD와 직접 비교 가능한 항목)
```
GenTop_pt 분포      ← MiniAOD의 GenTop pT와 일치해야 함
Channel_Idx 비율    ← MiniAOD의 채널 비율과 일치해야 함
```

## 4. ROOT에서 빠르게 그려보기

```cpp
root -l gencat_out.root

// 인덱스 분포 (상사 보고용)
GenCatTree->Draw("SelectedIdx[2]")               // top의 인덱스
GenCatTree->Draw("SelectedIdx[8]")               // W+ daughter의 인덱스
GenCatTree->Draw("GenPar_Idx")                   // 전체 12-slot 인덱스

// 물리 검증 (MiniAOD와 일치해야 하는 것들)
GenCatTree->Draw("GenTop_pt", "isSignal")
GenCatTree->Draw("Channel_Idx")
GenCatTree->Draw("GenPar_pdgId", "GenPar_Idx>=0")
```

## 5. 상사에게 보고할 때 권장 메시지

> "NanoAOD에서 GenPar_Idx / SelectedIdx를 동일한 12-slot 컨벤션으로 재현했습니다.
> 인덱스 절대값 분포는 NanoAOD pruning이 MiniAOD와 달라서 모양이 다르게 나오지만,
> 인덱스가 가리키는 입자의 pdgId 정합성은 100%이고 top pT 등 물리 분포는
> MiniAOD와 일치합니다. 검증 스크립트와 히스토그램 첨부합니다."

