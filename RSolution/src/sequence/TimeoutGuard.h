#pragma once

#include <chrono>

// 설계서 5.1.1 / 5.1.5.5: 단계/명령 타임아웃 감시용 경량 가드.
namespace rs::sequence
{
    class TimeoutGuard
    {
    public:
        explicit TimeoutGuard(std::chrono::milliseconds timeout) noexcept
            : m_deadline(std::chrono::steady_clock::now() + timeout) {}

        bool Expired() const noexcept
        {
            return std::chrono::steady_clock::now() >= m_deadline;
        }

        void Reset(std::chrono::milliseconds timeout) noexcept
        {
            m_deadline = std::chrono::steady_clock::now() + timeout;
        }

    private:
        std::chrono::steady_clock::time_point m_deadline;
    };
}
