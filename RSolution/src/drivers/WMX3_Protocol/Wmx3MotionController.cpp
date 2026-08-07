#include "pch.h"
#include "Wmx3MotionController.h"

#include <map>
#include <mutex>
#include <thread>

#include "core/infrastructure/logging/LogManager.h"

// WMX3 SDK 는 별도 설치(기본 C:\Program Files\SoftServo\WMX3\Include)가 필요하다.
// 미설치 PC 에서도 프로젝트가 빌드되도록 헤더 존재 여부로 분기한다.
// 설치 후 vcxproj 의 WMX3 include/lib 경로가 유효해지면 자동으로 실제 구현이 컴파일된다.
#if __has_include(<WMX3Api.h>)
#  define RS_WMX3_SDK 1
#  include <WMX3Api.h>
#  include <EcApi.h>
#  pragma comment(lib, "WMX3Api.lib")
#  pragma comment(lib, "CoreMotionApi.lib")
#  pragma comment(lib, "EcApi.lib")
#else
#  define RS_WMX3_SDK 0
#endif

namespace rs::drivers
{
    namespace
    {
        core::Error MakeError(int code, std::string message)
        {
            return core::Error{ code, std::move(message) };
        }

        core::Status SdkMissing()
        {
            return core::Status{ MakeError(wmx3_errc::SdkNotInstalled,
                                           "WMX3 SDK not installed (rebuild after installing WMX3)") };
        }
    }

#if RS_WMX3_SDK

    using namespace wmx3Api;
    using namespace ecApi;

    struct Wmx3MotionController::Impl
    {
        mutable std::recursive_mutex mutex;

        WMX3Api    api;
        CoreMotion core{ &api };
        Ecat       ecat{ &api };

        bool open{ false };

        std::map<hal::AxisId, Wmx3AxisParam> params;

        Wmx3AxisParam ParamOf(hal::AxisId axis) const
        {
            auto it = params.find(axis);
            return it != params.end() ? it->second : Wmx3AxisParam{};
        }

        // WMX3 API 반환 코드를 Error 로 변환(문자열은 SDK 의 ErrorToString 사용).
        core::Status Wrap(int ret, char const* what)
        {
            if (ret == 0)
            {
                return core::Status::Success();
            }
            char err[256]{};
            core.ErrorToString(ret, err, sizeof(err));
            RS_ERROR(rs::LogChannel::Exception,
                     L"WMX3 " << what << L" failed: 0x" << std::hex << ret);
            return core::Status{ MakeError(ret, std::string{ what } + ": " + err) };
        }

        core::Result<CoreMotionStatus> Status()
        {
            CoreMotionStatus st{};
            int ret = core.GetStatus(&st);
            if (ret != 0)
            {
                char err[256]{};
                core.ErrorToString(ret, err, sizeof(err));
                return core::Result<CoreMotionStatus>{ MakeError(ret, std::string{ "GetStatus: " } + err) };
            }
            return core::Result<CoreMotionStatus>{ st };
        }

        // 위치 지령 생성. 가감속은 "시간(ms)" 규약 — 가속도 = 속도 / (time/1000).
        Motion::PosCommand MakePosCommand(hal::AxisId axis, double target,
                                          double velocity, double accMs, double decMs)
        {
            Wmx3AxisParam const p = ParamOf(axis);
            Motion::PosCommand cmd = Motion::PosCommand();

            cmd.axis                = static_cast<int>(axis);
            cmd.target              = target * p.unitScale;
            cmd.profile.type        = ProfileType::Trapezoidal;
            cmd.profile.velocity    = velocity * p.unitScale;
            cmd.profile.acc         = (velocity * p.unitScale) / (accMs / 1000.0);
            cmd.profile.dec         = (velocity * p.unitScale) / (decMs / 1000.0);
            cmd.profile.jerkAccRatio = 0.5;
            cmd.profile.jerkDecRatio = 0.5;
            return cmd;
        }
    };

#else  // SDK 미설치 — 호출을 모두 SdkNotInstalled 로 되돌리는 스텁

