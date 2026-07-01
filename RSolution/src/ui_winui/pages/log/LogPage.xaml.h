#pragma once

#include "LogPage.g.h"

namespace winrt::RSolution::implementation
{
    struct LogPage : LogPageT<LogPage>
    {
        LogPage()
        {
            InitializeComponent();
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct LogPage : LogPageT<LogPage, implementation::LogPage>
    {
    };
}
