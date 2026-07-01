#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "core/infrastructure/logging/LogManager.h"
#include "core/domain/EquipmentStateMachine.h"
#include "tests/SelfTests.h"

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

#ifdef _DEBUG
        // 설계서 12(테스트 전략) 단위 테스트를 Debug 빌드마다 자동 실행한다. 전역 싱글톤을
        // 건드리지 않는 격리된 인스턴스만 검사하므로 실제 앱 동작에 영향을 주지 않는다.
        rs::tests::RunSelfTests();
#endif

        // 설계서 7.1.5: 상태 전이는 단일 진입점(RequestTransition)을 경유하므로,
        // 여기서 모든 전이(이전/다음 상태, 요청자, 사유, 허용 여부)를 Process 채널에 기록한다.
        rs::domain::EquipmentStateMachine::Instance().SetListener(
            [](rs::domain::TransitionResult const& r, std::wstring_view who, std::wstring_view why)
            {
                std::wostringstream oss;
                oss << rs::domain::ToString(r.from) << L" -> " << rs::domain::ToString(r.to)
                    << L" | allowed=" << (r.allowed ? L"Y" : L"N")
                    << L" | by=" << who << L" | reason=" << why;
                if (!r.reason.empty())
                    oss << L" | " << r.reason;
                RS_INFO(rs::LogChannel::Process, oss.str());
            });

        window = make<MainWindow>();
        window.Activate();
    }
}
