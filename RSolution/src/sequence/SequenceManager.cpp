#include "pch.h"
#include "sequence/SequenceManager.h"
#include "core/domain/EquipmentStateMachine.h"

namespace rs::sequence
{
    SequenceManager::SequenceManager(SequenceContext context)
        : m_context(context)
    {
    }

    void SequenceManager::SetSteps(std::vector<std::unique_ptr<ISequenceStep>> steps)
    {
        m_steps = std::move(steps);
    }

    rs::core::Status SequenceManager::Start()
    {
        if (m_running.load())
            return rs::core::Error{ 1, "sequence already running" };
        if (m_steps.empty())
            return rs::core::Error{ 2, "no sequence steps" };

        // 상태 전이 단일 진입점 경유(7.1.5). 허용되지 않으면 시작하지 않는다.
        if (m_context.stateMachine)
        {
            const auto r = m_context.stateMachine->RequestTransition(
                rs::domain::EquipmentCommand::Start, L"Sequence", L"자동 운전 시작");
            if (!r.allowed)
                return rs::core::Error{ 3, "Start not allowed in current state" };
        }

        m_current = 0;
        m_entered = false;
        m_paused = false;
        m_stopRequested = false;
        m_abortRequested = false;
        m_running = true;
        return rs::core::Status::Success();
    }

    void SequenceManager::RequestPause()  { m_paused = true; }
    void SequenceManager::RequestResume() { m_paused = false; }
    void SequenceManager::RequestStop()   { m_stopRequested = true; }
    void SequenceManager::RequestAbort()  { m_abortRequested = true; }

    bool SequenceManager::IsRunning() const noexcept { return m_running.load(); }
    bool SequenceManager::IsPaused() const noexcept  { return m_paused.load(); }

    void SequenceManager::EnterStep(std::size_t index)
    {
        m_current = index;
        m_entered = true;
        ISequenceStep& step = *m_steps[index];
        step.OnEnter(m_context);
        m_timeout.emplace(step.Timeout());
    }

    void SequenceManager::FailCurrent(std::wstring_view /*reason*/)
    {
        if (m_current < m_steps.size())
            m_steps[m_current]->OnFailure(m_context);
        Finish(false);
    }

    void SequenceManager::Finish(bool normal)
    {
        m_running = false;
        m_entered = false;
        m_timeout.reset();
        if (m_context.stateMachine)
        {
            using C = rs::domain::EquipmentCommand;
            m_context.stateMachine->RequestTransition(
                normal ? C::ProcessComplete : C::Abort,
                L"Sequence",
                normal ? L"공정 완료" : L"시퀀스 실패/중단");
        }
    }

    void SequenceManager::Tick()
    {
        if (!m_running.load())
            return;

        // 즉시/안전 정지(Abort) — 현재 단계를 종료하고 시퀀스 중단.
        if (m_abortRequested.exchange(false))
        {
            if (m_entered && m_current < m_steps.size())
                m_steps[m_current]->OnExit(m_context);
            Finish(false);
            return;
        }

        // 일시정지 — 현재 단계가 Pause 허용(7.1.8)일 때만 멈춘다.
        if (m_paused.load())
        {
            if (m_current < m_steps.size() && m_steps[m_current]->CanPause())
                return;
        }

        // 정상 정지(Stop) — 현재 단계를 마무리하고 종료.
        if (m_stopRequested.load())
        {
            if (m_entered && m_current < m_steps.size())
                m_steps[m_current]->OnExit(m_context);
            Finish(true);
            return;
        }

        if (m_current >= m_steps.size())
        {
            Finish(true);
            return;
        }

        ISequenceStep& step = *m_steps[m_current];

        if (!m_entered)
        {
            if (!step.CanStart(m_context))
            {
                FailCurrent(L"시작 조건 미충족");
                return;
            }
            EnterStep(m_current);
        }

        // 타임아웃 감시(5.1.1 TimeoutGuard).
        if (m_timeout && m_timeout->Expired())
        {
            step.OnExit(m_context);
            FailCurrent(L"타임아웃");
            return;
        }

        const StepResult result = step.Execute(m_context);
        if (result == StepResult::Completed)
        {
            step.OnExit(m_context);
            m_entered = false;
            if (++m_current >= m_steps.size())
                Finish(true);
        }
        else if (result == StepResult::Failed)
        {
            step.OnExit(m_context);
            FailCurrent(L"단계 실패");
        }
        // Running 이면 다음 Tick 에서 계속.
    }
}
