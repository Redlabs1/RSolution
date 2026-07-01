# RSolution — 반도체 장비 제어 HMI

WinUI 3 + C++/WinRT 데스크톱 애플리케이션. 반도체 제조 장비의 제어/감시 HMI.
상세 설계 근거: `0.Doc/반도체 장비 제어 프로그램(C++) 상세 아키텍처 설계서.docx`

## 빌드

- **반드시 Visual Studio 2026 _Professional_ 로 빌드한다.** 같은 PC의 _Community_ 2026은 C++ 툴셋이 불완전(`vcvarsall.bat`/CRT 헤더/lib 누락)하여 빌드 불가. `vswhere -latest` 는 Community를 고를 수 있으니 경로를 명시할 것.
- 표준: **ISO C++20** (`/std:c++20`, 설계서 6.4.1). 경고 수준 W4.
- 패키지 형태: **비패키지(unpackaged)**. vcxproj에 `AppxPackage=false` + `WindowsPackageType=None` 설정됨 (`Package.appxmanifest` 없음). 추가 `/p:` 플래그 불필요.
- 빌드 명령 (Bash 도구에서 임시 .bat 생성 후 실행):
  ```bat
  set "VS=C:\Program Files\Microsoft Visual Studio\18\Professional"
  call "%VS%\VC\Auxiliary\Build\vcvars64.bat"
  "%VS%\MSBuild\Current\Bin\MSBuild.exe" RSolution\RSolution.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m
  ```
- 산출물: `RSolution\x64\Debug\RSolution\RSolution.exe`

## 코드 컨벤션

- **한글 주석이 있는 .cpp/.h/.xaml 은 UTF-8 BOM 으로 저장한다.** BOM 없으면 MSVC가 코드페이지 949로 오해해 `warning C4819` (또는 한글 깨짐). 새 파일 작성 후 BOM을 붙일 것.
- 비-WinRT C++ 코드의 네임스페이스는 **`rs`** (WinRT 투영 `RSolution`/`winrt::RSolution`과 충돌 방지). 하위: `rs::core`, `rs::domain`, `rs::hal`, `rs::host`.
- WinRT 투영 타입은 `winrt::RSolution::implementation` (구현) / `RSolution`(IDL).
- 모듈 경계는 예외 대신 `rs::core::Result<T>` / `Status` / `Error` 로 변환 (설계서 6.3).
- `core` / `domain` / `hal` / `host` 인터페이스는 **WinRT·벤더 SDK 비의존**으로 유지(어느 계층에서나 사용 가능, 설계서 6.4.4 Adapter 격리).
- `src` 가 include 루트로 등록됨 → 계층 간 include는 `#include "core/..."`, `#include "hal/..."` 형식.

## 디렉터리 구조 (설계서 6.1 기준)

```
src/
  app/                     # 진입점, pch, 모듈 초기화
  core/
    common/                # Result/Error, IEventBus, EventBus 등 공통
    domain/                # EquipmentState/Command/StateMachine, AlarmCode, InterlockClass,
                           # IRecipeRepository/RecipeRepository, IAlarmService/AlarmService
    application/           # (예정) 운전 시나리오, 명령 처리
    infrastructure/
      logging/             # LogManager (다채널 로거)
      data/                # DataManager (JSON 입출력)
  sequence/                # SequenceManager, SequenceRunner(jthread), ISequenceStep,
                           # SequenceContext, TimeoutGuard, steps/(DelayStep, MoveAxisStep)
  hal/                     # IMotionController, IIoProvider, IVisionCamera (장치 추상화 인터페이스)
  simulator/               # SimulatedMotionController/IoProvider/HostGateway (하드웨어 없는 검증)
  host/                    # IHostGateway (SECS/GEM·MES)
  tests/                   # SelfTests — 격리된 자체 단위 테스트(Debug 빌드마다 자동 실행)
  ui_winui/                # WinUI 셸/페이지/뷰모델/서비스
  platform/                # Windows 플랫폼 코드
  (예정) drivers/          # 벤더 SDK 어댑터 — 실제 SDK 선정 전까지 보류(가짜 구현 지양)
```

