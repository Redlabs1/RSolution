#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include <XamlBindingInfo.xaml.g.h>
#include <winrt/impl/Windows.UI.Xaml.Controls.0.h>


#include "pages/operation/OperationPage.xaml.h"
//#include "pages/manual/ManualPage.xaml.h"
//#include "pages/recipe/RecipePage.xaml.h"
//#include "pages/alarm/AlarmPage.xaml.h"
//#include "pages/io/IoMonitorPage.xaml.h"
//#include "pages/maintenance/MaintenancePage.xaml.h"
//#include "pages/system/SystemPage.xaml.h"


using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Navigation;
// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::RSolution::implementation
{

    MainWindow::MainWindow()
    {
        InitializeComponent();

        // 첫 항목(Operation) 선택 → 연한 파랑 하이라이트 + 페이지 이동(SelectionChanged 경유)
        NavView().SelectedItem(NavView().MenuItems().GetAt(0));
    }

    void MainWindow::NavView_SelectionChanged(
        Microsoft::UI::Xaml::Controls::NavigationView const& sender,
        Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        if (args.IsSettingsSelected())
        {
            // TODO: Settings 페이지 네비게이션 구현
            return;
        }

        if (auto container = args.SelectedItemContainer())
        {
            auto item = container.as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
            if (auto tagValue = item.Tag())
            {
                Navigate(winrt::unbox_value<hstring>(tagValue));
            }
        }
    }

    void MainWindow::Navigate(hstring const& tag)
    {
        if (tag == L"main")
        {
            ContentFrame().Navigate(xaml_typename<RSolution::OperationPage>());
        }
        else if (tag == L"autorun")
        {
            ContentFrame().Navigate(xaml_typename<RSolution::AutoRunPage>());
        }
        else if (tag == L"manual")
        {
            ContentFrame().Navigate(xaml_typename<RSolution::ManualPage>());
        }
        else if (tag == L"recipe")
        {
            ContentFrame().Navigate(xaml_typename<RSolution::RecipePage>());
        }
        else if (tag == L"alarm")
        {
            ContentFrame().Navigate(xaml_typename<RSolution::AlarmPage>());
        }
        else if (tag == L"trend")
        {
            ContentFrame().Navigate(xaml_typename<RSolution::TrendPage>());
        }
        else if (tag == L"maintenance")
        {
            ContentFrame().Navigate(xaml_typename<RSolution::MaintenancePage>());
        }
        else if (tag == L"log")
        {
            ContentFrame().Navigate(xaml_typename<RSolution::LogPage>());
        }
        else if (tag == L"system")
        {
            ContentFrame().Navigate(xaml_typename<RSolution::SystemPage>());
        }
    }

    //int32_t MainWindow::MyProperty()
    //{
    //    throw hresult_not_implemented();
    //}

    //void MainWindow::MyProperty(int32_t /* value */)
    //{
    //    throw hresult_not_implemented();
    //}
}
