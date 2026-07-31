#include "pch.h"
#include "ManualPage.xaml.h"
#if __has_include("ManualPage.g.cpp")
#include "ManualPage.g.cpp"
#endif

#include <cstdio>
#include <sstream>
#include <thread>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "core/infrastructure/data/DataManager.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    using rs::drivers::McProtocolClient;

    // ---- PLC 연결 설정 저장/복원 (<exe>\Data\System\PlcConnectionParam.json) ----

    std::wstring PlcConfigPath()
    {
        wchar_t buf[MAX_PATH]{};
        const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring path(buf, n);
        const auto slash = path.find_last_of(L"\\/");
        return (slash == std::wstring::npos ? std::wstring{} : path.substr(0, slash + 1))
               + L"Data\\System\\PlcConnectionParam.json";
    }

    // 저장된 설정을 읽는다. 파일이 없거나 항목이 없으면 기본값 유지.
    void LoadPlcConfig(std::wstring& ip, std::uint16_t& port)
    {
        Windows::Data::Json::JsonObject obj;
        if (!rs::DataManager::ReadObject(PlcConfigPath(), obj))
            return;
        ip = obj.GetNamedString(L"ip", hstring(ip));
        const double p = obj.GetNamedNumber(L"port", static_cast<double>(port));
        if (p >= 1 && p <= 65535)
            port = static_cast<std::uint16_t>(p);
    }

    bool SavePlcConfig(std::wstring const& ip, std::uint16_t port)
    {
        Windows::Data::Json::JsonObject obj;
        obj.SetNamedValue(L"ip", Windows::Data::Json::JsonValue::CreateStringValue(ip));
        obj.SetNamedValue(L"port", Windows::Data::Json::JsonValue::CreateNumberValue(port));
        return rs::DataManager::WriteObject(PlcConfigPath(), obj);
    }

    std::wstring Widen(std::string const& s)
    {
        return std::wstring(s.begin(), s.end());   // 드라이버 메시지는 ASCII
    }

    std::wstring FormatError(rs::core::Error const& e)
    {
        std::wostringstream oss;
        oss << L"실패 [" << e.code << L"] " << Widen(e.message);
        return oss.str();
    }

    // 10진수 또는 0x 접두 16진수 파싱. 실패 시 false.
    bool ParseUint(hstring const& text, std::uint32_t& out)
    {
        std::wstring s(text);
        try
        {
            size_t pos = 0;
            const int base = (s.size() > 2 && s[0] == L'0' && (s[1] == L'x' || s[1] == L'X')) ? 16 : 10;
            const unsigned long v = std::stoul(s, &pos, base);
            if (pos != s.size()) return false;
            out = static_cast<std::uint32_t>(v);
            return true;
        }
        catch (...) { return false; }
    }

    // "1,2,0x10" 형식 → 워드 배열
    bool ParseWordValues(hstring const& text, std::vector<std::uint16_t>& out)
    {
        out.clear();
        std::wstringstream ss{ std::wstring(text) };
        std::wstring item;
        while (std::getline(ss, item, L','))
        {
            // 앞뒤 공백 제거
            const auto b = item.find_first_not_of(L" \t");
            const auto e = item.find_last_not_of(L" \t");
            if (b == std::wstring::npos) return false;
            std::uint32_t v = 0;
            if (!ParseUint(hstring(item.substr(b, e - b + 1)), v) || v > 0xFFFF) return false;
            out.push_back(static_cast<std::uint16_t>(v));
        }
        return !out.empty();
    }

    // "1,0,1" 형식 → 비트 배열
    bool ParseBitValues(hstring const& text, std::vector<bool>& out)
    {
        out.clear();
        std::wstringstream ss{ std::wstring(text) };
        std::wstring item;
        while (std::getline(ss, item, L','))
        {
            const auto b = item.find_first_not_of(L" \t");
            if (b == std::wstring::npos) return false;
            const wchar_t c = item[b];
            if (c != L'0' && c != L'1') return false;
            out.push_back(c == L'1');
        }
        return !out.empty();
    }

    std::wstring DeviceName(rs::drivers::McDevice dev)
    {
        switch (dev)
        {
        case rs::drivers::McDevice::D: return L"D";
        case rs::drivers::McDevice::W: return L"W";
        case rs::drivers::McDevice::R: return L"R";
        case rs::drivers::McDevice::M: return L"M";
        case rs::drivers::McDevice::X: return L"X";
        case rs::drivers::McDevice::Y: return L"Y";
        case rs::drivers::McDevice::B: return L"B";
        default:                       return L"?";
        }
    }
}

