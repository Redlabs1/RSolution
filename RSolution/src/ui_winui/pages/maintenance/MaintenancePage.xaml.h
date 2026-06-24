#pragma once

#include "MaintenancePage.g.h"

namespace winrt::RSolution::implementation
{
    struct MaintenancePage : MaintenancePageT<MaintenancePage>
    {
        MaintenancePage()
        {
            InitializeComponent();
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct MaintenancePage : MaintenancePageT<MaintenancePage, implementation::MaintenancePage>
    {
    };
}
