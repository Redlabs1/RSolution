#pragma once

#include "ManualPage.g.h"

namespace winrt::RSolution::implementation
{
    struct ManualPage : ManualPageT<ManualPage>
    {
        ManualPage()
        {
            InitializeComponent();
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct ManualPage : ManualPageT<ManualPage, implementation::ManualPage>
    {
    };
}
