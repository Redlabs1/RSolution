#include "pch.h"
#include "MC_Protocol.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cstdio>

#include "core/infrastructure/logging/LogManager.h"

#pragma comment(lib, "ws2_32.lib")

// 3E 프레임(바이너리) 구성:
//   요청  : 50 00 | 네트워크 00 | PC FF | 모듈 I/O FF 03 | 국번 00 | 요구길이(2,LE) |
//           감시타이머(2) | 커맨드(2) | 서브커맨드(2) | 본문...
//   응답  : D0 00 | (에코 5바이트) | 응답길이(2,LE) | 종료코드(2,LE) | 데이터...
// 상세 포맷 근거: MELSEC 커뮤니케이션 프로토콜 매뉴얼 (SH-080008)

namespace rs::drivers
{
    namespace
    {
        constexpr std::uintptr_t kInvalidSocket = ~static_cast<std::uintptr_t>(0);

        // Winsock 은 프로세스당 1회 초기화면 충분 — 최초 사용 시 lazy init.
        bool EnsureWinsock()
        {
            static const bool ok = []
            {
                WSADATA wsa{};
                return ::WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
            }();
            return ok;
        }

        core::Error MakeError(int code, std::string message)
        {
            return core::Error{ code, std::move(message) };
        }
    }

    McProtocolClient& McProtocolClient::Instance()
    {
        static McProtocolClient s_instance;
        return s_instance;
    }

    McProtocolClient::~McProtocolClient()
    {
        Disconnect();
    }

    // ------------------------------------------------------------------ 연결

    core::Status McProtocolClient::Connect(std::string const& ip, std::uint16_t port, int timeoutMs)
    {
        if (!EnsureWinsock())
            return MakeError(mc_errc::SocketError, "WSAStartup failed");

        std::lock_guard lock(m_mutex);
        CloseNoLock();  // 기존 연결이 있으면 정리 후 재연결

        SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET)
            return MakeError(mc_errc::SocketError, "socket() failed");