## 핵심 컴포넌트 (구현됨)

- **LogManager** (`core/infrastructure/logging`): 다채널 로거. 채널별 폴더 + 일자/크기 롤링 + 보관일수 삭제. `LogManagerSystemParam.json` 으로 구성(없으면 `exe\logs\<CHANNEL>` 기본값). 채널: Debug/Info/Event/Process/Exception/Param/Tcpip/Gpib. 매크로 `RS_INFO(채널, expr)` 등. Alarm 페이지에 실시간 표시 싱크 연결됨.
- **DataManager** (`core/infrastructure/data`): JSON 파일 읽기/쓰기(들여쓰기 직렬화), `Windows.Data.Json` 기반, 정적 유틸리티.
- **EquipmentStateMachine** (`core/domain`, 설계서 7.1): 8개 상태(PowerOff~Maintenance) 전이 테이블(7.1.4) + 상태별 명령 허용(7.1.6). **모든 전이는 단일 진입점 `RequestTransition`** 경유(7.1.5), 리스너로 전이 로깅. 스레드 안전. `Instance()` 전역 싱글톤(App 시작 시 리스너를 Process 채널 로깅에 연결) + 격리 테스트용 기본 생성자 둘 다 제공. 상태가 실제로 바뀌면 `EventBus` 로 `state.changed` 토픽 발행(payload=UTF-8 상태명).
- **EventBus** (`core/common`): `IEventBus` 구현. 싱글톤(`Instance()`), 스레드 안전, 핸들러는 잠금 밖에서 호출(재진입/데드락 방지). `EquipmentStateMachine`(`state.changed`)과 `AlarmService`(`alarm.changed`)가 발행자.
- **AlarmService** (`core/domain`): `IAlarmService` 구현. 싱글톤(`Instance()`). **알람 생명주기(5.1.5.2)**: `Raise`(Detected→Raised) → `Acknowledge`(→Acknowledged) → `Clear`(→Cleared, 원인 해소) → `Archive`(→Archived, Cleared 상태에서만 허용). `m_active` 는 Archived 이전 전체(`GetActive()`), `GetUnresolved()` 는 Raised/Acknowledged 만(= 바 색상 등 "실제 활성 문제" 판단용), `GetHistory()` 는 Archived 이력. **Reset 조건(5.1.5.6)**: `CanReset()` 은 미해소(Raised/Acknowledged) 알람이 남아있으면 false, `RequestReset()` 은 이를 확인한 뒤에만 상태머신 Reset 을 요청하고 성공 시 Cleared 알람을 자동 Archive. **알람↔상태머신 통합(7.1.7)**: 등급별로 `EquipmentStateMachine::Instance()` 에 전이 요청 — Info/Warning은 상태 유지, Major(Error)는 운전 중(AutoRunning/Paused)이면 `Stop` 요청, Fatal(Critical)은 즉시 `AlarmRaised` 로 Alarm 전이.
- **AlarmCode** (`core/domain`, 설계서 5.1.5.1): 알람 코드(uint32) 범위를 카테고리(Motion/Io/Recipe/Comm/Safety)로 나누고 `FormatAlarmId()` 로 `"MOT-001"` 형식 표시 문구 생성. 기존 `IAlarmService` 시그니처는 그대로 유지(코드값 자체는 안 바뀜).
- **InterlockClass** (`core/domain`, 설계서 5.1.5.3): Safety/EquipmentProtection/ProcessProtection/Operation 분류 + `BlocksAutoStart()` 판정.
- **RecipeRepository** (`core/domain`): `IRecipeRepository` 파일 기반 구현. `<dir>\<id>.json` 으로 `DataManager` 통해 저장/조회, `Validate()` 실제 검증(id/name/version), `List()` 는 디렉터리 스캔.
- **SequenceManager** (`sequence`, 설계서 5.1.1): 자동 운전 단계 실행. `Start/Pause/Resume/Stop/Abort` + `Tick()` 루프(타임아웃/실패복구, Stop↔Abort 구분 7.1.8). 단계는 `ISequenceStep`(시작/완료 조건, 타임아웃, 복구, Pause 허용)으로 구현. 전이는 상태머신 경유.
- **SequenceRunner** (`sequence`, 설계서 4.2): `std::jthread`+`stop_token` 으로 `SequenceManager::Tick()` 을 주기 호출하는 작업 스레드. `Stop()` 은 다음 짧은 sleep 구간에서 즉시 반응(6.4.5.2 종료 지연 방지).
- **구체 Step** (`sequence/steps`): `DelayStep`(시간 대기), `MoveAxisStep`(`IMotionController` 사용, 실패 시 안전정지 `OnFailure`).
- **simulator/**: `SimulatedMotionController`(가상 축 상태·이동시간 근사), `SimulatedIoProvider`(메모리 I/O 맵+이벤트 구독), `SimulatedHostGateway`(SECS/GEM 8.1 매핑 — CEID/ALID 스타일 로그, 레시피 업/다운로드 메모리 저장, 원격명령 콜백). 하드웨어 없이 시퀀스/알람/호스트 흐름 검증 가능(설계서 3.2 검증성).
- **MachineUiMapper** (`ui_winui/viewmodels/domain_adapters`): `EquipmentState`(8종) → `UiMachineState`(6종, `MachineUiSnapshot.h`) 매핑. 매핑 근거는 헤더 주석 참조.
- **OperationPage 실시간 연동**: 하단 알람 바는 `AlarmService`↔`EventBus`(`alarm.changed`)로 발생/미발생(빨강/초록) 자동 전환. "SYSTEM STATUS" 옆 라벨은 `EquipmentStateMachine`↔`EventBus`(`state.changed`)로 현재 상태 실시간 표시.
- **TrendPage 실시간 그래프**: 외부 차트 라이브러리 없이 `Canvas`+`Polyline` 직접 그리기로 구현. 500ms 주기 `DispatcherTimer` 로 온도/압력 2계열 샘플링(최근 60개 스크롤), 각 계열 자체 min/max 로 정규화해 공용 캔버스 높이에 표시. **현재는 시뮬레이션 값**(사인파+노이즈) — 실제 `IIoProvider`/센서 연동은 TODO(설계서 5.1.7 Trace Manager).
- **tests/SelfTests**: `RunSelfTests()` — AlarmCode/InterlockClass(순수 함수), `EquipmentStateMachine`(격리된 로컬 인스턴스, `Instance()` 미사용), `RecipeRepository`(임시 폴더, 정리함)를 검증. **전역 싱글톤을 건드리지 않아 실행 중인 앱 상태를 오염시키지 않음.** `App::OnLaunched` 에서 `_DEBUG` 빌드마다 자동 실행, 결과는 Debug 채널에 기록. (AlarmService 는 순수 싱글톤이라 격리 인스턴스 생성이 불가능해 self-test 대상에서 제외 — 정책 로직은 이 세션에서 실제 시나리오로 수동 검증됨.)
- **인터페이스만 있는 것** (설계서 6.2): `IMotionController`/`IIoProvider`/`IVisionCamera`는 `simulator/` 구현만 있고 실제 벤더 어댑터(`drivers/`)는 없음. `IHostGateway`도 시뮬레이터만 있음(실제 SECS/GEM 스택 없음).

## WinUI 주의사항 (해결 이력)

- `App::App()` 는 `InitializeComponent()` 를 호출해야 함(App.xaml 리소스 로드). 누락 시 시작 시 `hresult_error`.
- 각 페이지 생성자도 `InitializeComponent()` 호출(없으면 빈 화면).
- 페이지 XAML 루트는 `Page`, IDL은 `Microsoft.UI.Xaml.Controls.Page` (실수로 `Window` 금지).
- `Icon="..."` 축약은 유효한 `Symbol` 열거형만 가능(예: `Warning` 없음 → `Important` 또는 `FontIcon`).
- XAML `x:Bind` 대상 속성은 **IDL에 선언**되어야 함(구현 메서드만으론 부족). 보편 타입은 IDL에서 `Object`(= C++ `IInspectable`).
- XAML 마크업 생성물(`*.xaml.g.h`)이 소스 하위경로(`Generated Files\src\ui_winui\...`)에 생성되므로 해당 디렉터리들이 `AdditionalIncludeDirectories` 에 등록되어 있음(vcxproj). `Generated Files\module.g.cpp` 도 명시적으로 컴파일 포함.

## 로드맵 (설계서 기준)

- [x] 상태 머신(7.1) + Sequence 골격(5.1.1) — EquipmentStateMachine, SequenceManager 구현/검증 완료
- [x] 알람↔상태머신 통합(7.1.7) — EventBus/AlarmService 구현, 등급별 전이(Major→Stop, Fatal→AlarmRaised) 실시나리오 검증 완료, OperationPage 알람 바 연동
- [x] Sequence Thread(4.2) — `SequenceRunner`(jthread+stop_token) 구현, 실제 시퀀스 파이프라인(Idle→AutoRunning→Stopping) 실행 검증 완료
- [x] 구체 `ISequenceStep` 구현 — `DelayStep`, `MoveAxisStep`(IMotionController 연동, 실패 시 안전정지)
- [x] `simulator/` — SimulatedMotionController/IoProvider/HostGateway 구현, 하드웨어 없이 시퀀스 실행 검증 완료
- [x] 도메인 상세 — 알람 ID 체계(5.1.5.1 `AlarmCode`), 인터락 분류(5.1.5.3 `InterlockClass`), Recipe 검증(`RecipeRepository`) 구현/검증 완료
- [x] UI 연동 — `EquipmentStateMachine`/`AlarmService` ↔ `IEventBus` ↔ OperationPage 연결(state.changed/alarm.changed 실시간 반영), `MachineUiMapper` 로 `MachineUiSnapshot` → `EquipmentState` 정렬 완료
- [x] SECS/GEM 매핑(8.1) — `SimulatedHostGateway` 로 상태/알람/이벤트 보고, 레시피 송수신, 원격명령 매핑 구현
- [x] 테스트 전략(12) — `tests/SelfTests` 로 레시피 검증·상태 전이 단위 테스트 자동화(Debug 빌드 시 실행)
- [x] 알람 생명주기(5.1.5.2 Detected/Raised/Acknowledged/Cleared/Archived) + Reset 조건(5.1.5.6) — `AlarmService`(Raise/Acknowledge/Clear/Archive/CanReset/RequestReset) 구현, 실시나리오(미해소 시 Reset 거부 → Clear 후 허용 → 자동 Archive) 검증 완료
- [ ] `drivers/` — 벤더 SDK 어댑터(HAL 구현체). **의도적 보류**: 실제 SDK가 없어 가짜 구현은 장식적 코드가 되므로, 벤더/장비 확정 후 `simulator/` 구현체를 대체하는 방식으로 진행
- [ ] SECS/GEM 실제 드라이버(현재는 시뮬레이터만), 호스트 통신 스레드 분리
- [ ] 인터락 실시간 평가(5.1.5.5 — Real-time Loop/10~100ms/명령 실행 전/이벤트 기반 계층별 처리)는 아직 없음(`InterlockClass` 는 분류 데이터만 제공)
- [ ] Reset 조건 중 축/출력 안전 상태 확인(5.1.5.6 나머지 2개 조건)은 HAL/모션 연동 필요 — 현재 `CanReset()` 은 알람 원인 해소 여부만 확인
