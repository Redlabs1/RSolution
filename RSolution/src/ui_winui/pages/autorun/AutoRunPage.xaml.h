#pragma once

#include "AutoRunPage.g.h"

namespace winrt::RSolution::implementation
{
    struct AutoRunPage : AutoRunPageT<AutoRunPage>
    {
        AutoRunPage()
        {
            InitializeComponent();
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct AutoRunPage : AutoRunPageT<AutoRunPage, implementation::AutoRunPage>
    {
    };
}
