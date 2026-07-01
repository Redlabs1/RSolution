#include "pch.h"
#include "sequence/SequenceRunner.h"

namespace rs::sequence
{
    SequenceRunner::SequenceRunner(SequenceManager& manager, std::chrono::milliseconds period)
        : m_manager(manager), m_period(period)
    {
    }

    SequenceRunner::~SequenceRunner()
    {
        Stop();   // jthread 소멸자가 stop+join 하지만, 루프가 즉시 반응하도록 명시적으로 신호
    }

    void SequenceRunner::Start()
    {
        if (m_running.exchange(true))
            return;   // 이미 실행 중

        m_thread = std::jthread([this](std::stop_token token) { Run(token); });
    }

    void SequenceRunner::Stop()
    {
        if (!m_running.exchange(false))
            return;
        m_thread.request_stop();
        if (m_thread.joinable())
            m_thread.join();
    }

    bool SequenceRunner::IsRunning() const noexcept
    {
        return m_running.load();
    }

    void SequenceRunner::Run(std::stop_token token)
    {
        // 6.4.5.1: 실시간 루프가 아니므로 sleep 기반 주기 실행을 허용한다(Sequence Thread 는
        // Real-time Loop 와 분리된 별도 작업 스레드, 설계서 4.2 프로세스/스레드 모델).
        while (!token.stop_requested())
        {
            m_manager.Tick();

            // 짧은 간격으로 나눠 잠들어 stop 요청에 빠르게 반응(블로킹 I/O 중에도 종료 가능해야
            // 한다는 6.4.5.2 원칙을 sleep 세분화로 근사).
            auto remaining = m_period;
            constexpr auto slice = std::chrono::milliseconds(5);
            while (remaining.count() > 0 && !token.stop_requested())
            {
                const auto step = (remaining < slice) ? remaining : slice;
                std::this_thread::sleep_for(step);
                remaining -= step;
            }
        }
    }
}
