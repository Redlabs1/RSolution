#include "pch.h"
#include "HeaderPanel.xaml.h"
#if __has_include("HeaderPanel.g.cpp")
#include "HeaderPanel.g.cpp"
#endif

#include <ctime>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::RSolution::implementation
{
    HeaderPanel::HeaderPanel()
    {
        InitializeComponent();

        // 시계: 1초 주기 갱신. UI 스레드 타이머이므로 마샬링 불필요(UI 문서 7장).
        m_clockTimer = DispatcherTimer{};
        m_clockTimer.Interval(std::chrono::seconds{ 1 });
        m_clockTimer.Tick({ this, &HeaderPanel::OnTick });
        m_clockTimer.Start();
    }

    void HeaderPanel::OnTick(Windows::Foundation::IInspectable const&, Windows::Foundation::IInspectable const&)
    {
        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        wchar_t buf[32]{};
        std::wcsftime(buf, std::size(buf), L"%Y-%m-%d %H:%M:%S", &local);
        TimeText().Text(buf);
    }

    void HeaderPanel::SetEquipmentState(hstring const& stateText, hstring const& brushKey)
    {
        StateText().Text(stateText);

        // 상태 색상은 App.xaml 리소스에서 조회(UI 문서 5장: Brush 통합 관리). 키가 없으면 현재 색 유지.
        auto resources = Application::Current().Resources();
        auto key = box_value(brushKey);
        if (resources.HasKey(key))
        {
            if (auto brush = resources.Lookup(key).try_as<Media::Brush>())
            {
                StateBadge().Background(brush);
            }
        }
    }

    void HeaderPanel::SetMode(hstring const& modeText)
    {
        ModeText().Text(modeText);
    }

    void HeaderPanel::SetUser(hstring const& userText)
    {
        UserText().Text(userText);
    }

    void HeaderPanel::SetHostState(bool online)
    {
        HostText().Text(online ? L"Host: ONLINE" : L"Host: OFFLINE");
        HostText().Foreground(Media::SolidColorBrush{ online
            ? Windows::UI::Color{ 0xFF, 0x2E, 0xCC, 0x71 }    // 녹색(연결)
            : Windows::UI::Color{ 0xFF, 0xE6, 0x7E, 0x22 } }); // 주황(통신 이상, UI 문서 5장)
    }

    void HeaderPanel::SetPlcState(bool ok)
    {
        PlcText().Text(ok ? L"PLC: OK" : L"PLC: FAULT");
        PlcText().Foreground(Media::SolidColorBrush{ ok
            ? Windows::UI::Color{ 0xFF, 0x2E, 0xCC, 0x71 }
            : Windows::UI::Color{ 0xFF, 0xE6, 0x7E, 0x22 } });
    }
}
