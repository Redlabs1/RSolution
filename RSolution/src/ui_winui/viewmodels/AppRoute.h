#pragma once
#include <winrt/base.h>

enum class AppRoute
{
    Unknown,
    Operation,
    Manual,
    Recipe,
    Alarm,
    IoMonitor,
    Trend,
    Maintenance,
    System
};

inline AppRoute ParseRoute(winrt::hstring const& tag)
{
    if (tag == L"operation")   return AppRoute::Operation;
    if (tag == L"manual")      return AppRoute::Manual;
    if (tag == L"recipe")      return AppRoute::Recipe;
    if (tag == L"alarm")       return AppRoute::Alarm;
    if (tag == L"io_monitor")  return AppRoute::IoMonitor;
    if (tag == L"trend")       return AppRoute::Trend;
    if (tag == L"maintenance") return AppRoute::Maintenance;
    if (tag == L"system")      return AppRoute::System;
    return AppRoute::Unknown;
}