namespace winrt::RSolution::implementation
{
    ManualPage::ManualPage()
    {
        InitializeComponent();

        // 저장된 PLC 연결 설정(PlcConnectionParam.json)이 있으면 IP/포트에 반영.
        {
            std::wstring ip = L"192.168.1.10";
            std::uint16_t port = 5002;
            LoadPlcConfig(ip, port);
            IpBox().Text(ip);
            PortBox().Text(std::to_wstring(port));
        }

        // 페이지 재진입 시 현재 연결 상태 반영(연결은 앱 전역 인스턴스가 유지).
        ApplyConnectionUi(McProtocolClient::Instance().IsConnected(), L"");
    }

    // ------------------------------------------------------------ UI 상태

    void ManualPage::ApplyConnectionUi(bool connected, hstring const& detail)
    {
        const Windows::UI::Color color = connected
            ? Windows::UI::Color{ 255, 0x5B, 0xBA, 0x6F }    // 초록
            : Windows::UI::Color{ 255, 0xE2, 0x57, 0x4C };   // 빨강
        ConnDot().Fill(Media::SolidColorBrush(color));
        ConnStateText().Text(connected ? L"Connected" : L"Disconnected");

        ConnectButton().IsEnabled(!connected);
        DisconnectButton().IsEnabled(connected);
        TestButton().IsEnabled(connected);
        WordReadButton().IsEnabled(connected);
        WordWriteButton().IsEnabled(connected);
        BitReadButton().IsEnabled(connected);
        BitWriteButton().IsEnabled(connected);

        if (!connected)
            CpuText().Text(L"CPU: -");
        if (!detail.empty())
            StatusText().Text(detail);
    }

    // work(워커 스레드) 결과 문자열을 target(지정 시)과 StatusText 에 반영하고 연결 UI 를 갱신한다.
    void ManualPage::RunAsync(std::function<std::wstring()> work,
                              Microsoft::UI::Xaml::Controls::TextBlock const& target)
    {
        if (m_busy->exchange(true))
            return;   // 이전 요청 진행 중 — 중복 실행 방지
        StatusText().Text(L"통신 중...");

        auto dispatcher = DispatcherQueue();
        auto weak = get_weak();
        auto busy = m_busy;

        std::thread([dispatcher, weak, busy, target, work = std::move(work)]()
        {
            const std::wstring message = work();
            busy->store(false);
            if (!dispatcher)
                return;
            dispatcher.TryEnqueue([weak, target, message]()
            {
                if (auto self = weak.get())
                {
                    if (target)
                        target.Text(hstring(message));
                    self->ApplyConnectionUi(McProtocolClient::Instance().IsConnected(), L"");
                    self->StatusText().Text(hstring(message));
                }
            });
        }).detach();
    }

    rs::drivers::McDevice ManualPage::SelectedWordDevice()
    {
        switch (WordDeviceBox().SelectedIndex())
        {
        case 1:  return rs::drivers::McDevice::W;
        case 2:  return rs::drivers::McDevice::R;
        default: return rs::drivers::McDevice::D;
        }
    }

    rs::drivers::McDevice ManualPage::SelectedBitDevice()
    {
        switch (BitDeviceBox().SelectedIndex())
        {
        case 1:  return rs::drivers::McDevice::X;
        case 2:  return rs::drivers::McDevice::Y;
        case 3:  return rs::drivers::McDevice::B;
        default: return rs::drivers::McDevice::M;
        }
    }

    // ------------------------------------------------------------ 연결/해제

