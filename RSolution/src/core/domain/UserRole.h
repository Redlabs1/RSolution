#pragma once

#include <string_view>
#include <cstdint>

// 설계서 v2 9장: 사용자 권한 등급과 권한-기능 매트릭스(9.4).
// 권한은 UI 버튼 활성화 기준이 아니라 Application 계층 명령 진입점의 검증 기준이다(설계서 9장 서두).
namespace rs::domain
{
    enum class UserRole : std::uint8_t
    {
        None = 0,       // 미로그인
        Operator,       // 생산 작업자
        Engineer,       // 공정 엔지니어
        Maintenance,    // 설비 엔지니어
        Administrator   // 관리자
    };

    enum class Permission : std::uint8_t
    {
        AutoStartStop,      // 자동 운전 시작/정지
        AlarmAckReset,      // 알람 확인/Reset
        RecipeSelect,       // 레시피 선택(승인된 레시피)
        RecipeEdit,         // 레시피 편집
        RecipeApprove,      // 레시피 승인
        ManualAxisMove,     // 수동 축 이동
        IoForceOutput,      // I/O 강제 출력
        AlarmMasking,       // 알람 마스킹(허용 항목만, 설계서 5.6.8)
        SystemSetup,        // 시스템 설정 변경
        UserManagement      // 사용자 계정 관리
    };

    // 설계서 9.4 권한-기능 매트릭스. 컴파일 시점 검증 가능하도록 constexpr 로 정의(설계서 6.5.2).
    constexpr bool HasPermission(UserRole role, Permission p) noexcept
    {
        switch (p)
        {
        case Permission::AutoStartStop:
        case Permission::AlarmAckReset:
        case Permission::RecipeSelect:
            return role >= UserRole::Operator;
        case Permission::RecipeEdit:
            return role >= UserRole::Engineer;
        case Permission::ManualAxisMove:
        case Permission::IoForceOutput:
        case Permission::AlarmMasking:
            return role >= UserRole::Maintenance;
        case Permission::RecipeApprove:
        case Permission::SystemSetup:
        case Permission::UserManagement:
            return role == UserRole::Administrator;
        default:
            return false;
        }
    }

    constexpr std::wstring_view ToString(UserRole r) noexcept
    {
        switch (r)
        {
        case UserRole::None:          return L"None";
        case UserRole::Operator:      return L"Operator";
        case UserRole::Engineer:      return L"Engineer";
        case UserRole::Maintenance:   return L"Maintenance";
        case UserRole::Administrator: return L"Administrator";
        default:                      return L"Unknown";
        }
    }

    static_assert(HasPermission(UserRole::Operator, Permission::AutoStartStop));
    static_assert(!HasPermission(UserRole::Operator, Permission::RecipeEdit));
    static_assert(!HasPermission(UserRole::Maintenance, Permission::SystemSetup));
    static_assert(HasPermission(UserRole::Administrator, Permission::UserManagement));
}