    struct Wmx3MotionController::Impl
    {
        mutable std::recursive_mutex          mutex;
        bool                                  open{ false };
        std::map<hal::AxisId, Wmx3AxisParam>  params;

        Wmx3AxisParam ParamOf(hal::AxisId axis) const
        {
            auto it = params.find(axis);
            return it != params.end() ? it->second : Wmx3AxisParam{};
        }
    };

#endif

    Wmx3MotionController::Wmx3MotionController()
        : m_impl(std::make_unique<Impl>())
    {
    }

    Wmx3MotionController::~Wmx3MotionController()
    {
        Close();
    }

    Wmx3MotionController& Wmx3MotionController::Instance()
    {
        static Wmx3MotionController s_instance;
        return s_instance;
    }

    bool Wmx3MotionController::IsSdkAvailable() noexcept
    {
        return RS_WMX3_SDK != 0;
    }

    bool Wmx3MotionController::IsOpen() const noexcept
    {
        std::lock_guard lock(m_impl->mutex);
        return m_impl->open;
    }

    core::Status Wmx3MotionController::SetAxisParam(hal::AxisId axis, Wmx3AxisParam const& param)
    {
        std::lock_guard lock(m_impl->mutex);
        m_impl->params[axis] = param;
        return core::Status::Success();
    }

    core::Result<Wmx3AxisParam> Wmx3MotionController::GetAxisParam(hal::AxisId axis) const
    {
        std::lock_guard lock(m_impl->mutex);
        return core::Result<Wmx3AxisParam>{ m_impl->ParamOf(axis) };
    }

#if RS_WMX3_SDK

    core::Status Wmx3MotionController::Open(std::string const& enginePath, std::string const& paramXmlPath)
    {
        std::lock_guard lock(m_impl->mutex);
        if (m_impl->open)
        {
            return core::Status::Success();
        }

        // CreateDevice 는 성공 시 0.
        int ret = m_impl->api.CreateDevice(enginePath.c_str(), DeviceType::DeviceTypeNormal, INFINITE);
        if (ret != 0)
        {
            return m_impl->Wrap(ret, "CreateDevice");
        }
        m_impl->api.SetDeviceName("RSolution.Motion");

        // StartCommunication 은 즉시 Communicating 이 되지 않아 상태를 폴링하며 재시도한다.
        bool communicating = false;
        for (int i = 0; i < 100 && !communicating; ++i)
        {
            m_impl->api.StartCommunication(INFINITE);

            EngineStatus es{};
            m_impl->api.GetEngineStatus(&es);
            communicating = (es.state == EngineState::Communicating);
        }
        if (!communicating)
        {
            m_impl->api.CloseDevice();
            RS_ERROR(rs::LogChannel::Exception, L"WMX3 engine did not reach Communicating");
            return core::Status{ MakeError(wmx3_errc::EngineNotReady,
                                           "WMX3 engine did not reach Communicating state") };
        }

        if (!paramXmlPath.empty())
        {
            ret = m_impl->core.config->ImportAndSetAll(paramXmlPath.c_str());
            if (ret != 0)
            {
                RS_WARN(rs::LogChannel::Exception, L"WMX3 ImportAndSetAll failed: 0x" << std::hex << ret);
            }
        }

        m_impl->open = true;
        RS_INFO(rs::LogChannel::Info, L"WMX3 engine opened");
        return core::Status::Success();
    }

