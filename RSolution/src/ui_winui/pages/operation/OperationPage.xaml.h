#pragma once

#include "OperationPage.g.h"

namespace winrt::RSolution::implementation
{
    struct OperationPage : OperationPageT<OperationPage>
    {
        OperationPage()
        {
            InitializeComponent();
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct OperationPage : OperationPageT<OperationPage, implementation::OperationPage>
    {
    };
}
