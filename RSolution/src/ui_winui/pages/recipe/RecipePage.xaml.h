#pragma once

#include "RecipePage.g.h"

namespace winrt::RSolution::implementation
{
    struct RecipePage : RecipePageT<RecipePage>
    {
        RecipePage()
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
    struct RecipePage : RecipePageT<RecipePage, implementation::RecipePage>
    {
    };
}