    void ManualPage::OnSaveIpClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        std::uint32_t port = 0;
        if (!ParseUint(PortBox().Text(), port) || port == 0 || port > 65535)
        {
            StatusText().Text(L"포트 번호가 올바르지 않습니다.");
            return;
        }
        const std::wstring ip{ IpBox().Text() };
        StatusText().Text(SavePlcConfig(ip, static_cast<std::uint16_t>(port))
            ? L"연결 설정 저장 완료 (PlcConnectionParam.json)"
            : L"연결 설정 저장 실패");
    }

    void ManualPage::OnConnectClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        const std::string ip = to_string(IpBox().Text());
        std::uint32_t port = 0;
        if (!ParseUint(PortBox().Text(), port) || port == 0 || port > 65535)
        {
            StatusText().Text(L"포트 번호가 올바르지 않습니다.");
            return;
        }

        // 워커 스레드에서 UI 객체 속성에 접근하지 않도록, dispatcher 는 UI 스레드에서 미리 캡처한다.
        auto cpuText = CpuText();
        auto dispatcher = DispatcherQueue();
        const std::wstring wideIp{ IpBox().Text() };
        RunAsync([ip, wideIp, port, cpuText, dispatcher]() -> std::wstring
        {
            auto& plc = McProtocolClient::Instance();
            auto status = plc.Connect(ip, static_cast<std::uint16_t>(port));
            if (!status.ok())
                return FormatError(status.error());

            // 연결에 성공한 IP/포트는 다음 실행을 위해 자동 저장.
            SavePlcConfig(wideIp, static_cast<std::uint16_t>(port));

            // 연결 직후 CPU 형명 읽기로 실제 교신까지 확인.
            std::wstring cpu = L"?";
            if (auto name = plc.ReadCpuName(); name.ok())
                cpu = Widen(name.value());

            if (dispatcher)
            {
                const hstring text = hstring(L"CPU: ") + hstring(cpu);
                dispatcher.TryEnqueue([cpuText, text]() { cpuText.Text(text); });
            }
            return L"연결 성공 (CPU: " + cpu + L")";
        }, nullptr);
    }

    void ManualPage::OnDisconnectClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        McProtocolClient::Instance().Disconnect();
        ApplyConnectionUi(false, L"연결 종료");
    }

    void ManualPage::OnLoopbackClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        RunAsync([]() -> std::wstring
        {
            auto status = McProtocolClient::Instance().LoopbackTest();
            return status.ok() ? L"루프백 테스트 통과 — 통신 경로 정상" : FormatError(status.error());
        }, nullptr);
    }

    // ------------------------------------------------------------ 워드 영역

    void ManualPage::OnWordReadClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        std::uint32_t addr = 0, count = 0;
        if (!ParseUint(WordAddrBox().Text(), addr) ||
            !ParseUint(WordCountBox().Text(), count) ||
            count == 0 || count > McProtocolClient::MaxWordPoints)
        {
            WordResultText().Text(L"주소/개수 입력이 올바르지 않습니다.");
            return;
        }

        const auto dev = SelectedWordDevice();
        RunAsync([dev, addr, count]() -> std::wstring
        {
            auto result = McProtocolClient::Instance().ReadWords(dev, addr, static_cast<std::uint16_t>(count));
            if (!result.ok())
                return FormatError(result.error());

            std::wostringstream oss;
            oss << DeviceName(dev) << addr << L"~ : ";
            for (size_t i = 0; i < result.value().size(); ++i)
            {
                if (i) oss << L", ";
                wchar_t hex[8]{};
                ::swprintf_s(hex, L"%04X", result.value()[i]);
                oss << result.value()[i] << L"(0x" << hex << L")";
            }
            return oss.str();
        }, WordResultText());
    }

    void ManualPage::OnWordWriteClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        std::uint32_t addr = 0;
        std::vector<std::uint16_t> values;
        if (!ParseUint(WordAddrBox().Text(), addr) || !ParseWordValues(WordValueBox().Text(), values))
        {
            WordResultText().Text(L"주소/값 입력이 올바르지 않습니다. (예: 1,2,0x10)");
            return;
        }

        const auto dev = SelectedWordDevice();
        RunAsync([dev, addr, values = std::move(values)]() -> std::wstring
        {
            auto status = McProtocolClient::Instance().WriteWords(dev, addr, values);
            if (!status.ok())
                return FormatError(status.error());
            std::wostringstream oss;
            oss << DeviceName(dev) << addr << L"~ 에 " << values.size() << L"워드 쓰기 완료";
            return oss.str();
        }, WordResultText());
    }

    // ------------------------------------------------------------ 비트 영역

    void ManualPage::OnBitReadClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        std::uint32_t addr = 0, count = 0;
        if (!ParseUint(BitAddrBox().Text(), addr) ||
            !ParseUint(BitCountBox().Text(), count) ||
            count == 0 || count > McProtocolClient::MaxBitPoints)
        {
            BitResultText().Text(L"주소/개수 입력이 올바르지 않습니다.");
            return;
        }

        const auto dev = SelectedBitDevice();
        RunAsync([dev, addr, count]() -> std::wstring
        {
            auto result = McProtocolClient::Instance().ReadBits(dev, addr, static_cast<std::uint16_t>(count));
            if (!result.ok())
                return FormatError(result.error());

            std::wostringstream oss;
            oss << DeviceName(dev) << addr << L"~ : ";
            for (size_t i = 0; i < result.value().size(); ++i)
            {
                if (i) oss << L",";
                oss << (result.value()[i] ? L"1" : L"0");
            }
            return oss.str();
        }, BitResultText());
    }

    void ManualPage::OnBitWriteClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        std::uint32_t addr = 0;
        std::vector<bool> values;
        if (!ParseUint(BitAddrBox().Text(), addr) || !ParseBitValues(BitValueBox().Text(), values))
        {
            BitResultText().Text(L"주소/값 입력이 올바르지 않습니다. (예: 1,0,1)");
            return;
        }

        const auto dev = SelectedBitDevice();
        RunAsync([dev, addr, values = std::move(values)]() -> std::wstring
        {
            auto status = McProtocolClient::Instance().WriteBits(dev, addr, values);
            if (!status.ok())
                return FormatError(status.error());
            std::wostringstream oss;
            oss << DeviceName(dev) << addr << L"~ 에 " << values.size() << L"비트 쓰기 완료";
            return oss.str();
        }, BitResultText());
    }

    // ------------------------------------------------------------ 기존 속성

    int32_t ManualPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void ManualPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
