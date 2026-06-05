#include "pch.h"
#include "IoMonitorPage.xaml.h"
#if __has_include("IoMonitorPage.g.cpp")
#include "IoMonitorPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::RSolution::implementation
{
    int32_t IoMonitorPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void IoMonitorPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
