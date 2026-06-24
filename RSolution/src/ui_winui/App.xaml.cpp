#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "core/infrastructure/logging/LogManager.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::RSolution::implementation
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
        // App.xaml 의 병합 리소스(XamlControlsResources 등)를 로드한다.
        // 이 호출이 없으면 WinUI 컨트롤의 기본 스타일을 찾지 못해 hresult_error 가 발생한다.
        InitializeComponent();

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    /// <summary>
    /// Invoked when the application is launched.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        // 설정 파일(LogManagerSystemParam.json)을 읽어 채널을 구성. 없으면 기본값.
        rs::LogManager::Instance().LoadConfig(L"");
#ifdef _DEBUG
        rs::LogManager::Instance().SetMinLevel(rs::LogLevel::Trace);
#endif
        RS_INFO(rs::LogChannel::Info, L"RSolution 애플리케이션 시작");

        window = make<MainWindow>();
        window.Activate();
    }
}
