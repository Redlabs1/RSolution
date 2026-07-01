#include "pch.h"
#include "AutoRunPage.xaml.h"
#if __has_include("AutoRunPage.g.cpp")
#include "AutoRunPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::RSolution::implementation
{
    int32_t AutoRunPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void AutoRunPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
