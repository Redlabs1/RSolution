#pragma once

#include "IoMonitorPage.g.h"

namespace winrt::RSolution::implementation
{
    struct IoMonitorPage : IoMonitorPageT<IoMonitorPage>
    {
        IoMonitorPage()
        {
            InitializeComponent();
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct IoMonitorPage : IoMonitorPageT<IoMonitorPage, implementation::IoMonitorPage>
    {
    };
}
