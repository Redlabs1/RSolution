#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <cstdint>

// 설계서 6.2 / 4.x: 모듈 간 비동기 이벤트 발행 및 구독.
// 표현 계층과 제어 계층을 분리하고 이벤트 기반으로 상태를 전달하는 데 사용한다.
namespace rs::core
{
    // 모듈 간 전달되는 이벤트의 최소 표현. payload 는 핸들/ID 또는 직렬화 문자열로 전달한다
    // (설계서 6.4.5.4: 대용량 객체는 variant 직접 보관 대신 ID/핸들로 전달).
    struct Event
    {
        std::string topic;      // 예: "alarm.raised", "state.changed"
        std::string payload;    // JSON 또는 식별자
        std::int64_t timestamp{ 0 };
    };

    using EventHandler = std::function<void(Event const&)>;
    using SubscriptionToken = std::uint64_t;

    class IEventBus
    {
    public:
        virtual ~IEventBus() = default;

        // 이벤트 발행(비동기). 발행 스레드를 블로킹하지 않아야 한다.
        virtual void Publish(Event const& event) = 0;

        // 토픽 구독. 반환된 토큰으로 Unsubscribe.
        virtual SubscriptionToken Subscribe(std::string_view topic, EventHandler handler) = 0;

        virtual void Unsubscribe(SubscriptionToken token) = 0;
    };
}
