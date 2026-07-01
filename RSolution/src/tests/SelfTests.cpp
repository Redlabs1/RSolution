#include "pch.h"
#include "tests/SelfTests.h"

#include "core/domain/AlarmCode.h"
#include "core/domain/InterlockClass.h"
#include "core/domain/EquipmentStateMachine.h"
#include "core/domain/RecipeRepository.h"
#include "core/infrastructure/logging/LogManager.h"

#include <windows.h>
#include <vector>
#include <string>

namespace rs::tests
{
    namespace
    {
        struct TestContext
        {
            std::vector<std::wstring> failures;

            void Check(bool condition, wchar_t const* description)
            {
                if (!condition)
                    failures.push_back(description);
            }
        };

        // ---- 알람 ID 체계(5.1.5.1) ----
        void TestAlarmCode(TestContext& t)
        {
            using namespace rs::domain;
            t.Check(MakeAlarmCode(AlarmCategory::Motion, 1) == 1001, L"AlarmCode: MOT 카테고리 코드 생성");
            t.Check(CategoryOf(1001) == AlarmCategory::Motion, L"AlarmCode: 1001 -> Motion 카테고리 판별");
            t.Check(CategoryOf(5001) == AlarmCategory::Safety, L"AlarmCode: 5001 -> Safety 카테고리 판별");
            t.Check(FormatAlarmId(1001) == L"MOT-001", L"AlarmCode: 1001 표시 형식 MOT-001");
            t.Check(FormatAlarmId(3007) == L"RCP-007", L"AlarmCode: 3007 표시 형식 RCP-007");
        }

        // ---- 인터락 분류(5.1.5.3) ----
        void TestInterlockClass(TestContext& t)
        {
            using namespace rs::domain;
            t.Check(BlocksAutoStart(InterlockClass::Safety), L"Interlock: Safety 는 자동시작 차단");
            t.Check(BlocksAutoStart(InterlockClass::Operation), L"Interlock: Operation 은 자동시작 차단");
            t.Check(ToString(InterlockClass::EquipmentProtection) == L"EquipmentProtection",
                L"Interlock: ToString 표시 확인");
        }

        // ---- 상태 전이(7.1.4/7.1.6) — 전역 싱글톤이 아닌 격리된 로컬 인스턴스 사용 ----
        void TestEquipmentStateMachine(TestContext& t)
        {
            using namespace rs::domain;
            EquipmentStateMachine sm;   // Instance() 미사용 — 실제 앱 상태와 완전히 격리

            auto r1 = sm.RequestTransition(EquipmentCommand::Start, L"SelfTest", L"reject");
            t.Check(!r1.allowed, L"StateMachine: PowerOff 에서 Start 거부");

            sm.RequestTransition(EquipmentCommand::PowerOn, L"SelfTest", L"");
            sm.RequestTransition(EquipmentCommand::InitComplete, L"SelfTest", L"");
            t.Check(sm.State() == EquipmentState::Idle, L"StateMachine: PowerOn+InitComplete -> Idle");

            auto r2 = sm.RequestTransition(EquipmentCommand::Start, L"SelfTest", L"");
            t.Check(r2.allowed && sm.State() == EquipmentState::AutoRunning,
                L"StateMachine: Idle 에서 Start -> AutoRunning");

            auto r3 = sm.RequestTransition(EquipmentCommand::RecipeSelect, L"SelfTest", L"");
            t.Check(!r3.allowed, L"StateMachine: AutoRunning 중 RecipeSelect 거부(7.1.6)");

            sm.RequestTransition(EquipmentCommand::Pause, L"SelfTest", L"");
            t.Check(sm.State() == EquipmentState::Paused, L"StateMachine: Pause -> Paused");

            sm.RequestTransition(EquipmentCommand::Resume, L"SelfTest", L"");
            t.Check(sm.State() == EquipmentState::AutoRunning, L"StateMachine: Resume -> AutoRunning");

            sm.RequestTransition(EquipmentCommand::Stop, L"SelfTest", L"");
            t.Check(sm.State() == EquipmentState::Stopping, L"StateMachine: Stop -> Stopping");

            sm.RequestTransition(EquipmentCommand::StopComplete, L"SelfTest", L"");
            t.Check(sm.State() == EquipmentState::Idle, L"StateMachine: StopComplete -> Idle");

            auto r4 = sm.RequestTransition(EquipmentCommand::Reset, L"SelfTest", L"");
            t.Check(!r4.allowed, L"StateMachine: Idle 에서 Reset 거부(Alarm 전용)");
        }

        // ---- Recipe 검증/저장/조회 — 임시 폴더에서 격리 실행 후 정리 ----
        void TestRecipeRepository(TestContext& t)
        {
            using namespace rs::domain;

            wchar_t exeBuf[MAX_PATH]{};
            ::GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
            std::wstring exeDir(exeBuf);
            const auto pos = exeDir.find_last_of(L"\\/");
            if (pos != std::wstring::npos) exeDir.erase(pos + 1);
            const std::wstring testDir = exeDir + L"selftest_recipes_tmp";

            RecipeRepository repo(testDir);

            Recipe invalid;   // id 비어있음
            auto vInvalid = repo.Validate(invalid);
            t.Check(!vInvalid.ok(), L"Recipe: 빈 id 는 검증 실패");

            Recipe valid;
            valid.id = "R-SELFTEST-001";
            valid.name = "SelfTest Recipe";
            valid.version = 1;
            valid.parameters["temperature"] = "250";

            auto vValid = repo.Validate(valid);
            t.Check(vValid.ok(), L"Recipe: 유효한 레시피는 검증 통과");

            auto saved = repo.Save(valid);
            t.Check(saved.ok(), L"Recipe: 저장 성공");

            auto loaded = repo.Load(valid.id);
            t.Check(loaded.ok()
                && loaded.value().id == valid.id
                && loaded.value().name == valid.name
                && loaded.value().version == valid.version
                && loaded.value().parameters.at("temperature") == "250",
                L"Recipe: 저장/조회 왕복 일치");

            auto list = repo.List();
            bool found = false;
            if (list.ok())
                for (auto const& id : list.value())
                    if (id == valid.id) found = true;
            t.Check(found, L"Recipe: List() 에 저장한 id 포함");

            // 정리: 테스트 산출물 삭제.
            ::DeleteFileW((testDir + L"\\" + std::wstring(valid.id.begin(), valid.id.end()) + L".json").c_str());
            ::RemoveDirectoryW(testDir.c_str());
        }
    }

    bool RunSelfTests()
    {
        TestContext t;
        TestAlarmCode(t);
        TestInterlockClass(t);
        TestEquipmentStateMachine(t);
        TestRecipeRepository(t);

        RS_INFO(rs::LogChannel::Debug, L"[SelfTest] 실행 완료: " << (t.failures.empty() ? L"전부 통과" : L"실패 있음"));
        for (auto const& f : t.failures)
            RS_ERROR(rs::LogChannel::Debug, L"[SelfTest] FAIL: " << f);

        return t.failures.empty();
    }
}
