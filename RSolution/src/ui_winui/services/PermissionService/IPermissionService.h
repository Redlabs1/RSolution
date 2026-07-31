#pragma once

#include "core/domain/UserRole.h"

// UI 문서 v2 6장: 권한 검사 서비스 인터페이스.
// core/domain/UserRole.h 의 HasPermission() 을 단일 진실 공급원으로 사용한다.
// ViewModel 은 이 서비스로 IsEnabled 바인딩용 값을 계산하고,
// 명령 실행 검증은 Application 계층에서 이중으로 수행한다(UI 는 표시 제어만 담당).
namespace rs::ui
{
    class IPermissionService
    {
    public:
        virtual ~IPermissionService() = default;

        // 현재 로그인 사용자의 권한 등급. 미로그인 시 UserRole::None.
        virtual rs::domain::UserRole CurrentRole() const = 0;

        // 로그인/로그아웃/세션 타임아웃 강등 시 갱신 (UI 문서 3.11 세션 정책).
        virtual void SetCurrentRole(rs::domain::UserRole role) = 0;

        // 해당 기능 권한 보유 여부 (설계서 9.4 권한-기능 매트릭스).
        virtual bool Has(rs::domain::Permission permission) const = 0;
    };
}
