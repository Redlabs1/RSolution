#pragma once

#include "SystemPage.g.h"

namespace winrt::RSolution::implementation
{
    struct SystemPage : SystemPageT<SystemPage>
    {
        SystemPage()
        {
            InitializeComponent();
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct SystemPage : SystemPageT<SystemPage, implementation::SystemPage>
    {
    };
}
