#include "pch.h"
#include "AlarmPage.xaml.h"
#if __has_include("AlarmPage.g.cpp")
#include "AlarmPage.g.cpp"
#endif
#include "core/infrastructure/logging/LogManager.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    constexpr uint32_t kMaxEntries = 500;   // UI 에 유지할 최대 줄 수
}

namespace winrt::RSolution::implementation
{
    AlarmPage::AlarmPage()
    {
        InitializeComponent();

        // LogManager 싱크 등록: 로그가 임의 스레드에서 들어오면 UI 스레드로 마샬링해 목록에 추가.
        // 페이지 대신 관찰 벡터를 캡처하므로 페이지 수명에 묶이지 않는다.
        auto entries = m_logEntries;
        auto dispatcher = DispatcherQueue();

        m_sinkId = rs::LogManager::Instance().AddSink(
            [entries, dispatcher](rs::LogRecord const& rec)
            {
                hstring line{ rec.formatted };
                if (dispatcher)
                {
                    dispatcher.TryEnqueue([entries, line]()
                    {
                        entries.InsertAt(0, box_value(line));   // 최신을 맨 위에
                        while (entries.Size() > kMaxEntries)
                            entries.RemoveAt(entries.Size() - 1);
                    });
                }
            });

        // 페이지가 화면에서 내려가면 싱크 해제.
        Unloaded([id = m_sinkId](IInspectable const&, RoutedEventArgs const&)
        {
            rs::LogManager::Instance().RemoveSink(id);
        });
    }

    Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> AlarmPage::LogEntries()
    {
        return m_logEntries;
    }

    int32_t AlarmPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void AlarmPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
