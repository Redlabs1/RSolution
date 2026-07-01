#pragma once

#include "core/common/Result.h"
#include "core/domain/IRecipeRepository.h"
#include <functional>
#include <string>
#include <cstdint>

// 설계서 6.2 / 5.1.6 / 8: IHostGateway — 이벤트 보고, 알람 보고, 원격 명령 수신, 레시피 송수신.
// 설계서 4: 호스트 통신 장애가 장비 핵심 제어를 방해하지 않도록 분리한다.
namespace rs::host
{
    // 호스트(MES/EAP)로부터 수신한 원격 명령.
    struct RemoteCommand
    {
        std::string name;       // 예: "START", "STOP", "PP-SELECT"
        std::string argument;   // 파라미터(예: 레시피 id)
    };

    using RemoteCommandHandler = std::function<void(RemoteCommand const&)>;

    class IHostGateway
    {
    public:
        virtual ~IHostGateway() = default;

        // 이벤트/알람 보고(SECS/GEM Collection Event, Alarm Report).
        virtual rs::core::Status ReportEvent(std::uint32_t eventId, std::string const& payload) = 0;
        virtual rs::core::Status ReportAlarm(std::uint32_t alarmCode, bool set) = 0;

        // 원격 명령 수신 콜백 등록.
        virtual void OnRemoteCommand(RemoteCommandHandler handler) = 0;

        // 레시피 송수신(PP-SEND / PP-REQUEST).
        virtual rs::core::Status SendRecipe(rs::domain::Recipe const& recipe) = 0;
        virtual rs::core::Result<rs::domain::Recipe> ReceiveRecipe(std::string const& id) = 0;
    };
}
