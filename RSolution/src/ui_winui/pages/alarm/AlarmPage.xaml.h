#pragma once

#include "AlarmPage.g.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::RSolution::implementation
{
    struct AlarmPage : AlarmPageT<AlarmPage>
    {
        AlarmPage();

        // ListView 가 x:Bind 로 구독하는 실시간 로그 목록(최신이 위).
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> LogEntries();

        int32_t MyProperty();
        void MyProperty(int32_t value);

    private:
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_logEntries{
            winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>() };
        size_t m_sinkId{ 0 };
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct AlarmPage : AlarmPageT<AlarmPage, implementation::AlarmPage>
    {
    };
}
