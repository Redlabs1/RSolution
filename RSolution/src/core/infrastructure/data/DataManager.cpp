#include "pch.h"
#include "DataManager.h"

#include <windows.h>
#include <fstream>
#include <iterator>

namespace rs
{
    using namespace winrt::Windows::Data::Json;

    namespace
    {
        std::string ToUtf8(std::wstring_view text)
        {
            if (text.empty()) return {};
            const int len = ::WideCharToMultiByte(CP_UTF8, 0, text.data(),
                static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            std::string out(static_cast<size_t>(len), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, text.data(),
                static_cast<int>(text.size()), out.data(), len, nullptr, nullptr);
            return out;
        }

        std::wstring FromUtf8(const char* data, size_t size)
        {
            if (size == 0) return {};
            const int len = ::MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(size), nullptr, 0);
            std::wstring out(static_cast<size_t>(len), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(size), out.data(), len);
            return out;
        }

        void CreateParentDirectories(std::wstring const& path)
        {
            const auto pos = path.find_last_of(L"\\/");
            if (pos == std::wstring::npos) return;
            const std::wstring dir = path.substr(0, pos);
            std::wstring partial;
            partial.reserve(dir.size());
            for (wchar_t ch : dir)
            {
                partial.push_back(ch);
                if (ch == L'\\' || ch == L'/')
                    ::CreateDirectoryW(partial.c_str(), nullptr);
            }
            ::CreateDirectoryW(dir.c_str(), nullptr);
        }

        std::wstring Indent(int depth)
        {
            return std::wstring(static_cast<size_t>(depth) * 2, L' ');
        }
    }

    std::wstring DataManager::Prettify(IJsonValue const& value)
    {
        switch (value.ValueType())
        {
        case JsonValueType::Object:
        {
            JsonObject obj = value.GetObject();
            if (obj.Size() == 0) return L"{}";

            // 재귀를 위해 람다 대신 정적 도우미를 직접 호출.
            std::wstring s = L"{\n";
            uint32_t i = 0;
            const uint32_t n = obj.Size();
            for (auto const& kv : obj)
            {
                s += Indent(1);
                s += std::wstring(JsonValue::CreateStringValue(kv.Key()).Stringify());  // "key"
                s += L": ";
                // 값 재귀 (한 단계 들여쓰기 보정)
                std::wstring child = Prettify(kv.Value());
                // child 내부의 줄들을 한 단계 더 들여쓴다.
                std::wstring shifted;
                shifted.reserve(child.size());
                for (size_t p = 0; p < child.size(); ++p)
                {
                    shifted.push_back(child[p]);
                    if (child[p] == L'\n') shifted += Indent(1);
                }
                s += shifted;
                if (++i < n) s += L",";
                s += L"\n";
            }
            s += L"}";
            return s;
        }
        case JsonValueType::Array:
        {
            JsonArray arr = value.GetArray();
            if (arr.Size() == 0) return L"[]";

            std::wstring s = L"[\n";
            const uint32_t n = arr.Size();
            for (uint32_t i = 0; i < n; ++i)
            {
                s += Indent(1);
                std::wstring child = Prettify(arr.GetAt(i));
                std::wstring shifted;
                shifted.reserve(child.size());
                for (size_t p = 0; p < child.size(); ++p)
                {
                    shifted.push_back(child[p]);
                    if (child[p] == L'\n') shifted += Indent(1);
                }
                s += shifted;
                if (i + 1 < n) s += L",";
                s += L"\n";
            }
            s += L"]";
            return s;
        }
        default:
            // String / Number / Boolean / Null — 이스케이프 포함 리터럴.
            return std::wstring(value.Stringify());
        }
    }

    bool DataManager::ReadText(std::wstring const& path, std::wstring& out)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        size_t off = 0;
        if (bytes.size() >= 3 &&
            static_cast<unsigned char>(bytes[0]) == 0xEF &&
            static_cast<unsigned char>(bytes[1]) == 0xBB &&
            static_cast<unsigned char>(bytes[2]) == 0xBF)
            off = 3;
        out = FromUtf8(bytes.data() + off, bytes.size() - off);
        return true;
    }

    bool DataManager::WriteText(std::wstring const& path, std::wstring_view text)
    {
        CreateParentDirectories(path);
        std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!f) return false;
        const std::string utf8 = ToUtf8(text);
        f.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
        return static_cast<bool>(f);
    }

    bool DataManager::ReadObject(std::wstring const& path, JsonObject& out)
    {
        std::wstring text;
        if (!ReadText(path, text)) return false;
        JsonObject parsed{ nullptr };
        if (!JsonObject::TryParse(winrt::hstring{ text }, parsed)) return false;
        out = parsed;
        return true;
    }

    bool DataManager::ReadArray(std::wstring const& path, JsonArray& out)
    {
        std::wstring text;
        if (!ReadText(path, text)) return false;
        JsonArray parsed{ nullptr };
        if (!JsonArray::TryParse(winrt::hstring{ text }, parsed)) return false;
        out = parsed;
        return true;
    }

    bool DataManager::WriteObject(std::wstring const& path, JsonObject const& obj, bool pretty)
    {
        const std::wstring text = pretty ? Prettify(obj) : std::wstring(obj.Stringify());
        return WriteText(path, text);
    }

    bool DataManager::WriteArray(std::wstring const& path, JsonArray const& arr, bool pretty)
    {
        const std::wstring text = pretty ? Prettify(arr) : std::wstring(arr.Stringify());
        return WriteText(path, text);
    }
}