        // 넌블로킹 connect + select 로 타임아웃 제어
        u_long nonBlocking = 1;
        ::ioctlsocket(s, FIONBIO, &nonBlocking);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = ::htons(port);
        if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1)
        {
            ::closesocket(s);
            return MakeError(mc_errc::InvalidArg, "invalid ip address: " + ip);
        }

        ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(s, &writeSet);
        timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
        if (::select(0, nullptr, &writeSet, nullptr, &tv) != 1)
        {
            ::closesocket(s);
            RS_WARN(rs::LogChannel::Tcpip, L"MC connect timeout");
            return MakeError(mc_errc::ConnectTimeout, "connect timeout: " + ip);
        }

        nonBlocking = 0;
        ::ioctlsocket(s, FIONBIO, &nonBlocking);   // 블로킹 모드 복귀

        const DWORD recvTimeout = static_cast<DWORD>(timeoutMs);
        ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char const*>(&recvTimeout), sizeof(recvTimeout));
        const BOOL keepAlive = TRUE;
        ::setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char const*>(&keepAlive), sizeof(keepAlive));

        m_socket = static_cast<std::uintptr_t>(s);
        m_connected = true;
        RS_INFO(rs::LogChannel::Tcpip, L"MC connected " << std::wstring(ip.begin(), ip.end()).c_str() << L":" << port);
        return core::Status::Success();
    }

    void McProtocolClient::Disconnect()
    {
        std::lock_guard lock(m_mutex);
        if (m_connected)
            RS_INFO(rs::LogChannel::Tcpip, L"MC disconnected");
        CloseNoLock();
    }

    bool McProtocolClient::IsConnected() const noexcept
    {
        std::lock_guard lock(m_mutex);
        return m_connected;
    }

    void McProtocolClient::CloseNoLock()
    {
        if (m_socket != kInvalidSocket)
        {
            ::closesocket(static_cast<SOCKET>(m_socket));
            m_socket = kInvalidSocket;
        }
        m_connected = false;
    }

    // ------------------------------------------------------------- 송수신 공통

    core::Status McProtocolClient::SendAll(std::uint8_t const* buf, int len)
    {
        int total = 0;
        while (total < len)
        {
            const int n = ::send(static_cast<SOCKET>(m_socket), reinterpret_cast<char const*>(buf) + total, len - total, 0);
            if (n <= 0)
                return MakeError(mc_errc::SocketError, "send failed");
            total += n;
        }
        return core::Status::Success();
    }

    core::Status McProtocolClient::RecvAll(std::uint8_t* buf, int len)
    {
        int total = 0;
        while (total < len)
        {
            const int n = ::recv(static_cast<SOCKET>(m_socket), reinterpret_cast<char*>(buf) + total, len - total, 0);
            if (n == 0)
                return MakeError(mc_errc::SocketError, "connection closed by peer");
            if (n < 0)
            {
                const int err = ::WSAGetLastError();
                return MakeError(err == WSAETIMEDOUT ? mc_errc::RecvTimeout : mc_errc::SocketError,
                                 "recv failed (wsa=" + std::to_string(err) + ")");
            }
            total += n;
        }
        return core::Status::Success();
    }

    core::Status McProtocolClient::Transact(std::uint16_t command, std::uint16_t subCommand,
                                            std::vector<std::uint8_t> const& body,
                                            std::vector<std::uint8_t>& out)
    {
        std::lock_guard lock(m_mutex);
        if (!m_connected)
            return MakeError(mc_errc::NotConnected, "not connected");

        // ---- 요청 프레임 조립 ----
        const std::uint16_t reqLen = static_cast<std::uint16_t>(6 + body.size());  // 타이머(2)+커맨드(2)+서브(2)+본문
        std::vector<std::uint8_t> req;
        req.reserve(15 + body.size());
        const std::uint8_t header[15] = {
            0x50, 0x00,                       // 서브헤더
            0x00,                             // 네트워크 번호(자국)
            0xFF,                             // PC 번호
            0xFF, 0x03,                       // 요구 상대 모듈 I/O (CPU 직접)
            0x00,                             // 요구 상대 모듈 국번
            static_cast<std::uint8_t>(reqLen), static_cast<std::uint8_t>(reqLen >> 8),
            0x10, 0x00,                       // CPU 감시 타이머 (16 x 250ms = 4초)
            static_cast<std::uint8_t>(command), static_cast<std::uint8_t>(command >> 8),
            static_cast<std::uint8_t>(subCommand), static_cast<std::uint8_t>(subCommand >> 8),
        };
        req.insert(req.end(), header, header + 15);
        req.insert(req.end(), body.begin(), body.end());

        auto sent = SendAll(req.data(), static_cast<int>(req.size()));
        if (!sent.ok()) { CloseNoLock(); return sent; }

        // ---- 응답 수신: 고정 헤더 9바이트 → 길이만큼 추가 수신 ----
        std::uint8_t head[9]{};
        auto received = RecvAll(head, 9);
        if (!received.ok()) { CloseNoLock(); return received; }

        if (head[0] != 0xD0 || head[1] != 0x00)
        {
            CloseNoLock();
            return MakeError(mc_errc::BadResponse, "invalid subheader");
        }

        const int dataLen = head[7] | (head[8] << 8);   // 종료코드(2) 포함 길이
        if (dataLen < 2 || dataLen > 2 + 0x2000)
        {
            CloseNoLock();
            return MakeError(mc_errc::BadResponse, "invalid response length");
        }

        std::vector<std::uint8_t> payload(static_cast<size_t>(dataLen));
        received = RecvAll(payload.data(), dataLen);
        if (!received.ok()) { CloseNoLock(); return received; }

        const std::uint16_t endCode = static_cast<std::uint16_t>(payload[0] | (payload[1] << 8));
        if (endCode != 0)
        {
            RS_WARN(rs::LogChannel::Tcpip, L"MC end code 0x" << std::hex << endCode);
            char buf[16]{};
            ::sprintf_s(buf, "0x%04X", endCode);
            return MakeError(static_cast<int>(endCode), std::string("PLC end code ") + buf);
        }

        out.assign(payload.begin() + 2, payload.end());
        return core::Status::Success();
    }

    // ------------------------------------------------------------- 상태 확인

    core::Status McProtocolClient::LoopbackTest()
    {
        static constexpr char kMsg[] = "0123456789";     // 0-9/A-F 문자만 허용
        constexpr std::uint16_t n = sizeof(kMsg) - 1;

        std::vector<std::uint8_t> body;
        body.push_back(static_cast<std::uint8_t>(n));
        body.push_back(static_cast<std::uint8_t>(n >> 8));
        body.insert(body.end(), kMsg, kMsg + n);

        std::vector<std::uint8_t> resp;
        auto status = Transact(0x0619, 0x0000, body, resp);
        if (!status.ok())
            return status;

        if (resp.size() != 2u + n ||
            (resp[0] | (resp[1] << 8)) != n ||
            !std::equal(kMsg, kMsg + n, resp.begin() + 2))
        {
            return MakeError(mc_errc::BadResponse, "loopback data mismatch");
        }
        return core::Status::Success();
    }

    core::Result<std::string> McProtocolClient::ReadCpuName()
    {
        std::vector<std::uint8_t> resp;
        auto status = Transact(0x0101, 0x0000, {}, resp);
        if (!status.ok())
            return status.error();
        if (resp.size() < 16)
            return MakeError(mc_errc::BadResponse, "cpu name response too short");

        std::string name(resp.begin(), resp.begin() + 16);   // 형명 16자(공백 패딩)
        while (!name.empty() && (name.back() == ' ' || name.back() == '\0'))
            name.pop_back();
        return name;
    }

    // ------------------------------------------------------------- 워드 영역

    namespace
    {
        // 본문 공통부: 선두 디바이스 번호(3,LE) + 디바이스 코드(1) + 점수(2,LE)
        void AppendDeviceSpec(std::vector<std::uint8_t>& body, McDevice dev,
                              std::uint32_t head, std::uint16_t points)
        {
            body.push_back(static_cast<std::uint8_t>(head));
            body.push_back(static_cast<std::uint8_t>(head >> 8));
            body.push_back(static_cast<std::uint8_t>(head >> 16));
            body.push_back(static_cast<std::uint8_t>(dev));
            body.push_back(static_cast<std::uint8_t>(points));
            body.push_back(static_cast<std::uint8_t>(points >> 8));
        }
    }

    core::Result<std::vector<std::uint16_t>> McProtocolClient::ReadWords(McDevice dev, std::uint32_t head, std::uint16_t points)
    {
        if (points == 0 || points > MaxWordPoints)
            return MakeError(mc_errc::InvalidArg, "word points must be 1.." + std::to_string(MaxWordPoints));

        std::vector<std::uint8_t> body;
        AppendDeviceSpec(body, dev, head, points);

        std::vector<std::uint8_t> resp;
        auto status = Transact(0x0401, 0x0000, body, resp);
        if (!status.ok())
            return status.error();
        if (resp.size() != static_cast<size_t>(points) * 2)
            return MakeError(mc_errc::BadResponse, "unexpected word data length");

        std::vector<std::uint16_t> words(points);
        for (std::uint16_t i = 0; i < points; ++i)
            words[i] = static_cast<std::uint16_t>(resp[i * 2] | (resp[i * 2 + 1] << 8));
        return words;
    }

    core::Status McProtocolClient::WriteWords(McDevice dev, std::uint32_t head, std::vector<std::uint16_t> const& data)
    {
        if (data.empty() || data.size() > MaxWordPoints)
            return MakeError(mc_errc::InvalidArg, "word points must be 1.." + std::to_string(MaxWordPoints));

        std::vector<std::uint8_t> body;
        AppendDeviceSpec(body, dev, head, static_cast<std::uint16_t>(data.size()));
        for (std::uint16_t w : data)
        {
            body.push_back(static_cast<std::uint8_t>(w));
            body.push_back(static_cast<std::uint8_t>(w >> 8));
        }

        std::vector<std::uint8_t> resp;
        return Transact(0x1401, 0x0000, body, resp);
    }

    // ------------------------------------------------------------- 비트 영역

    core::Result<std::vector<bool>> McProtocolClient::ReadBits(McDevice dev, std::uint32_t head, std::uint16_t points)
    {
        if (points == 0 || points > MaxBitPoints)
            return MakeError(mc_errc::InvalidArg, "bit points must be 1.." + std::to_string(MaxBitPoints));

        std::vector<std::uint8_t> body;
        AppendDeviceSpec(body, dev, head, points);

        std::vector<std::uint8_t> resp;
        auto status = Transact(0x0401, 0x0001, body, resp);   // 서브커맨드 0x0001 = 비트 단위
        if (!status.ok())
            return status.error();

        // 비트 단위 응답: 1점당 4비트(니블) — 홀수 점수면 마지막 하위 니블은 패딩.
        const size_t expected = (static_cast<size_t>(points) + 1) / 2;
        if (resp.size() != expected)
            return MakeError(mc_errc::BadResponse, "unexpected bit data length");

        std::vector<bool> bits(points);
        for (std::uint16_t i = 0; i < points; ++i)
        {
            const std::uint8_t byte = resp[i / 2];
            bits[i] = ((i % 2 == 0) ? (byte >> 4) : (byte & 0x0F)) != 0;
        }
        return bits;
    }

    core::Status McProtocolClient::WriteBits(McDevice dev, std::uint32_t head, std::vector<bool> const& values)
    {
        if (values.empty() || values.size() > MaxBitPoints)
            return MakeError(mc_errc::InvalidArg, "bit points must be 1.." + std::to_string(MaxBitPoints));

        std::vector<std::uint8_t> body;
        AppendDeviceSpec(body, dev, head, static_cast<std::uint16_t>(values.size()));

        // 1점당 4비트(니블) 패킹 — 첫 점이 상위 니블.
        for (size_t i = 0; i < values.size(); i += 2)
        {
            std::uint8_t byte = values[i] ? 0x10 : 0x00;
            if (i + 1 < values.size() && values[i + 1])
                byte |= 0x01;
            body.push_back(byte);
        }

        std::vector<std::uint8_t> resp;
        return Transact(0x1401, 0x0001, body, resp);
    }
}
