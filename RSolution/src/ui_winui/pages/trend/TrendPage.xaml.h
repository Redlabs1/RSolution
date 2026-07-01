#pragma once

#include "TrendPage.g.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <deque>

namespace winrt::RSolution::implementation
{
    struct TrendPage : TrendPageT<TrendPage>
    {
        TrendPage();

        int32_t MyProperty();
        void MyProperty(int32_t value);

    private:
        void OnTick(winrt::Windows::Foundation::IInspectable const& sender,
                    winrt::Windows::Foundation::IInspectable const& args);
        void DrawGridLines();
        void SampleAndRedraw();

        winrt::Microsoft::UI::Xaml::DispatcherTimer m_timer{ nullptr };
        std::deque<double> m_temperature;
        std::deque<double> m_pressure;
        double m_phase{ 0.0 };

        static constexpr size_t kMaxSamples = 60;
        static constexpr double kTempMin = 20.0, kTempMax = 90.0;
        static constexpr double kPressureMin = 0.0, kPressureMax = 10.0;
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct TrendPage : TrendPageT<TrendPage, implementation::TrendPage>
    {
    };
}