    void Wmx3MotionController::Close()
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return;
        }
        m_impl->api.StopCommunication(INFINITE);
        m_impl->api.CloseDevice();
        m_impl->open = false;
        RS_INFO(rs::LogChannel::Info, L"WMX3 engine closed");
    }

    core::Status Wmx3MotionController::Initialize(hal::AxisId axis)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        Wmx3AxisParam const p = m_impl->ParamOf(axis);
        int const a = static_cast<int>(axis);

        // 원점 복귀 속도 반영. 나머지 홈 파라미터를 지우지 않도록 읽어서 속도만 갱신한다.
        Config::HomeParam homeParam{};
        int ret = m_impl->core.config->GetHomeParam(a, &homeParam);
        if (ret != 0)
        {
            return m_impl->Wrap(ret, "GetHomeParam");
        }
        homeParam.homingVelocityFast = p.homeVelocity * p.unitScale;
        ret = m_impl->core.config->SetHomeParam(a, &homeParam);
        if (ret != 0)
        {
            return m_impl->Wrap(ret, "SetHomeParam");
        }

        // 인포지션 폭 반영.
        Config::SystemParam sysParam{};
        sysParam.feedbackParam->inPosWidth = p.inPosRange * p.unitScale;
        ret = m_impl->core.config->SetParam(a, &sysParam);
        if (ret != 0)
        {
            return m_impl->Wrap(ret, "SetParam(inPosWidth)");
        }

        RS_INFO(rs::LogChannel::Info, L"WMX3 axis " << a << L" initialized");
        return core::Status::Success();
    }

    core::Status Wmx3MotionController::EnableServo(hal::AxisId axis, bool on)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        int const a = static_cast<int>(axis);
        int ret = m_impl->core.axisControl->SetServoOn(a, on ? 1 : 0);
        if (ret != 0)
        {
            return m_impl->Wrap(ret, "SetServoOn");
        }

        // SetServoOn 반환과 실제 서보 상태 변화 사이에 지연이 있어 상태로 재확인한다.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto st = m_impl->Status();
        if (st.ok() && st.value().axesStatus[a].servoOn != on)
        {
            return core::Status{ MakeError(wmx3_errc::ServoAlarm,
                                           on ? "servo did not turn on" : "servo did not turn off") };
        }
        return core::Status::Success();
    }

    core::Status Wmx3MotionController::Home(hal::AxisId axis)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }
        return m_impl->Wrap(m_impl->core.home->StartHome(static_cast<int>(axis)), "StartHome");
    }

    core::Status Wmx3MotionController::MoveAbsolute(hal::AxisId axis, double position, double velocity)
    {
        Wmx3AxisParam const p = [&] { std::lock_guard lock(m_impl->mutex); return m_impl->ParamOf(axis); }();
        return MoveAbsoluteEx(axis, position, velocity, p.accTimeMs, p.decTimeMs);
    }

    core::Status Wmx3MotionController::MoveRelative(hal::AxisId axis, double distance, double velocity)
    {
        Wmx3AxisParam const p = [&] { std::lock_guard lock(m_impl->mutex); return m_impl->ParamOf(axis); }();
        return MoveRelativeEx(axis, distance, velocity, p.accTimeMs, p.decTimeMs);
    }

    core::Status Wmx3MotionController::MoveAbsoluteEx(hal::AxisId axis, double position, double velocity,
                                                      double accTimeMs, double decTimeMs)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }
        auto cmd = m_impl->MakePosCommand(axis, position, velocity, accTimeMs, decTimeMs);
        return m_impl->Wrap(m_impl->core.motion->StartPos(&cmd), "StartPos");
    }

    core::Status Wmx3MotionController::MoveRelativeEx(hal::AxisId axis, double distance, double velocity,
                                                      double accTimeMs, double decTimeMs)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }
        auto cmd = m_impl->MakePosCommand(axis, distance, velocity, accTimeMs, decTimeMs);
        return m_impl->Wrap(m_impl->core.motion->StartMov(&cmd), "StartMov");
    }

    core::Status Wmx3MotionController::Stop(hal::AxisId axis)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        auto st = m_impl->Status();
        if (!st.ok())
        {
            return core::Status{ st.error() };
        }
        if (st.value().axesStatus[static_cast<int>(axis)].opState == OperationState::Idle)
        {
            return core::Status::Success();
        }
        return m_impl->Wrap(m_impl->core.motion->Stop(static_cast<int>(axis)), "Stop");
    }

    core::Status Wmx3MotionController::StartJog(hal::AxisId axis, bool forward, bool fast)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        Wmx3AxisParam const p = m_impl->ParamOf(axis);
        double const velocity = (fast ? p.jogHighVelocity : p.jogLowVelocity) * p.unitScale;

        Motion::JogCommand cmd = Motion::JogCommand();
        cmd.axis                 = static_cast<int>(axis);
        cmd.profile.type         = ProfileType::JerkRatio;
        cmd.profile.velocity     = forward ? velocity : -velocity;
        cmd.profile.acc          = velocity / (p.accTimeMs / 1000.0);
        cmd.profile.dec          = velocity / (p.decTimeMs / 1000.0);
        cmd.profile.jerkAccRatio = 0.5;
        cmd.profile.jerkDecRatio = 0.5;

        return m_impl->Wrap(m_impl->core.motion->StartJog(&cmd), "StartJog");
    }

    core::Status Wmx3MotionController::StopJog(hal::AxisId axis)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }
        return m_impl->Wrap(m_impl->core.motion->Stop(static_cast<int>(axis)), "Stop(jog)");
    }

    core::Status Wmx3MotionController::JogStep(hal::AxisId axis, double step, bool fast)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        auto st = m_impl->Status();
        if (!st.ok())
        {
            return core::Status{ st.error() };
        }

        Wmx3AxisParam const p = m_impl->ParamOf(axis);
        double const current = st.value().axesStatus[static_cast<int>(axis)].actualPos / p.unitScale;
        double const velocity = fast ? p.jogHighVelocity : p.jogLowVelocity;

        return MoveAbsoluteEx(axis, current + step, velocity, p.accTimeMs, p.decTimeMs);
    }

    core::Status Wmx3MotionController::AlarmReset(hal::AxisId axis)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        int const a = static_cast<int>(axis);
        auto st = m_impl->Status();
        if (!st.ok())
        {
            return core::Status{ st.error() };
        }
        if (!st.value().axesStatus[a].ampAlarm)
        {
            return core::Status::Success();
        }

        // 축 알람을 먼저 지우고, 그래도 남으면 앰프 알람을 지운다(순서를 바꾸면 앰프가 다시 올린다).
        int ret = m_impl->core.axisControl->ClearAxisAlarm(a);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        st = m_impl->Status();
        if (st.ok() && st.value().axesStatus[a].ampAlarm)
        {
            ret = m_impl->core.axisControl->ClearAmpAlarm(a);
        }
        return m_impl->Wrap(ret, "ClearAlarm");
    }

    core::Status Wmx3MotionController::ClearHomeDone(hal::AxisId axis)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }
        return m_impl->Wrap(m_impl->core.home->SetHomeDone(static_cast<int>(axis), 0), "SetHomeDone");
    }

    core::Status Wmx3MotionController::EmergencyStop()
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }
        RS_WARN(rs::LogChannel::Event, L"WMX3 emergency stop");
        return m_impl->Wrap(m_impl->core.ExecEStop(EStopLevel::Final), "ExecEStop");
    }

    core::Status Wmx3MotionController::ReleaseEmergencyStop()
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }
        return m_impl->Wrap(m_impl->core.ReleaseEStop(), "ReleaseEStop");
    }

    core::Status Wmx3MotionController::StopAll()
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        auto st = m_impl->Status();
        if (!st.ok())
        {
            return core::Status{ st.error() };
        }

        // 움직이는 축만 정지한다.
        for (int i = 0; i < constants::maxAxes; ++i)
        {
            if (st.value().axesStatus[i].opState != OperationState::Idle)
            {
                m_impl->core.motion->Stop(i);
            }
        }
        return core::Status::Success();
    }

    core::Status Wmx3MotionController::WaitMoveDone(hal::AxisId axis, int timeoutMs)
    {
        int const a = static_cast<int>(axis);
        auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        for (;;)
        {
            core::Result<CoreMotionStatus> st{ core::Error{} };
            {
                std::lock_guard lock(m_impl->mutex);
                if (!m_impl->open)
                {
                    return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
                }
                st = m_impl->Status();
            }
            if (!st.ok())
            {
                return core::Status{ st.error() };
            }

            auto const& ax = st.value().axesStatus[a];

            // 이상 상황은 전 축 정지 후 즉시 반환한다.
            if (ax.ampAlarm)
            {
                StopAll();
                return core::Status{ MakeError(wmx3_errc::ServoAlarm, "amp alarm during move") };
            }
            if (ax.servoOffline)
            {
                StopAll();
                return core::Status{ MakeError(wmx3_errc::ServoOffline, "servo offline during move") };
            }
            if (ax.positiveLS || ax.negativeLS)
            {
                StopAll();
                return core::Status{ MakeError(wmx3_errc::LimitHit, "limit switch hit during move") };
            }
            if (ax.opState == OperationState::Idle && ax.inPos)
            {
                return core::Status::Success();
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                StopAll();
                return core::Status{ MakeError(wmx3_errc::Timeout, "move did not complete in time") };
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    core::Result<hal::AxisStatus> Wmx3MotionController::GetStatus(hal::AxisId axis)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Result<hal::AxisStatus>{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        auto st = m_impl->Status();
        if (!st.ok())
        {
            return core::Result<hal::AxisStatus>{ st.error() };
        }

        Wmx3AxisParam const p = m_impl->ParamOf(axis);
        auto const& ax = st.value().axesStatus[static_cast<int>(axis)];

        hal::AxisStatus out{};
        out.position = ax.actualPos / p.unitScale;
        out.servoOn  = ax.servoOn != 0;
        out.homed    = ax.homeDone != 0;

        if (ax.ampAlarm || ax.servoOffline)      out.state = hal::MotionState::Error;
        else if (!ax.servoOn)                    out.state = hal::MotionState::Disabled;
        else if (ax.opState == OperationState::Home ||
                 ax.opState == OperationState::GantryHome)
                                                 out.state = hal::MotionState::Homing;
        else if (ax.opState != OperationState::Idle)
                                                 out.state = hal::MotionState::Moving;
        else                                     out.state = hal::MotionState::Idle;

        return core::Result<hal::AxisStatus>{ out };
    }

    core::Result<Wmx3AxisDetail> Wmx3MotionController::GetDetail(hal::AxisId axis)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Result<Wmx3AxisDetail>{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        auto st = m_impl->Status();
        if (!st.ok())
        {
            return core::Result<Wmx3AxisDetail>{ st.error() };
        }

        Wmx3AxisParam const p = m_impl->ParamOf(axis);
        auto const& ax = st.value().axesStatus[static_cast<int>(axis)];

        Wmx3AxisDetail d{};
        d.servoOn         = ax.servoOn != 0;
        d.servoAlarm      = ax.ampAlarm != 0;
        d.homeDone        = ax.homeDone != 0;
        d.inPosition      = ax.inPos != 0;
        d.positiveLimit   = ax.positiveLS != 0;
        d.negativeLimit   = ax.negativeLS != 0;
        d.homeSensor      = ax.homeSwitch != 0;
        d.commandPosition = ax.posCmd / p.unitScale;
        d.actualPosition  = ax.actualPos / p.unitScale;
        d.actualTorque    = ax.actualTorque;

        switch (ax.opState)
        {
        case OperationState::Idle:                d.opState = L"Idle"; break;
        case OperationState::Pos:                 d.opState = L"Pos"; break;
        case OperationState::Jog:                 d.opState = L"Jog"; break;
        case OperationState::Home:                d.opState = L"Home"; break;
        case OperationState::Sync:                d.opState = L"Sync"; break;
        case OperationState::GantryHome:          d.opState = L"GantryHome"; break;
        case OperationState::Stop:                d.opState = L"Stop"; break;
        case OperationState::Intpl:               d.opState = L"Intpl"; break;
        case OperationState::Velocity:            d.opState = L"Velocity"; break;
        case OperationState::ConstLinearVelocity: d.opState = L"ConstLinearVelocity"; break;
        case OperationState::Trq:                 d.opState = L"Trq"; break;
        case OperationState::DirectControl:       d.opState = L"DirectControl"; break;
        case OperationState::PVT:                 d.opState = L"PVT"; break;
        case OperationState::ECAM:                d.opState = L"ECAM"; break;
        case OperationState::SyncCatchUp:         d.opState = L"SyncCatchUp"; break;
        case OperationState::DancerControl:       d.opState = L"DancerControl"; break;
        default:                                  d.opState = L"Unknown"; break;
        }

        switch (ax.homeState)
        {
        case HomeState::Idle:                           d.homeState = d.homeDone ? L"Complete" : L"Idle"; break;
        case HomeState::ZPulseSearch:                   d.homeState = L"ZPulseSearch"; break;
        case HomeState::ZPulseSearchReverse:            d.homeState = L"ZPulseSearchReverse"; break;
        case HomeState::ZPulseSearchPauseReverse:       d.homeState = L"ZPulseSearchPauseReverse"; break;
        case HomeState::HSSearch:                       d.homeState = L"HSSearch"; break;
        case HomeState::HSSearchPause:                  d.homeState = L"HSSearchPause"; break;
        case HomeState::HSAndZPulseSearch:              d.homeState = L"HSAndZPulseSearch"; break;
        case HomeState::HSAndZPulseSearchPause:         d.homeState = L"HSAndZPulseSearchPause"; break;
        case HomeState::HSOffSearch:                    d.homeState = L"HSOffSearch"; break;
        case HomeState::HSOffSearchPause:               d.homeState = L"HSOffSearchPause"; break;
        case HomeState::HSOffAndZPulseSearch:           d.homeState = L"HSOffAndZPulseSearch"; break;
        case HomeState::HSOffAndZPulseSearchPause:      d.homeState = L"HSOffAndZPulseSearchPause"; break;
        case HomeState::LSSearch:                       d.homeState = L"LSSearch"; break;
        case HomeState::LSSearchPause:                  d.homeState = L"LSSearchPause"; break;
        case HomeState::HSClearReverse:                 d.homeState = L"HSClearReverse"; break;
        case HomeState::HSClearReversePause:            d.homeState = L"HSClearReversePause"; break;
        case HomeState::HSFallingEdgeSearchReverse:     d.homeState = L"HSFallingEdgeSearchReverse"; break;
        case HomeState::HSFallingEdgeSearchReversePause:d.homeState = L"HSFallingEdgeSearchReversePause"; break;
        case HomeState::LSFallingEdgeSearchReverse:     d.homeState = L"LSFallingEdgeSearchReverse"; break;
        case HomeState::LSFallingEdgeSearchReversePause:d.homeState = L"LSFallingEdgeSearchReversePause"; break;
        case HomeState::TouchProbeSearch:               d.homeState = L"TouchProbeSearch"; break;
        case HomeState::TouchProbeSearchPause:          d.homeState = L"TouchProbeSearchPause"; break;
        case HomeState::SecondHSSearch:                 d.homeState = L"SecondHSSearch"; break;
        case HomeState::SecondHSSearchPause:            d.homeState = L"SecondHSSearchPause"; break;
        case HomeState::SecondTouchProbeSearch:         d.homeState = L"SecondTouchProbeSearch"; break;
        case HomeState::SecondTouchProbeSearchPause:    d.homeState = L"SecondTouchProbeSearchPause"; break;
        case HomeState::MechanicalEndDetection:         d.homeState = L"MechanicalEndDetection"; break;
        case HomeState::HomeShift:                      d.homeState = L"HomeShift"; break;
        case HomeState::HomeShiftPause:                 d.homeState = L"HomeShiftPause"; break;
        case HomeState::Cancel:                         d.homeState = L"Cancel"; break;
        default:                                        d.homeState = L"Unknown"; break;
        }

        return core::Result<Wmx3AxisDetail>{ d };
    }

    core::Status Wmx3MotionController::SetSync(hal::AxisId master, hal::AxisId slave)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }
        return m_impl->Wrap(m_impl->core.sync->SetSyncMasterSlave(static_cast<int>(master),
                                                                  static_cast<int>(slave)),
                            "SetSyncMasterSlave");
    }

    core::Status Wmx3MotionController::ResolveSync(hal::AxisId slave)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }
        return m_impl->Wrap(m_impl->core.sync->ResolveSync(static_cast<int>(slave)), "ResolveSync");
    }

    core::Result<Wmx3SlaveHealth> Wmx3MotionController::CheckSlaves()
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Result<Wmx3SlaveHealth>{ MakeError(wmx3_errc::NotOpen, "WMX3 engine is not open") };
        }

        EcMasterInfo info{};
        int ret = m_impl->ecat.GetMasterInfo(&info);
        if (ret != 0)
        {
            return core::Result<Wmx3SlaveHealth>{ MakeError(ret, "GetMasterInfo failed") };
        }

        Wmx3SlaveHealth h{};
        h.allOperational = true;
        h.onlineCount    = info.GetOnlineSlaveCount();

        for (unsigned int i = 0; i < info.numOfSlaves; ++i)
        {
            if (info.slaves[i].state != EcStateMachine::Op)
            {
                h.allOperational = false;
                if (info.slaves[i].state == EcStateMachine::None ||
                    info.slaves[i].state == EcStateMachine::Init)
                {
                    h.anyLost = true;
                }
            }
        }
        return core::Result<Wmx3SlaveHealth>{ h };
    }

