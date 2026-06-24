#pragma once

#include <string>
#include <string_view>
#include <sstream>
#include <mutex>
#include <fstream>
#include <functional>
#include <map>

namespace rs
{
    enum class LogLevel
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Off
    };

    // 로그 채널(스트림). 각 채널은 자기 폴더/크기롤링/보관일수를 가진다.
    // LogManagerSystemParam.json 의 *LoggerParam 키에 매핑된다.
    enum class LogChannel
    {
        Debug,      // DebugLoggerParam
        Info,       // InfoLoggerParam
        Event,      // EventLoggerParam
        Process,    // ProLoggerParam
        Exception,  // ExceptionLoggerParam
        Param,      // ParamLoggerParam
        Tcpip,      // TCPIPLoggerParam
        Gpib,       // GpibLoggerParam
        Count
    };

    std::wstring_view ToString(LogLevel level) noexcept;
    std::wstring_view ToString(LogChannel channel) noexcept;   // 표시/파일명용 (예: INFO)
    const wchar_t*    ConfigKey(LogChannel channel) noexcept;   // JSON 키 (예: InfoLoggerParam)

    // 싱크(예: UI)로 전달되는 한 건의 로그 레코드.
    struct LogRecord
    {
        LogChannel   channel;
        LogLevel     level;
        std::wstring time;        // "yyyy-MM-dd HH:mm:ss.fff"
        std::wstring message;
        std::wstring formatted;   // 파일에 기록되는 전체 라인(개행 제외)
    };

    using LogSink = std::function<void(LogRecord const&)>;

    // 다채널 스레드 안전 로거.
    //  - 채널별 폴더: <LogDirPath>\<CHANNEL>_<yyyy-MM-dd>[.N].log
    //  - 크기 롤링(FileSizeLimit), 보관일수 자동 삭제(DeleteLogByDay)
    //  - 디버그 출력 + 등록 싱크(실시간 UI)
    //  - 설정은 LogManagerSystemParam.json 에서 로드(없으면 기본값)
    class LogManager
    {
    public:
        static LogManager& Instance() noexcept;

        // JSON 설정 파일을 읽어 채널을 구성한다. 파일/키가 없으면 기본값.
        void LoadConfig(std::wstring const& jsonPath);

        void SetMinLevel(LogLevel level) noexcept;
        LogLevel MinLevel() const noexcept;
        bool IsEnabled(LogLevel level) const noexcept;

        void Write(LogChannel channel, LogLevel level, std::wstring_view message) noexcept;

        // 실시간 싱크. 반환된 id 로 RemoveSink. 콜백은 임의 스레드에서 호출될 수 있다.
        size_t AddSink(LogSink sink);
        void RemoveSink(size_t id);

        void Flush();

        LogManager(LogManager const&) = delete;
        LogManager& operator=(LogManager const&) = delete;

    private:
        struct Channel
        {
            bool               enabled{ true };
            std::wstring       dir;
            unsigned long long sizeLimit{ 100ull * 1024 * 1024 };  // FileSizeLimit
            int                retentionDays{ 90 };                // DeleteLogByDay

            // 런타임 상태
            std::ofstream      file;
            std::wstring       currentDate;
            unsigned long long currentBytes{ 0 };
            unsigned           index{ 0 };
            bool               opened{ false };
        };

        LogManager();
        ~LogManager();

        Channel& ChannelFor(LogChannel ch) noexcept;
        void     ApplyDefaults();                         // 잠금 보유 상태
        void     RollIfNeeded(Channel& c, LogChannel ch); // 잠금 보유 상태
        void     OpenChannelFile(Channel& c, LogChannel ch); // 잠금 보유 상태
        void     RunRetention(Channel& c, LogChannel ch); // 잠금 보유 상태
        std::wstring FilePathFor(Channel const& c, LogChannel ch) const;

        mutable std::mutex m_mutex;
        Channel            m_channels[static_cast<size_t>(LogChannel::Count)];
        std::wstring       m_baseDir;     // 설정 없을 때 기본 베이스 폴더
        LogLevel           m_minLevel{ LogLevel::Info };
        bool               m_configured{ false };

        std::map<size_t, LogSink> m_sinks;
        size_t                    m_nextSinkId{ 1 };
    };
}

// 채널 + 레벨 지정 로깅. 예: RS_INFO(rs::LogChannel::Process, L"recipe " << name << L" loaded");
#define RS_LOG(channel, level, expr)                                              \
    do {                                                                          \
        if (::rs::LogManager::Instance().IsEnabled(level)) {                      \
            std::wostringstream _rs_oss;                                          \
            _rs_oss << expr;                                                      \
            ::rs::LogManager::Instance().Write((channel), (level), _rs_oss.str());\
        }                                                                         \
    } while (false)

#define RS_TRACE(channel, expr) RS_LOG((channel), ::rs::LogLevel::Trace, expr)
#define RS_DEBUG(channel, expr) RS_LOG((channel), ::rs::LogLevel::Debug, expr)
#define RS_INFO(channel, expr)  RS_LOG((channel), ::rs::LogLevel::Info,  expr)
#define RS_WARN(channel, expr)  RS_LOG((channel), ::rs::LogLevel::Warn,  expr)
#define RS_ERROR(channel, expr) RS_LOG((channel), ::rs::LogLevel::Error, expr)
#define RS_FATAL(channel, expr) RS_LOG((channel), ::rs::LogLevel::Fatal, expr)
