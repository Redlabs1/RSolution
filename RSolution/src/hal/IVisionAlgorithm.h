#pragma once

#include "core/common/Result.h"
#include "hal/IVisionCamera.h"   // ImageFrame
#include <string>

// 설계서 v2 5.5.2: IVisionAlgorithm — 이미지 입력, 파라미터 적용, 검사 실행, 결과 반환.
// 검사 결과는 OK/NG 뿐 아니라 점수, 측정값, X/Y/Theta 보정값, 실패 원인을 포함한다(설계서 5.5.6).
namespace rs::hal
{
    enum class VisionJudgement
    {
        Ok,
        Ng,
        NotEvaluated    // 장치 오류 등으로 판정 자체가 수행되지 않음 — 검사 NG 와 구분(설계서 5.5.4)
    };

    struct VisionResult
    {
        VisionJudgement judgement{ VisionJudgement::NotEvaluated };
        double       score{ 0.0 };          // 매칭 점수/신뢰도(0.0~1.0)
        double       measuredValue{ 0.0 };  // 측정값(치수 등, 검사 항목에 따라 해석)
        double       offsetX{ 0.0 };        // 얼라인 보정값
        double       offsetY{ 0.0 };
        double       offsetTheta{ 0.0 };
        std::wstring failureReason;         // NG/미판정 원인
        std::wstring imagePath;             // 저장된 이미지 경로(비동기 저장 후 채워질 수 있음)
    };

    class IVisionAlgorithm
    {
    public:
        virtual ~IVisionAlgorithm() = default;

        // 알고리즘 파라미터(ROI, 임계값, 패턴 모델 등)를 비전 레시피 식별자로 로딩.
        virtual rs::core::Status LoadParameters(std::wstring const& visionRecipeId) = 0;
        virtual rs::core::Result<VisionResult> Execute(ImageFrame const& image) = 0;
    };
}
