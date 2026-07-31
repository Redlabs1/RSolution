#pragma once

#include "core/common/Result.h"
#include "hal/IVisionCamera.h"   // ImageFrame
#include <string>

// 설계서 v2 5.5.2 / 5.5.5: IImageStorage — 원본/처리/NG 이미지와 메타데이터 저장.
// 저장은 검사 실행 흐름과 분리해 비동기 큐 + 별도 저장 스레드로 처리한다(설계서 5.5.6).
namespace rs::hal
{
    enum class ImageKind
    {
        Raw,        // 원본
        Processed,  // 처리 이미지
        Ng          // NG 이미지(우선 저장 대상)
    };

    // 추적성 메타데이터(설계서 5.5.5): 레시피/알고리즘 버전, 촬상 조건, Lot/Wafer 식별 정보 등.
    struct ImageMetadata
    {
        std::wstring visionRecipeId;
        std::wstring equipmentRecipeVersion;
        std::wstring algorithmVersion;
        std::wstring lotId;
        std::wstring sequenceStep;
        double       positionX{ 0.0 };
        double       positionY{ 0.0 };
    };

    class IImageStorage
    {
    public:
        virtual ~IImageStorage() = default;

        // 비동기 저장 요청 — 즉시 반환. 실제 저장 실패는 이벤트/알람으로 통지.
        // ImageFrame 은 데이터를 소유하므로 큐 전달이 안전하다(IVisionCamera.h 참조).
        virtual rs::core::Status EnqueueSave(ImageKind kind, ImageFrame frame, ImageMetadata metadata) = 0;
        // 저장 큐 적체 상태 조회(HealthMonitor 연계).
        virtual rs::core::Result<std::size_t> GetPendingCount() = 0;
    };
}
