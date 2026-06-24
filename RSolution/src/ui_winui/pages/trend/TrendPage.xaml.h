#pragma once

#include "TrendPage.g.h"

namespace winrt::RSolution::implementation
{
    struct TrendPage : TrendPageT<TrendPage>
    {
        TrendPage()
        {
            InitializeComponent();
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct TrendPage : TrendPageT<TrendPage, implementation::TrendPage>
    {
    };
}
