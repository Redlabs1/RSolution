#pragma once

// 설계서 12 테스트 전략의 "단위 테스트" 항목(레시피 검증, 상태 전이) 을 하드웨어 없이
// 자동 검증하는 자체 테스트. 외부 테스트 프레임워크 없이 assert 방식으로 구현하며,
// 프로세스 전역 싱글톤(EquipmentStateMachine::Instance, AlarmService::Instance)은 실행 중인
// 앱의 실제 상태를 오염시킬 수 있으므로 사용하지 않고, 격리된 로컬 인스턴스만 검사한다.
namespace rs::tests
{
    // 모든 테스트를 실행하고 통과 여부를 LogManager(Debug 채널)에 기록한다.
    // 반환값: 전부 통과하면 true.
    bool RunSelfTests();
}
