#pragma once

#include "HeaderPanel.g.h"

// UI 문서 v2 2장: 모든 Page 에서 동일하게 유지되는 상단 공통 상태 영역.
// 표시 전용 컴포넌트이며 제어 모듈을 직접 호출하지 않는다(UI 문서 7장 데이터 흐름 규칙).
namespace winrt::RSolution::implementation
{
    struct HeaderPanel : HeaderPanelT<HeaderPanel>
    {
        HeaderPanel();

        // 장비 상태 배지. brushKey 는 App.xaml 의 상태 Brush 리소스 키(UI 문서 5.2 매핑표).
        void SetEquipmentState(hstring const& stateText, hstring const& brushKey);
        void SetMode(hstring const& modeText);
        void SetUser(hstring const& userText);
        void SetHostState(bool online);
        void SetPlcState(bool ok);

    private:
        void OnTick(Windows::Foundation::IInspectable const&, Windows::Foundation::IInspectable const&);

        Microsoft::UI::Xaml::DispatcherTimer m_clockTimer{ nullptr };
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct HeaderPanel : HeaderPanelT<HeaderPanel, implementation::HeaderPanel>
    {
    };
}
