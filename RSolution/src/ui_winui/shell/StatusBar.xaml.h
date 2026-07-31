#pragma once

#include "StatusBar.g.h"

// UI 문서 v2 2장: 하단 공통 알람 영역. 알람 등급별 색상 규칙은 UI 문서 5.3.
namespace winrt::RSolution::implementation
{
    struct StatusBar : StatusBarT<StatusBar>
    {
        StatusBar()
        {
            InitializeComponent();
        }

        // severity: "Info" | "Warning" | "Major" | "Fatal" (설계서 5.6 등급 체계)
        void SetAlarm(hstring const& message, hstring const& severity);
        void ClearAlarm();

        void AckClick(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void ResetClick(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void BuzzerOffClick(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct StatusBar : StatusBarT<StatusBar, implementation::StatusBar>
    {
    };
}
