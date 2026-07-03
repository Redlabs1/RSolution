#pragma once

#include <string_view>

// UI 문서 v2 2.2: 화면 라우팅 식별자. MainWindow 의 NavigationViewItem Tag 와 1:1 대응한다.
// v2 에서 AutoRun / Log 가 추가되어 pages/ 하위 전체 페이지와 정렬되었다.
enum class AppRoute
{
    Unknown,
    Operation,    // Tag "main"
    AutoRun,      // Tag "autorun" (v2 추가)
    Manual,       // Tag "manual"
    Recipe,       // Tag "recipe"
    Alarm,        // Tag "alarm"
    IoMonitor,    // Tag "io"
    Trend,        // Tag "trend"
    Maintenance,  // Tag "maintenance"
    Log,          // Tag "log" (v2 추가)
    System        // Tag "system"
};

// WinRT 비의존(std::wstring_view) — ViewModel/서비스 어디서나 사용 가능.
constexpr AppRoute ParseRoute(std::wstring_view tag) noexcept
{
    if (tag == L"main")        return AppRoute::Operation;
    if (tag == L"autorun")     return AppRoute::AutoRun;
    if (tag == L"manual")      return AppRoute::Manual;
    if (tag == L"recipe")      return AppRoute::Recipe;
    if (tag == L"alarm")       return AppRoute::Alarm;
    if (tag == L"io")          return AppRoute::IoMonitor;
    if (tag == L"trend")       return AppRoute::Trend;
    if (tag == L"maintenance") return AppRoute::Maintenance;
    if (tag == L"log")         return AppRoute::Log;
    if (tag == L"system")      return AppRoute::System;
    return AppRoute::Unknown;
}

constexpr std::wstring_view ToTag(AppRoute route) noexcept
{
    switch (route)
    {
    case AppRoute::Operation:   return L"main";
    case AppRoute::AutoRun:     return L"autorun";
    case AppRoute::Manual:      return L"manual";
    case AppRoute::Recipe:      return L"recipe";
    case AppRoute::Alarm:       return L"alarm";
    case AppRoute::IoMonitor:   return L"io";
    case AppRoute::Trend:       return L"trend";
    case AppRoute::Maintenance: return L"maintenance";
    case AppRoute::Log:         return L"log";
    case AppRoute::System:      return L"system";
    default:                    return L"";
    }
}

static_assert(ParseRoute(L"autorun") == AppRoute::AutoRun);
static_assert(ParseRoute(L"log") == AppRoute::Log);
static_assert(ToTag(AppRoute::IoMonitor) == std::wstring_view{ L"io" });