#else  // ---- SDK 미설치 스텁 ----

    core::Status Wmx3MotionController::Open(std::string const&, std::string const&) { return SdkMissing(); }
    void         Wmx3MotionController::Close() {}

    core::Status Wmx3MotionController::Initialize(hal::AxisId)                        { return SdkMissing(); }
    core::Status Wmx3MotionController::EnableServo(hal::AxisId, bool)                 { return SdkMissing(); }
    core::Status Wmx3MotionController::Home(hal::AxisId)                              { return SdkMissing(); }
    core::Status Wmx3MotionController::MoveAbsolute(hal::AxisId, double, double)      { return SdkMissing(); }
    core::Status Wmx3MotionController::MoveRelative(hal::AxisId, double, double)      { return SdkMissing(); }
    core::Status Wmx3MotionController::MoveAbsoluteEx(hal::AxisId, double, double, double, double) { return SdkMissing(); }
    core::Status Wmx3MotionController::MoveRelativeEx(hal::AxisId, double, double, double, double) { return SdkMissing(); }
    core::Status Wmx3MotionController::Stop(hal::AxisId)                              { return SdkMissing(); }
    core::Status Wmx3MotionController::StartJog(hal::AxisId, bool, bool)              { return SdkMissing(); }
    core::Status Wmx3MotionController::StopJog(hal::AxisId)                           { return SdkMissing(); }
    core::Status Wmx3MotionController::JogStep(hal::AxisId, double, bool)             { return SdkMissing(); }
    core::Status Wmx3MotionController::AlarmReset(hal::AxisId)                        { return SdkMissing(); }
    core::Status Wmx3MotionController::ClearHomeDone(hal::AxisId)                     { return SdkMissing(); }
    core::Status Wmx3MotionController::EmergencyStop()                                { return SdkMissing(); }
    core::Status Wmx3MotionController::ReleaseEmergencyStop()                         { return SdkMissing(); }
    core::Status Wmx3MotionController::StopAll()                                      { return SdkMissing(); }
    core::Status Wmx3MotionController::WaitMoveDone(hal::AxisId, int)                 { return SdkMissing(); }
    core::Status Wmx3MotionController::SetSync(hal::AxisId, hal::AxisId)              { return SdkMissing(); }
    core::Status Wmx3MotionController::ResolveSync(hal::AxisId)                       { return SdkMissing(); }

    core::Result<hal::AxisStatus> Wmx3MotionController::GetStatus(hal::AxisId)
    {
        return core::Result<hal::AxisStatus>{ SdkMissing().error() };
    }

    core::Result<Wmx3AxisDetail> Wmx3MotionController::GetDetail(hal::AxisId)
    {
        return core::Result<Wmx3AxisDetail>{ SdkMissing().error() };
    }

    core::Result<Wmx3SlaveHealth> Wmx3MotionController::CheckSlaves()
    {
        return core::Result<Wmx3SlaveHealth>{ SdkMissing().error() };
    }

#endif
}
