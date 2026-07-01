#pragma once

#include "host/IHostGateway.h"
#include "core/infrastructure/logging/LogManager.h"
#include <map>
#include <mutex>
#include <vector>

// 설계서 8.1(SECS/GEM 기능 매핑) 시뮬레이터 구현. 실제 SECS/GEM 드라이버 대신 Event 채널에
// 표준 매핑 형식(CEID/ALID 스타일)으로 기록하여, 호스트 없이 이벤트·알람·레시피·원격명령
// 흐름을 검증할 수 있게 한다(설계서 3.2 검증성, 12 호스트 통신 테스트).
namespace rs::simulator
{
    class SimulatedHostGateway : public rs::host::IHostGateway
    {
    public:
        // 상태 보고(8.1 "상태 보고"): 장비 상태/제어모드 변경을 CEID 이벤트로 기록.
        rs::core::Status ReportEvent(std::uint32_t eventId, std::string const& payload) override
        {
            RS_INFO(rs::LogChannel::Event, L"[SECS/GEM] CEID=" << eventId << L" payload(utf8 len)=" << payload.size());
            return rs::core::Status::Success();
        }

        // 알람 보고(8.1 "알람 보고"): Alarm ID/등급/발생·해제 시간을 ALID 스타일로 기록.
        rs::core::Status ReportAlarm(std::uint32_t alarmCode, bool set) override
        {
            RS_INFO(rs::LogChannel::Event, L"[SECS/GEM] ALID=" << alarmCode << (set ? L" SET" : L" CLEAR"));
            return rs::core::Status::Success();
        }

        void OnRemoteCommand(rs::host::RemoteCommandHandler handler) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_handlers.push_back(std::move(handler));
        }

        // 원격 명령(8.1 "원격 명령"): 상태 검증은 호출자(Application 계층)가 수행한다는 전제 하에
        // 등록된 핸들러로 전달만 한다.
        void InjectRemoteCommandForTest(rs::host::RemoteCommand const& cmd)
        {
            std::vector<rs::host::RemoteCommandHandler> handlers;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                handlers = m_handlers;
            }
            for (auto const& h : handlers)
            {
                try { h(cmd); } catch (...) {}
            }
        }

        // 레시피 관리(8.1 "레시피 관리"): 업로드/다운로드를 메모리 저장소로 모사.
        rs::core::Status SendRecipe(rs::domain::Recipe const& recipe) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_recipes[recipe.id] = recipe;
            RS_INFO(rs::LogChannel::Event, L"[SECS/GEM] PP-SEND id=" << Widen(recipe.id));
            return rs::core::Status::Success();
        }

        rs::core::Result<rs::domain::Recipe> ReceiveRecipe(std::string const& id) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_recipes.find(id);
            if (it == m_recipes.end())
                return rs::core::Error{ 1, "recipe not found on host: " + id };
            RS_INFO(rs::LogChannel::Event, L"[SECS/GEM] PP-REQUEST id=" << Widen(id));
            return it->second;
        }

    private:
        static std::wstring Widen(std::string const& s)
        {
            return std::wstring(s.begin(), s.end());   // 로그 표시용 근사 변환(ASCII 레시피 ID 가정)
        }

        std::mutex m_mutex;
        std::vector<rs::host::RemoteCommandHandler> m_handlers;
        std::map<std::string, rs::domain::Recipe> m_recipes;
    };
}
