#pragma once

#include "RecipePage.g.h"

namespace winrt::RSolution::implementation
{
    struct RecipePage : RecipePageT<RecipePage>
    {
        RecipePage()
        {
            InitializeComponent();
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::RSolution::factory_implementation
{
    struct RecipePage : RecipePageT<RecipePage, implementation::RecipePage>
    {
    };
}
