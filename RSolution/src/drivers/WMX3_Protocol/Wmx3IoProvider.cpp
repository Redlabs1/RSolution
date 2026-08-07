#include "pch.h"
#include "Wmx3IoProvider.h"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "core/infrastructure/logging/LogManager.h"

#if __has_include(<WMX3Api.h>)
#  define RS_WMX3_SDK 1
#  include <WMX3Api.h>
#  include <IOApi.h>
#  pragma comment(lib, "WMX3Api.lib")
#  pragma comment(lib, "IOApi.lib")
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

        core::Error SdkMissingError()
        {
            return MakeError(wmx3_errc::SdkNotInstalled,
                             "WMX3 SDK not installed (rebuild after installing WMX3)");
        }

        // 통짜 비트 주소 → (포트 바이트, 포트 내 비트). WMX3 I/O API 가 이 형태를 요구한다.
        constexpr int PortOf(hal::IoPoint point) noexcept { return static_cast<int>(point / 8); }
        constexpr int BitOf(hal::IoPoint point)  noexcept { return static_cast<int>(point % 8); }
    }

    struct Wmx3IoProvider::Impl
    {
        struct Subscription
        {
            hal::IoPoint        point;
            hal::IoEventHandler handler;
        };

        mutable std::mutex mutex;
        bool               open{ false };

#if RS_WMX3_SDK
        wmx3Api::WMX3Api api;
        wmx3Api::Io      io{ &api };
#endif

        std::map<hal::IoSubscription, Subscription> subs;
        hal::IoSubscription                         nextSub{ 1 };

        // 폴링 워커: 구독이 하나라도 있으면 돌고, 없으면 멈춘다.
        std::jthread                    worker;
        std::atomic<std::chrono::milliseconds::rep> pollMs{ 20 };
        std::map<hal::IoPoint, bool>    lastValue;
    };

    Wmx3IoProvider::Wmx3IoProvider()
        : m_impl(std::make_unique<Impl>())
    {
    }

    Wmx3IoProvider::~Wmx3IoProvider()
    {
        // 워커를 먼저 정리해야 Close 이후 SDK 를 건드리지 않는다.
        if (m_impl->worker.joinable())
        {
            m_impl->worker.request_stop();
            m_impl->worker.join();
        }
        Close();
    }

    Wmx3IoProvider& Wmx3IoProvider::Instance()
    {
        static Wmx3IoProvider s_instance;
        return s_instance;
    }

    bool Wmx3IoProvider::IsOpen() const noexcept
    {
        std::lock_guard lock(m_impl->mutex);
        return m_impl->open;
    }

    void Wmx3IoProvider::SetPollInterval(std::chrono::milliseconds interval)
    {
        m_impl->pollMs.store(interval.count() > 0 ? interval.count() : 1);
    }

    hal::IoSubscription Wmx3IoProvider::Subscribe(hal::IoPoint point, hal::IoEventHandler handler)
    {
        hal::IoSubscription id;
        bool startWorker = false;
        {
            std::lock_guard lock(m_impl->mutex);
            id = m_impl->nextSub++;
            startWorker = m_impl->subs.empty();
            m_impl->subs[id] = Impl::Subscription{ point, std::move(handler) };
        }

        if (startWorker && !m_impl->worker.joinable())
        {
            m_impl->worker = std::jthread([this](std::stop_token stop)
            {
                while (!stop.stop_requested())
                {
                    // 구독 목록 스냅샷을 뜬 뒤 잠금 밖에서 읽고 통지한다(핸들러 재진입 방지).
                    std::vector<Impl::Subscription> snapshot;
                    {
                        std::lock_guard lock(m_impl->mutex);
                        if (!m_impl->open || m_impl->subs.empty())
                        {
                            snapshot.clear();
                        }
                        else
                        {
                            snapshot.reserve(m_impl->subs.size());
                            for (auto const& [id, sub] : m_impl->subs)
                            {
                                snapshot.push_back(sub);
                            }
                        }
                    }

                    for (auto const& sub : snapshot)
                    {
                        auto r = ReadDigital(sub.point);
                        if (!r.ok())
                        {
                            continue;
                        }

                        bool changed = false;
                        {
                            std::lock_guard lock(m_impl->mutex);
                            auto it = m_impl->lastValue.find(sub.point);
                            changed = (it == m_impl->lastValue.end()) || (it->second != r.value());
                            m_impl->lastValue[sub.point] = r.value();
                        }
                        if (changed)
                        {
                            try { sub.handler(hal::IoChangeEvent{ sub.point, r.value() }); }
                            catch (...) {}
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(m_impl->pollMs.load()));
                }
            });
        }
        return id;
    }

    void Wmx3IoProvider::Unsubscribe(hal::IoSubscription token)
    {
        bool stopWorker = false;
        {
            std::lock_guard lock(m_impl->mutex);
            m_impl->subs.erase(token);
            stopWorker = m_impl->subs.empty();
        }
        if (stopWorker && m_impl->worker.joinable())
        {
            m_impl->worker.request_stop();
            m_impl->worker.join();
            m_impl->worker = std::jthread{};
        }
    }

#if RS_WMX3_SDK

    core::Status Wmx3IoProvider::Open(std::string const& enginePath)
    {
        std::lock_guard lock(m_impl->mutex);
        if (m_impl->open)
        {
            return core::Status::Success();
        }

        int ret = m_impl->api.CreateDevice(enginePath.c_str(),
                                           wmx3Api::DeviceType::DeviceTypeNormal, INFINITE);
        if (ret != 0)
        {
            RS_ERROR(rs::LogChannel::Exception, L"WMX3 IO CreateDevice failed: 0x" << std::hex << ret);
            return core::Status{ MakeError(ret, "WMX3 IO CreateDevice failed") };
        }
        m_impl->api.SetDeviceName("RSolution.IO");

        bool communicating = false;
        for (int i = 0; i < 100 && !communicating; ++i)
        {
            m_impl->api.StartCommunication(INFINITE);

            wmx3Api::EngineStatus es{};
            m_impl->api.GetEngineStatus(&es);
            communicating = (es.state == wmx3Api::EngineState::Communicating);
        }
        if (!communicating)
        {
            m_impl->api.CloseDevice();
            RS_ERROR(rs::LogChannel::Exception, L"WMX3 IO engine did not reach Communicating");
            return core::Status{ MakeError(wmx3_errc::EngineNotReady,
                                           "WMX3 IO engine did not reach Communicating state") };
        }

        m_impl->open = true;
        RS_INFO(rs::LogChannel::Info, L"WMX3 IO opened");
        return core::Status::Success();
    }

    void Wmx3IoProvider::Close()
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return;
        }
        m_impl->api.StopCommunication(INFINITE);
        m_impl->api.CloseDevice();
        m_impl->open = false;
        RS_INFO(rs::LogChannel::Info, L"WMX3 IO closed");
    }

    core::Result<bool> Wmx3IoProvider::ReadDigital(hal::IoPoint point)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Result<bool>{ MakeError(wmx3_errc::NotOpen, "WMX3 IO is not open") };
        }

        unsigned char value = 0;
        int ret = m_impl->io.GetInBit(PortOf(point), BitOf(point), &value);
        if (ret != 0)
        {
            return core::Result<bool>{ MakeError(ret, "GetInBit failed") };
        }
        return core::Result<bool>{ value != 0 };
    }

    core::Result<bool> Wmx3IoProvider::ReadDigitalOutput(hal::IoPoint point)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Result<bool>{ MakeError(wmx3_errc::NotOpen, "WMX3 IO is not open") };
        }

        unsigned char value = 0;
        int ret = m_impl->io.GetOutBit(PortOf(point), BitOf(point), &value);
        if (ret != 0)
        {
            return core::Result<bool>{ MakeError(ret, "GetOutBit failed") };
        }
        return core::Result<bool>{ value != 0 };
    }

    core::Status Wmx3IoProvider::WriteDigital(hal::IoPoint point, bool value)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 IO is not open") };
        }

        int ret = m_impl->io.SetOutBit(PortOf(point), BitOf(point), value ? 1 : 0);
        if (ret != 0)
        {
            return core::Status{ MakeError(ret, "SetOutBit failed") };
        }
        return core::Status::Success();
    }

    core::Result<double> Wmx3IoProvider::ReadAnalog(hal::IoPoint point)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Result<double>{ MakeError(wmx3_errc::NotOpen, "WMX3 IO is not open") };
        }

        int raw = 0;
        int ret = m_impl->io.GetInAnalogDataInt(static_cast<int>(point), &raw);
        if (ret != 0)
        {
            return core::Result<double>{ MakeError(ret, "GetInAnalogDataInt failed") };
        }
        return core::Result<double>{ static_cast<double>(raw) };
    }

    core::Status Wmx3IoProvider::WriteAnalog(hal::IoPoint point, double value)
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->open)
        {
            return core::Status{ MakeError(wmx3_errc::NotOpen, "WMX3 IO is not open") };
        }

        int ret = m_impl->io.SetOutAnalogDataInt(static_cast<int>(point), static_cast<int>(value));
        if (ret != 0)
        {
            return core::Status{ MakeError(ret, "SetOutAnalogDataInt failed") };
        }
        return core::Status::Success();
    }

#else  // ---- SDK 미설치 스텁 ----

    core::Status Wmx3IoProvider::Open(std::string const&) { return core::Status{ SdkMissingError() }; }
    void         Wmx3IoProvider::Close() {}

    core::Result<bool> Wmx3IoProvider::ReadDigital(hal::IoPoint)
    {
        return core::Result<bool>{ SdkMissingError() };
    }

    core::Result<bool> Wmx3IoProvider::ReadDigitalOutput(hal::IoPoint)
    {
        return core::Result<bool>{ SdkMissingError() };
    }

    core::Status Wmx3IoProvider::WriteDigital(hal::IoPoint, bool)
    {
        return core::Status{ SdkMissingError() };
    }

    core::Result<double> Wmx3IoProvider::ReadAnalog(hal::IoPoint)
    {
        return core::Result<double>{ SdkMissingError() };
    }

    core::Status Wmx3IoProvider::WriteAnalog(hal::IoPoint, double)
    {
        return core::Status{ SdkMissingError() };
    }

#endif
}
