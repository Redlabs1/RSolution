#pragma once

// 설계서 5.1.1: 시퀀스 단계들이 공유하는 실행 컨텍스트.
// 상태 머신과 HAL/도메인 서비스에 대한 접근을 제공한다(소유하지 않음 — 포인터).
namespace rs::domain { class EquipmentStateMachine; class IAlarmService; }
namespace rs::hal    { class IMotionController; class IIoProvider; }

namespace rs::sequence
{
    struct SequenceContext
    {
        rs::domain::EquipmentStateMachine* stateMachine{ nullptr };
        rs::hal::IMotionController*         motion{ nullptr };
        rs::hal::IIoProvider*              io{ nullptr };
        rs::domain::IAlarmService*         alarm{ nullptr };
    };
}
