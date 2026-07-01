#pragma once

#include "core/domain/IRecipeRepository.h"
#include <string>

// 설계서 6.2 / 5.1.4: IRecipeRepository 의 파일 기반 구현. 레시피를 <directory>\<id>.json 에
// DataManager(JSON, 들여쓰기)로 저장한다. 버전은 저장 시 호출자가 지정한 값을 그대로 기록한다
// (증가 규칙은 상위 Recipe Manager 정책 — 여기서는 저장/조회/검증만 책임진다).
namespace rs::domain
{
    class RecipeRepository : public IRecipeRepository
    {
    public:
        explicit RecipeRepository(std::wstring directory);

        rs::core::Status Save(Recipe const& recipe) override;
        rs::core::Result<Recipe> Load(std::string const& id) override;
        rs::core::Result<std::vector<std::string>> List() override;
        rs::core::Result<std::uint32_t> GetVersion(std::string const& id) override;
        rs::core::Status Validate(Recipe const& recipe) override;

    private:
        std::wstring PathFor(std::string const& id) const;

        std::wstring m_directory;
    };
}
