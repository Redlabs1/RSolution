#include "pch.h"
#include "core/common/EventBus.h"
#include <vector>

namespace rs::core
{
    EventBus& EventBus::Instance() noexcept
    {
        static EventBus instance;
        return instance;
    }

    SubscriptionToken EventBus::Subscribe(std::string_view topic, EventHandler handler)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const SubscriptionToken id = m_next++;
        m_subs.emplace(id, Subscription{ std::string(topic), std::move(handler) });
        return id;
    }

    void EventBus::Unsubscribe(SubscriptionToken token)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subs.erase(token);
    }

    void EventBus::Publish(Event const& event)
    {
        std::vector<EventHandler> handlers;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto const& [id, sub] : m_subs)
                if (sub.topic == event.topic)
                    handlers.push_back(sub.handler);
        }
        // 잠금 밖에서 호출(재진입/데드락 방지).
        for (auto const& h : handlers)
        {
            try { h(event); } catch (...) {}
        }
    }
}
