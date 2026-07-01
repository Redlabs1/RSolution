#include "pch.h"
#include "TrendPage.xaml.h"
#if __has_include("TrendPage.g.cpp")
#include "TrendPage.g.cpp"
#endif

#include <windows.h>
#include <chrono>
#include <cmath>
#include <random>
#include <algorithm>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Shapes;
using namespace Windows::Foundation;

namespace winrt::RSolution::implementation
{
    TrendPage::TrendPage()
    {
        InitializeComponent();

        // 캔버스가 레이아웃을 마친 뒤(ActualWidth/Height 확정) 첫 그리기를 수행한다.
        ChartCanvas().Loaded([this](IInspectable const&, RoutedEventArgs const&)
        {
            DrawGridLines();
            SampleAndRedraw();
        });

        m_timer = DispatcherTimer();
        m_timer.Interval(std::chrono::milliseconds(500));
        m_timer.Tick({ get_weak(), &TrendPage::OnTick });
        m_timer.Start();
    }

    void TrendPage::OnTick(IInspectable const& /*sender*/, IInspectable const& /*args*/)
    {
        SampleAndRedraw();
    }

    void TrendPage::DrawGridLines()
    {
        const double w = ChartCanvas().ActualWidth();
        const double h = ChartCanvas().ActualHeight();
        if (w <= 0 || h <= 0) return;

        Shapes::Line lines[4] = { GridLine0(), GridLine1(), GridLine2(), GridLine3() };
        for (size_t i = 0; i < 4; ++i)
        {
            const double y = h * (static_cast<double>(i) / 3.0);
            lines[i].X1(0.0f); lines[i].Y1(static_cast<float>(y));
            lines[i].X2(static_cast<float>(w)); lines[i].Y2(static_cast<float>(y));
        }
    }

    void TrendPage::SampleAndRedraw()
    {
        const double w = ChartCanvas().ActualWidth();
        const double h = ChartCanvas().ActualHeight();
        if (w <= 0 || h <= 0) return;

        // TODO: 실제 IIoProvider/센서 연동 전까지 시뮬레이션 값 사용
        // (설계서 5.1.7 Trace Manager — 주기 수집 공정 변수 관리).
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<double> noise(-1.0, 1.0);

        m_phase += 0.15;
        const double temperature = 55.0 + 20.0 * std::sin(m_phase) + noise(rng) * 1.5;
        const double pressure = 5.0 + 3.0 * std::sin(m_phase * 0.6 + 1.0) + noise(rng) * 0.3;

        m_temperature.push_back(temperature);
        m_pressure.push_back(pressure);
        if (m_temperature.size() > kMaxSamples) m_temperature.pop_front();
        if (m_pressure.size() > kMaxSamples) m_pressure.pop_front();

        DrawGridLines();

        auto toPoints = [&](std::deque<double> const& samples, double minV, double maxV)
        {
            Media::PointCollection pts;
            const double range = (maxV - minV) > 0.0 ? (maxV - minV) : 1.0;
            const size_t n = samples.size();
            for (size_t i = 0; i < n; ++i)
            {
                const double x = (kMaxSamples <= 1) ? 0.0 : (w * static_cast<double>(i) / static_cast<double>(kMaxSamples - 1));
                double v = (samples[i] - minV) / range;
                v = std::clamp(v, 0.0, 1.0);
                const double y = h - v * h;
                pts.Append(Point{ static_cast<float>(x), static_cast<float>(y) });
            }
            return pts;
        };

        TemperatureLine().Points(toPoints(m_temperature, kTempMin, kTempMax));
        PressureLine().Points(toPoints(m_pressure, kPressureMin, kPressureMax));

        wchar_t buf[32]{};
        ::swprintf_s(buf, L"%.1f °C", temperature);
        TemperatureValueText().Text(buf);
        ::swprintf_s(buf, L"%.1f kPa", pressure);
        PressureValueText().Text(buf);
    }

    int32_t TrendPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void TrendPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
