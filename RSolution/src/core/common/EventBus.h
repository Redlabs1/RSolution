#pragma once

#include "core/common/IEventBus.h"
#include <map>
#include <mutex>
#include <string>

// 설계서 6.2: IEventBus 구체 구현. 스레드 안전. 발행은 호출 스레드에서 동기 전달하되,
// 핸들러는 잠금 밖에서 호출한다(구독자가 UI 스레드 마샬링 등을 직접 수행).
namespace rs::core
{
    class EventBus : public IEventBus
    {
    public:
        static EventBus& Instance() noexcept;

        void Publish(Event const& event) override;
        SubscriptionToken Subscribe(std::string_view topic, EventHandler handler) override;
        void Unsubscribe(SubscriptionToken token) override;

        EventBus(EventBus const&) = delete;
        EventBus& operator=(EventBus const&) = delete;

    private:
        EventBus() = default;

        struct Subscription
        {
            std::string  topic;
            EventHandler handler;
        };

        std::mutex                              m_mutex;
        std::map<SubscriptionToken, Subscription> m_subs;
        SubscriptionToken                       m_next{ 1 };
    };
}
