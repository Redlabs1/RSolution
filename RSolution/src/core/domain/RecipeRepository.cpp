#include "pch.h"
#include "core/domain/RecipeRepository.h"
#include "core/infrastructure/data/DataManager.h"
#include <windows.h>
#include <vector>

namespace rs::domain
{
    namespace
    {
        using namespace winrt::Windows::Data::Json;

        std::wstring ToWide(std::string const& s)
        {
            if (s.empty()) return {};
            const int len = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
            std::wstring w(static_cast<size_t>(len), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), len);
            return w;
        }

        std::string ToUtf8(std::wstring const& w)
        {
            if (w.empty()) return {};
            const int len = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
            std::string s(static_cast<size_t>(len), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), len, nullptr, nullptr);
            return s;
        }

        JsonObject ToJson(Recipe const& recipe)
        {
            JsonObject obj;
            obj.SetNamedValue(L"id", JsonValue::CreateStringValue(ToWide(recipe.id)));
            obj.SetNamedValue(L"name", JsonValue::CreateStringValue(ToWide(recipe.name)));
            obj.SetNamedValue(L"version", JsonValue::CreateNumberValue(recipe.version));

            JsonObject params;
            for (auto const& [key, value] : recipe.parameters)
                params.SetNamedValue(ToWide(key), JsonValue::CreateStringValue(ToWide(value)));
            obj.SetNamedValue(L"parameters", params);
            return obj;
        }

        Recipe FromJson(JsonObject const& obj)
        {
            Recipe r;
            r.id = ToUtf8(std::wstring(obj.GetNamedString(L"id", L"")));
            r.name = ToUtf8(std::wstring(obj.GetNamedString(L"name", L"")));
            r.version = static_cast<std::uint32_t>(obj.GetNamedNumber(L"version", 1));

            if (obj.HasKey(L"parameters"))
            {
                JsonObject params = obj.GetNamedObject(L"parameters");
                for (auto const& kv : params)
                    r.parameters[ToUtf8(std::wstring(kv.Key()))] = ToUtf8(std::wstring(kv.Value().GetString()));
            }
            return r;
        }
    }

    RecipeRepository::RecipeRepository(std::wstring directory)
        : m_directory(std::move(directory))
    {
    }

    std::wstring RecipeRepository::PathFor(std::string const& id) const
    {
        return m_directory + L"\\" + ToWide(id) + L".json";
    }

    rs::core::Status RecipeRepository::Validate(Recipe const& recipe)
    {
        if (recipe.id.empty())
            return rs::core::Error{ 1, "recipe id is empty" };
        if (recipe.name.empty())
            return rs::core::Error{ 2, "recipe name is empty" };
        if (recipe.version < 1)
            return rs::core::Error{ 3, "recipe version must be >= 1" };
        return rs::core::Status::Success();
    }

    rs::core::Status RecipeRepository::Save(Recipe const& recipe)
    {
        if (auto v = Validate(recipe); !v.ok())
            return v;

        if (!rs::DataManager::WriteObject(PathFor(recipe.id), ToJson(recipe), true))
            return rs::core::Error{ 10, "failed to write recipe file: " + recipe.id };
        return rs::core::Status::Success();
    }

    rs::core::Result<Recipe> RecipeRepository::Load(std::string const& id)
    {
        JsonObject obj;
        if (!rs::DataManager::ReadObject(PathFor(id), obj))
            return rs::core::Error{ 11, "recipe not found: " + id };
        return FromJson(obj);
    }

    rs::core::Result<std::vector<std::string>> RecipeRepository::List()
    {
        std::vector<std::string> ids;
        const std::wstring pattern = m_directory + L"\\*.json";
        WIN32_FIND_DATAW fd{};
        HANDLE h = ::FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE)
            return ids;   // 폴더 없음/파일 없음 → 빈 목록

        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            std::wstring name(fd.cFileName);
            const auto pos = name.rfind(L".json");
            if (pos != std::wstring::npos)
                ids.push_back(ToUtf8(name.substr(0, pos)));
        } while (::FindNextFileW(h, &fd));
        ::FindClose(h);

        return ids;
    }

    rs::core::Result<std::uint32_t> RecipeRepository::GetVersion(std::string const& id)
    {
        auto r = Load(id);
        if (!r.ok())
            return r.error();
        return r.value().version;
    }
}
