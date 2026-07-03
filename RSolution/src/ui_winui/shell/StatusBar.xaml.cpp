#include "pch.h"
#include "StatusBar.xaml.h"
#if __has_include("StatusBar.g.cpp")
#include "StatusBar.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    Windows::UI::Color FromRgb(uint8_t r, uint8_t g, uint8_t b) noexcept
    {
        return Windows::UI::Color{ 0xFF, r, g, b };
    }
}

namespace winrt::RSolution::implementation
{
    void StatusBar::SetAlarm(hstring const& message, hstring const& severity)
    {
        AlarmText().Text(message);

        // UI 문서 5.3: Info/Warning/Major/Fatal 표준 표기. 배경·아이콘·텍스트 3중 표기(5.1 원칙).
        Windows::UI::Color back = FromRgb(0xEB, 0xF5, 0xFB);   // Info: 연한 파랑
        Windows::UI::Color fore = FromRgb(0x24, 0x71, 0xA3);
        if (severity == L"Warning")
        {
            back = FromRgb(0xFE, 0xF9, 0xE7);
            fore = FromRgb(0xB7, 0x95, 0x0B);
        }
        else if (severity == L"Major" || severity == L"Fatal")
        {
            back = FromRgb(0xFD, 0xEC, 0xEA);
            fore = FromRgb(0xC0, 0x39, 0x2B);
        }

        RootBar().Background(Media::SolidColorBrush{ back });
        AlarmText().Foreground(Media::SolidColorBrush{ fore });
        AlarmIcon().Foreground(Media::SolidColorBrush{ fore });
    }

    void StatusBar::ClearAlarm()
    {
        AlarmText().Text(L"No Active Alarm");
        RootBar().Background(Media::SolidColorBrush{ FromRgb(0xEC, 0xF0, 0xF1) });
        AlarmText().Foreground(Media::SolidColorBrush{ FromRgb(0x5D, 0x6D, 0x7E) });
        AlarmIcon().Foreground(Media::SolidColorBrush{ FromRgb(0x7F, 0x8C, 0x99) });
    }

    // TODO(Application 연계): 아래 핸들러는 Application 계층 명령 API(알람 Ack/Reset)로 연결한다.
    // UI 는 명령 API 만 호출하고 제어 모듈을 직접 호출하지 않는다(UI 문서 7장).
    void StatusBar::AckClick(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
    }

    void StatusBar::ResetClick(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
    }

    void StatusBar::BuzzerOffClick(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
    }
}
