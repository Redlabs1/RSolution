#include "pch.h"
#include "MaintenancePage.xaml.h"
#if __has_include("MaintenancePage.g.cpp")
#include "MaintenancePage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::RSolution::implementation
{
    int32_t MaintenancePage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MaintenancePage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
