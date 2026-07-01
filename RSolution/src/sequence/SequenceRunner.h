#pragma once

#include "sequence/SequenceManager.h"
#include <thread>
#include <stop_token>
#include <chrono>
#include <atomic>

// 설계서 4.2: SequenceManager::Tick() 을 고우선순위 작업 스레드에서 주기 호출한다.
// std::jthread 는 소멸 시 자동으로 stop_token 을 신호하고 join 하지만(6.4.5.2),
// 종료 지연을 피하기 위해 루프 내부에서 stop_token 을 짧은 주기로 확인한다.
namespace rs::sequence
{
    class SequenceRunner
    {
    public:
        explicit SequenceRunner(SequenceManager& manager,
                                std::chrono::milliseconds period = std::chrono::milliseconds(20));
        ~SequenceRunner();

        void Start();
        void Stop();   // 다음 주기 확인 시점에 스레드를 안전하게 종료(블로킹 없음)

        bool IsRunning() const noexcept;

        SequenceRunner(SequenceRunner const&) = delete;
        SequenceRunner& operator=(SequenceRunner const&) = delete;

    private:
        void Run(std::stop_token token);

        SequenceManager&           m_manager;
        std::chrono::milliseconds  m_period;
        std::jthread                m_thread;
        std::atomic<bool>          m_running{ false };
    };
}
