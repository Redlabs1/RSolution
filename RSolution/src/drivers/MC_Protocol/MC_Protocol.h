#pragma once

// 미쓰비시 PLC MC 프로토콜(3E 프레임, 바이너리) TCP 클라이언트.
// - 설계서 6.2 drivers/ 계층: 실제 장치 통신 드라이버 (벤더 SDK 불필요 — 순수 Winsock).
// - 헤더는 WinRT/Winsock 비의존(설계서 6.4.4) — 소켓 핸들은 uintptr_t 로 보관.
// - 모듈 경계는 rs::core::Result / Status 반환(설계서 6.3, 예외 없음).
// - 스레드 안전: 요청-응답 트랜잭션을 뮤텍스로 직렬화.

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "core/common/Result.h"

namespace rs::drivers
{
    // 디바이스 코드 (3E 프레임 바이너리 코드)
    enum class McDevice : std::uint8_t
    {
        D  = 0xA8,  // 데이터 레지스터 (워드)
        W  = 0xB4,  // 링크 레지스터 (워드)
        R  = 0xAF,  // 파일 레지스터 (워드)
        M  = 0x90,  // 내부 릴레이 (비트)
        X  = 0x9C,  // 입력 (비트)
        Y  = 0x9D,  // 출력 (비트)
        B  = 0xA0,  // 링크 릴레이 (비트)
        TN = 0xC2,  // 타이머 현재값 (워드)
        CN = 0xC5,  // 카운터 현재값 (워드)
    };

    // 에러 코드 (rs::core::Error::code 로 전달)
    namespace mc_errc
    {
        constexpr int NotConnected   = 1;  // 미연결 상태에서 통신 시도
        constexpr int SocketError    = 2;  // 소켓 생성/전송/수신 실패
        constexpr int ConnectTimeout = 3;  // 연결 타임아웃
        constexpr int RecvTimeout    = 4;  // 응답 타임아웃
        constexpr int BadResponse    = 5;  // 응답 프레임 형식 오류
        constexpr int InvalidArg     = 6;  // 점수 초과 등 인자 오류
        // 양수 0x4000 이상은 PLC 종료 코드(예: 0xC059)를 그대로 담는다.
    }

    class McProtocolClient
    {
    public:
        McProtocolClient() = default;
        ~McProtocolClient();

        McProtocolClient(McProtocolClient const&) = delete;
        McProtocolClient& operator=(McProtocolClient const&) = delete;

        // 앱 전역 공유 인스턴스(페이지 전환과 무관하게 연결 유지).
        static McProtocolClient& Instance();

        // ---- 연결 관리 ----
        core::Status Connect(std::string const& ip, std::uint16_t port, int timeoutMs = 3000);
        void         Disconnect();
        bool         IsConnected() const noexcept;

        // ---- 연결 상태 확인 ----
        core::Status              LoopbackTest();   // 0x0619: 송신 데이터가 그대로 돌아오는지 확인
        core::Result<std::string> ReadCpuName();    // 0x0101: CPU 형명(예: "Q03UDECPU")

        // ---- 워드 영역 읽기/쓰기 (0x0401 / 0x1401, 서브커맨드 0x0000) ----
        core::Result<std::vector<std::uint16_t>> ReadWords(McDevice dev, std::uint32_t head, std::uint16_t points);
        core::Status                             WriteWords(McDevice dev, std::uint32_t head, std::vector<std::uint16_t> const& data);

        // ---- 비트 영역 읽기/쓰기 (0x0401 / 0x1401, 서브커맨드 0x0001) ----
        core::Result<std::vector<bool>> ReadBits(McDevice dev, std::uint32_t head, std::uint16_t points);
        core::Status                    WriteBits(McDevice dev, std::uint32_t head, std::vector<bool> const& values);

        static constexpr std::uint16_t MaxWordPoints = 960;   // 3E 프레임 일괄 읽기/쓰기 제한
        static constexpr std::uint16_t MaxBitPoints  = 3584;

    private:
        // 요청 송신 + 응답 수신. 성공 시 종료코드 이후의 실데이터를 out 에 담는다.
        core::Status Transact(std::uint16_t command, std::uint16_t subCommand,
                              std::vector<std::uint8_t> const& body,
                              std::vector<std::uint8_t>& out);

        core::Status SendAll(std::uint8_t const* buf, int len);
        core::Status RecvAll(std::uint8_t* buf, int len);
        void         CloseNoLock();

        mutable std::mutex m_mutex;
        std::uintptr_t     m_socket{ ~static_cast<std::uintptr_t>(0) };  // INVALID_SOCKET
        bool               m_connected{ false };
    };
}
