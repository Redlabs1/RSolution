#include "pch.h"
#include "OperationPage.xaml.h"
#if __has_include("OperationPage.g.cpp")
#include "OperationPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::RSolution::implementation
{
    int32_t OperationPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void OperationPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
