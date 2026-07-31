#pragma once

#include "ManualPage.g.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "drivers/MC_Protocol/MC_Protocol.h"

namespace winrt::RSolution::implementation
{
    // Manual 페이지: PLC(MC 프로토콜) 연결 관리 + 워드/비트 영역 수동 읽기/쓰기 테스트.
    // 모든 통신은 백그라운드 스레드에서 수행하고, 결과만 DispatcherQueue 로 UI 에 반영한다.
    struct ManualPage : ManualPageT<ManualPage>
    {
        ManualPage();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        // ---- XAML 이벤트 핸들러 ----
        void OnSaveIpClick(winrt::Windows::Foundation::IInspectable const& sender,
                           winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnConnectClick(winrt::Windows::Foundation::IInspectable const& sender,
                            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnDisconnectClick(winrt::Windows::Foundation::IInspectable const& sender,
                               winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnLoopbackClick(winrt::Windows::Foundation::IInspectable const& sender,
                             winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnWordReadClick(winrt::Windows::Foundation::IInspectable const& sender,
                             winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnWordWriteClick(winrt::Windows::Foundation::IInspectable const& sender,
                              winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnBitReadClick(winrt::Windows::Foundation::IInspectable const& sender,
                            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnBitWriteClick(winrt::Windows::Foundation::IInspectable const& sender,
                             winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        // 연결 여부에 따라 버튼/표시등 상태 일괄 갱신 (UI 스레드에서 호출).
        void ApplyConnectionUi(bool connected, winrt::hstring const& detail);

        // 백그라운드 작업 실행 헬퍼: work 는 워커 스레드에서 실행하고,
        // 반환 문자열을 target(지정 시)과 StatusText 에 UI 스레드로 반영한다.
        void RunAsync(std::function<std::wstring()> work,
                      winrt::Microsoft::UI::Xaml::Controls::TextBlock const& target);

        rs::drivers::McDevice SelectedWordDevice();
        rs::drivers::McDevice SelectedBitDevice();

        std::shared_ptr<std::atomic_bool> m_busy = std::make_shared<std::atomic_bool>(false);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct ManualPage : ManualPageT<ManualPage, implementation::ManualPage>
    {
    };
}
