#pragma once

#include "OperationPage.g.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <cstdint>

namespace winrt::RSolution::implementation
{
    struct OperationPage : OperationPageT<OperationPage>
    {
        OperationPage();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        // 알람 발생 여부에 따라 하단 바 색상/문구를 바꾼다(발생=빨강, 미발생=초록).
        void SetAlarm(bool active, winrt::hstring const& message);

    private:
        void OnTick(winrt::Windows::Foundation::IInspectable const& sender,
                    winrt::Windows::Foundation::IInspectable const& args);
        void UpdateClock();   // 하단 바에 현재 시각 표시

        winrt::Microsoft::UI::Xaml::DispatcherTimer m_timer{ nullptr };
        std::uint64_t m_alarmSub{ 0 };   // EventBus 구독 토큰(alarm.changed)
        std::uint64_t m_stateSub{ 0 };   // EventBus 구독 토큰(state.changed)
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct OperationPage : OperationPageT<OperationPage, implementation::OperationPage>
    {
    };
}
