#include "pch.h"
#include "LogPage.xaml.h"
#if __has_include("LogPage.g.cpp")
#include "LogPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::RSolution::implementation
{
    int32_t LogPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void LogPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
