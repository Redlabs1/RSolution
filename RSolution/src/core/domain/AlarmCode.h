#pragma once

#include <cstdint>
#include <string>
#include <cwchar>

// 설계서 5.1.5.1: 알람 ID 체계. 내부 식별자(숫자, 로그/EventBus/AlarmService 에서 그대로 사용)와
// 표시 문구(카테고리-일련번호, 예: "MOT-001")를 분리한다. 기존 IAlarmService/AlarmService 는
// std::uint32_t 코드를 그대로 쓰므로, 이 헤더는 코드값의 범위를 카테고리로 나누고
// 사람이 읽는 형식으로 변환하는 유틸리티만 제공한다(기존 API 변경 없음).
//
// 범위 배정(1000 단위):
//   1000-1999 모션(MOT), 2000-2999 I/O(IO), 3000-3999 레시피(RCP),
//   4000-4999 통신(COM), 5000-5999 안전 인터락(SFT)
namespace rs::domain
{
    enum class AlarmCategory
    {
        Motion,      // MOT — 축 알람, 서보 오류, 원점 복귀 실패, 위치 편차
        Io,          // IO  — 센서 불일치, 출력 실패, 디바운스 오류
        Recipe,      // RCP — 레시피 미선택, 파라미터 범위 오류, 버전 불일치
        Comm,        // COM — 호스트 연결 실패, 메시지 타임아웃, 재접속 실패
        Safety,      // SFT — EMO, Door Open, Limit, Safety PLC 입력
        Unknown
    };

    namespace detail
    {
        constexpr std::uint32_t CategoryBase(AlarmCategory c) noexcept
        {
            switch (c)
            {
            case AlarmCategory::Motion: return 1000;
            case AlarmCategory::Io:     return 2000;
            case AlarmCategory::Recipe: return 3000;
            case AlarmCategory::Comm:   return 4000;
            case AlarmCategory::Safety: return 5000;
            default:                    return 0;
            }
        }

        constexpr const wchar_t* CategoryPrefix(AlarmCategory c) noexcept
        {
            switch (c)
            {
            case AlarmCategory::Motion: return L"MOT";
            case AlarmCategory::Io:     return L"IO";
            case AlarmCategory::Recipe: return L"RCP";
            case AlarmCategory::Comm:   return L"COM";
            case AlarmCategory::Safety: return L"SFT";
            default:                    return L"UNK";
            }
        }
    }

    // 코드값(1000 단위 범위)으로부터 카테고리를 판별한다. constexpr 로 컴파일 시점 검증 가능(6.4.2).
    constexpr AlarmCategory CategoryOf(std::uint32_t code) noexcept
    {
        if (code >= 5000 && code < 6000) return AlarmCategory::Safety;
        if (code >= 4000 && code < 5000) return AlarmCategory::Comm;
        if (code >= 3000 && code < 4000) return AlarmCategory::Recipe;
        if (code >= 2000 && code < 3000) return AlarmCategory::Io;
        if (code >= 1000 && code < 2000) return AlarmCategory::Motion;
        return AlarmCategory::Unknown;
    }

    // 지정 카테고리 내에서 code 를 생성한다(예: MakeAlarmCode(Motion, 1) == 1001).
    constexpr std::uint32_t MakeAlarmCode(AlarmCategory category, std::uint32_t sequenceInCategory) noexcept
    {
        return detail::CategoryBase(category) + sequenceInCategory;
    }

    // 표시용 문구로 변환한다. 예: 1001 -> "MOT-001".
    inline std::wstring FormatAlarmId(std::uint32_t code) noexcept
    {
        const AlarmCategory cat = CategoryOf(code);
        const std::uint32_t seq = code - detail::CategoryBase(cat);
        wchar_t buf[16]{};
        ::swprintf_s(buf, L"%s-%03u", detail::CategoryPrefix(cat), seq);
        return buf;
    }
}
