#pragma once

#include "OperationPage.g.h"

namespace winrt::RSolution::implementation
{
    struct OperationPage : OperationPageT<OperationPage>
    {
        OperationPage()
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
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
