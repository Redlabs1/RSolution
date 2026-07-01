#include "pch.h"
#include "OperationPage.xaml.h"
#if __has_include("OperationPage.g.cpp")
#include "OperationPage.g.cpp"
#endif

#include <windows.h>
#include <chrono>
#include <string>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "core/common/EventBus.h"
#include "core/domain/AlarmService.h"
#include "core/domain/EquipmentStateMachine.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    winrt::hstring Utf8ToHstring(std::string const& s)
    {
        if (s.empty()) return {};
        const int len = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring w(static_cast<size_t>(len), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), len);
        return winrt::hstring(w);
    }

    // 발생=빨강(#D9534F), 미발생=초록(#5BBA6F)
    void ApplyAlarm(Microsoft::UI::Xaml::Controls::Border const& bar,
                    Microsoft::UI::Xaml::Controls::TextBlock const& text,
                    bool active, winrt::hstring const& message)
    {
        const winrt::Windows::UI::Color color = active
            ? winrt::Windows::UI::Color{ 255, 0xD9, 0x53, 0x4F }
            : winrt::Windows::UI::Color{ 255, 0x5B, 0xBA, 0x6F };
        bar.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(color));
        text.Text(active ? message : winrt::hstring(L"No active alarm"));
    }
}

namespace winrt::RSolution::implementation
{
    OperationPage::OperationPage()
    {
        InitializeComponent();

        // 하단 알람 바에 현재 시각을 1초마다 갱신해 표시한다.
        UpdateClock();
        m_timer = DispatcherTimer();
        m_timer.Interval(std::chrono::seconds(1));
        m_timer.Tick({ get_weak(), &OperationPage::OnTick });
        m_timer.Start();

        // 현재 알람 상태를 반영(페이지 진입 시 이미 미해소 알람이 있을 수 있음).
        // Cleared(원인 해소)된 알람은 바에서 제외 — GetUnresolved() 는 Raised/Acknowledged 만 반환.
        auto unresolved = rs::domain::AlarmService::Instance().GetUnresolved();
        if (unresolved.ok() && !unresolved.value().empty())
            SetAlarm(true, Utf8ToHstring(unresolved.value().back().message));
        else
            SetAlarm(false, L"");

        // 알람 변경 이벤트 구독 → 임의 스레드에서 들어오면 UI 스레드로 마샬링해 바 갱신.
        auto bar = AlarmBar();
        auto text = AlarmText();
        auto dispatcher = DispatcherQueue();
        m_alarmSub = rs::core::EventBus::Instance().Subscribe(rs::domain::AlarmChangedTopic,
            [bar, text, dispatcher](rs::core::Event const& e)
            {
                const bool isActive = !e.payload.empty();
                const hstring msg = Utf8ToHstring(e.payload);
                if (dispatcher)
                {
                    dispatcher.TryEnqueue([bar, text, isActive, msg]()
                    {
                        ApplyAlarm(bar, text, isActive, msg);
                    });
                }
            });

        // 현재 장비 상태를 반영(진입 시점 상태를 즉시 표시).
        auto stateText = EquipmentStateText();
        {
            auto& sm = rs::domain::EquipmentStateMachine::Instance();
            stateText.Text(L"State: " + hstring(std::wstring(rs::domain::ToString(sm.State()))));
        }

        // 상태 변경 이벤트 구독(state.changed) → UI 스레드로 마샬링해 라벨 갱신.
        m_stateSub = rs::core::EventBus::Instance().Subscribe(rs::domain::StateChangedTopic,
            [stateText, dispatcher](rs::core::Event const& e)
            {
                const hstring msg = L"State: " + Utf8ToHstring(e.payload);
                if (dispatcher)
                {
                    dispatcher.TryEnqueue([stateText, msg]()
                    {
                        stateText.Text(msg);
                    });
                }
            });

        // 페이지가 내려가면 구독 해제.
        Unloaded([alarmToken = m_alarmSub, stateToken = m_stateSub](IInspectable const&, RoutedEventArgs const&)
        {
            rs::core::EventBus::Instance().Unsubscribe(alarmToken);
            rs::core::EventBus::Instance().Unsubscribe(stateToken);
        });
    }

    void OperationPage::SetAlarm(bool active, hstring const& message)
    {
        ApplyAlarm(AlarmBar(), AlarmText(), active, message);
    }

    void OperationPage::OnTick(Windows::Foundation::IInspectable const& /*sender*/,
                               Windows::Foundation::IInspectable const& /*args*/)
    {
        UpdateClock();
    }

    void OperationPage::UpdateClock()
    {
        SYSTEMTIME st{};
        ::GetLocalTime(&st);
        int hour12 = st.wHour % 12;
        if (hour12 == 0) hour12 = 12;
        const wchar_t* ampm = (st.wHour < 12) ? L"AM" : L"PM";

        wchar_t buf[48]{};
        // 년/월/일 - AM/PM 시:분:초
        ::swprintf_s(buf, L"%04d/%02d/%02d - %s %02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay, ampm, hour12, st.wMinute, st.wSecond);
        TimeText().Text(buf);
    }

    int32_t OperationPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void OperationPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
