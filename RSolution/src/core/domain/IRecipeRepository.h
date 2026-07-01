#pragma once

#include "core/common/Result.h"
#include <string>
#include <vector>
#include <map>
#include <cstdint>

// 설계서 6.2: IRecipeRepository — 레시피 저장, 조회, 버전 관리, 유효성 검증.
namespace rs::domain
{
    struct Recipe
    {
        std::string                        id;
        std::string                        name;
        std::uint32_t                      version{ 1 };
        std::map<std::string, std::string> parameters;   // 항목명 → 값(문자열 표현)
    };

    class IRecipeRepository
    {
    public:
        virtual ~IRecipeRepository() = default;

        virtual rs::core::Status              Save(Recipe const& recipe) = 0;
        virtual rs::core::Result<Recipe>      Load(std::string const& id) = 0;
        virtual rs::core::Result<std::vector<std::string>> List() = 0;   // 레시피 id 목록
        virtual rs::core::Result<std::uint32_t> GetVersion(std::string const& id) = 0;

        // 유효성 검증. 실패 시 Error.message 에 사유.
        virtual rs::core::Status Validate(Recipe const& recipe) = 0;
    };
}
