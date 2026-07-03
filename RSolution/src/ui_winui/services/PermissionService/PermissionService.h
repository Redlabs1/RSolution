#pragma once

#include "ui_winui/services/PermissionService/IPermissionService.h"

#include <atomic>

// UI 문서 v2 6장: IPermissionService 기본 구현.
// 권한 판정 로직은 갖지 않고 rs::domain::HasPermission() 에 위임한다.
// UI 스레드와 이벤트 스레드에서 동시에 조회될 수 있으므로 atomic 으로 보관한다.
namespace rs::ui
{
    class PermissionService final : public IPermissionService
    {
    public:
        rs::domain::UserRole CurrentRole() const override
        {
            return m_role.load(std::memory_order_acquire);
        }

        void SetCurrentRole(rs::domain::UserRole role) override
        {
            m_role.store(role, std::memory_order_release);
        }

        bool Has(rs::domain::Permission permission) const override
        {
            return rs::domain::HasPermission(CurrentRole(), permission);
        }

    private:
        std::atomic<rs::domain::UserRole> m_role{ rs::domain::UserRole::None };
    };
}
