#pragma once

#include <string>
#include <string_view>
#include <winrt/Windows.Data.Json.h>

namespace rs
{
    // JSON 파일을 읽고 쓰는 범용 유틸리티.
    // 외부 의존성 없이 Windows.Data.Json 사용. 모든 메서드는 예외를 던지지 않고 성공 여부를 반환한다.
    // UTF-8(BOM 자동 처리), 상위 폴더 자동 생성.
    class DataManager
    {
    public:
        // JSON 파일 → JsonObject. 실패(파일 없음/파싱 오류) 시 false.
        static bool ReadObject(std::wstring const& path, winrt::Windows::Data::Json::JsonObject& out);

        // JSON 파일 → JsonArray. 실패 시 false.
        static bool ReadArray(std::wstring const& path, winrt::Windows::Data::Json::JsonArray& out);

        // JsonObject → JSON 파일(UTF-8). pretty=true 면 들여쓰기 출력.
        static bool WriteObject(std::wstring const& path,
                                winrt::Windows::Data::Json::JsonObject const& obj,
                                bool pretty = true);

        // JsonArray → JSON 파일(UTF-8).
        static bool WriteArray(std::wstring const& path,
                               winrt::Windows::Data::Json::JsonArray const& arr,
                               bool pretty = true);

        // 저수준 텍스트 I/O.
        static bool ReadText(std::wstring const& path, std::wstring& out);
        static bool WriteText(std::wstring const& path, std::wstring_view text);

        // 들여쓰기 JSON 문자열로 직렬화(파일 저장 없이).
        static std::wstring Prettify(winrt::Windows::Data::Json::IJsonValue const& value);

        DataManager() = delete;
    };
}
