#pragma once

#include "MainWindow.g.h"

namespace winrt::RSolution::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void Navigate(hstring const& tag);
        void NavView_SelectionChanged(
            Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
