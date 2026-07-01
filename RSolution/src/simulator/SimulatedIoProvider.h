#pragma once

#include "hal/IIoProvider.h"
#include <map>
#include <mutex>

// 가상 I/O 공급자. 메모리 상의 디지털/아날로그 맵으로 동작하며, 테스트/시뮬레이터 코드가
// SetDigitalForTest 로 상태를 주입해 구독자에게 이벤트를 발행할 수 있다(설계서 3.2 검증성).
namespace rs::simulator
{
    class SimulatedIoProvider : public rs::hal::IIoProvider
    {
    public:
        rs::core::Result<bool> ReadDigital(rs::hal::IoPoint point) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_digital[point];
        }

        rs::core::Result<double> ReadAnalog(rs::hal::IoPoint point) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_analog[point];
        }

        rs::core::Status WriteDigital(rs::hal::IoPoint point, bool value) override
        {
            std::vector<rs::hal::IoEventHandler> toNotify;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_digital[point] = value;
                for (auto const& [id, sub] : m_subs)
                    if (sub.point == point)
                        toNotify.push_back(sub.handler);
            }
            rs::hal::IoChangeEvent e{ point, value };
            for (auto const& h : toNotify)
            {
                try { h(e); } catch (...) {}
            }
            return rs::core::Status::Success();
        }

        rs::core::Status WriteAnalog(rs::hal::IoPoint point, double value) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_analog[point] = value;
            return rs::core::Status::Success();
        }

        rs::hal::IoSubscription Subscribe(rs::hal::IoPoint point, rs::hal::IoEventHandler handler) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const auto id = m_nextSub++;
            m_subs[id] = Subscription{ point, std::move(handler) };
            return id;
        }

        void Unsubscribe(rs::hal::IoSubscription token) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_subs.erase(token);
        }

        // 테스트/시뮬레이터 전용: 외부 신호 변화를 주입한다(WriteDigital 과 동일하게 이벤트 발행).
        void SetDigitalForTest(rs::hal::IoPoint point, bool value)
        {
            WriteDigital(point, value);
        }

    private:
        struct Subscription
        {
            rs::hal::IoPoint point;
            rs::hal::IoEventHandler handler;
        };

        std::mutex m_mutex;
        std::map<rs::hal::IoPoint, bool> m_digital;
        std::map<rs::hal::IoPoint, double> m_analog;
        std::map<rs::hal::IoSubscription, Subscription> m_subs;
        rs::hal::IoSubscription m_nextSub{ 1 };
    };
}
