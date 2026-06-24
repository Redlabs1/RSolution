#include "pch.h"
#include "LogManager.h"

#include <windows.h>
#include <vector>
#include <iterator>
#include <winrt/Windows.Data.Json.h>

namespace rs
{
    namespace
    {
        const wchar_t* LevelText(LogLevel level) noexcept
        {
            switch (level)
            {
            case LogLevel::Trace: return L"TRACE";
            case LogLevel::Debug: return L"DEBUG";
            case LogLevel::Info:  return L"INFO ";
            case LogLevel::Warn:  return L"WARN ";
            case LogLevel::Error: return L"ERROR";
            case LogLevel::Fatal: return L"FATAL";
            default:              return L"-----";
            }
        }

        std::wstring ExeDirectory()
        {
            wchar_t buf[MAX_PATH]{};
            const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
            std::wstring path(buf, n);
            const auto pos = path.find_last_of(L"\\/");
            if (pos != std::wstring::npos)
                path.erase(pos + 1);
            return path;   // 끝에 \ 포함
        }

        std::string ToUtf8(std::wstring_view text)
        {
            if (text.empty()) return {};
            const int len = ::WideCharToMultiByte(CP_UTF8, 0, text.data(),
                static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            std::string out(static_cast<size_t>(len), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, text.data(),
                static_cast<int>(text.size()), out.data(), len, nullptr, nullptr);
            return out;
        }

        bool ReadFileTextUtf8(std::wstring const& path, std::wstring& out)
        {
            std::ifstream f(path, std::ios::binary);
            if (!f) return false;
            std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            size_t off = 0;
            if (bytes.size() >= 3 &&
                static_cast<unsigned char>(bytes[0]) == 0xEF &&
                static_cast<unsigned char>(bytes[1]) == 0xBB &&
                static_cast<unsigned char>(bytes[2]) == 0xBF)
                off = 3;
            const int len = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data() + off,
                static_cast<int>(bytes.size() - off), nullptr, 0);
            out.assign(static_cast<size_t>(len), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, bytes.data() + off,
                static_cast<int>(bytes.size() - off), out.data(), len);
            return true;
        }

        std::wstring DateString()
        {
            SYSTEMTIME st{};
            ::GetLocalTime(&st);
            wchar_t buf[16]{};
            ::swprintf_s(buf, L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
            return buf;
        }

        void CreateDirectories(std::wstring const& dir)
        {
            if (dir.empty()) return;
            std::wstring partial;
            partial.reserve(dir.size());
            for (size_t i = 0; i < dir.size(); ++i)
            {
                partial.push_back(dir[i]);
                if (dir[i] == L'\\' || dir[i] == L'/')
                    ::CreateDirectoryW(partial.c_str(), nullptr);
            }
            ::CreateDirectoryW(dir.c_str(), nullptr);
        }

        unsigned long long FileSize(std::wstring const& path)
        {
            WIN32_FILE_ATTRIBUTE_DATA fad{};
            if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
                return 0;
            return (static_cast<unsigned long long>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
        }
    }

    std::wstring_view ToString(LogLevel level) noexcept
    {
        return LevelText(level);
    }

    std::wstring_view ToString(LogChannel channel) noexcept
    {
        switch (channel)
        {
        case LogChannel::Debug:     return L"DEBUG";
        case LogChannel::Info:      return L"INFO";
        case LogChannel::Event:     return L"EVENT";
        case LogChannel::Process:   return L"PROCESS";
        case LogChannel::Exception: return L"EXCEPTION";
        case LogChannel::Param:     return L"PARAM";
        case LogChannel::Tcpip:     return L"TCPIP";
        case LogChannel::Gpib:      return L"GPIB";
        default:                    return L"UNKNOWN";
        }
    }

    const wchar_t* ConfigKey(LogChannel channel) noexcept
    {
        switch (channel)
        {
        case LogChannel::Debug:     return L"DebugLoggerParam";
        case LogChannel::Info:      return L"InfoLoggerParam";
        case LogChannel::Event:     return L"EventLoggerParam";
        case LogChannel::Process:   return L"ProLoggerParam";
        case LogChannel::Exception: return L"ExceptionLoggerParam";
        case LogChannel::Param:     return L"ParamLoggerParam";
        case LogChannel::Tcpip:     return L"TCPIPLoggerParam";
        case LogChannel::Gpib:      return L"GpibLoggerParam";
        default:                    return L"";
        }
    }

    LogManager& LogManager::Instance() noexcept
    {
        static LogManager instance;
        return instance;
    }

    LogManager::LogManager()
    {
        ApplyDefaults();
    }

    LogManager::~LogManager()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& c : m_channels)
            if (c.file.is_open()) c.file.flush();
    }

    LogManager::Channel& LogManager::ChannelFor(LogChannel ch) noexcept
    {
        return m_channels[static_cast<size_t>(ch)];
    }

    void LogManager::ApplyDefaults()
    {
        m_baseDir = ExeDirectory() + L"logs";
        for (size_t i = 0; i < static_cast<size_t>(LogChannel::Count); ++i)
        {
            Channel& c = m_channels[i];
            if (c.file.is_open()) c.file.close();
            c.enabled = true;
            c.dir = m_baseDir + L"\\" + std::wstring(ToString(static_cast<LogChannel>(i)));
            c.sizeLimit = 100ull * 1024 * 1024;
            c.retentionDays = 90;
            c.opened = false;
            c.index = 0;
            c.currentBytes = 0;
            c.currentDate.clear();
        }
    }

    void LogManager::LoadConfig(std::wstring const& jsonPath)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ApplyDefaults();
        m_configured = true;

        // 경로가 비어 있으면 실행 파일 폴더의 LogManagerSystemParam.json 을 찾는다.
        const std::wstring path = jsonPath.empty()
            ? (ExeDirectory() + L"LogManagerSystemParam.json")
            : jsonPath;

        std::wstring text;
        if (!ReadFileTextUtf8(path, text))
            return;   // 파일 없음 → 기본값 유지

        try
        {
            using namespace winrt::Windows::Data::Json;
            JsonObject root = JsonObject::Parse(winrt::hstring{ text });

            for (size_t i = 0; i < static_cast<size_t>(LogChannel::Count); ++i)
            {
                const LogChannel ch = static_cast<LogChannel>(i);
                const wchar_t* key = ConfigKey(ch);
                if (!root.HasKey(key))
                    continue;   // 키 없음 → 기본값 유지

                IJsonValue val = root.GetNamedValue(key);
                if (val.ValueType() != JsonValueType::Object)
                {
                    m_channels[i].enabled = false;   // null 등 → 비활성
                    continue;
                }

                JsonObject o = val.GetObject();
                Channel& c = m_channels[i];
                c.enabled = true;

                const winrt::hstring dir = o.GetNamedString(L"LogDirPath", L"");
                if (!dir.empty())
                    c.dir = std::wstring(dir);

                const double size = o.GetNamedNumber(L"FileSizeLimit", 0);
                if (size > 0)
                    c.sizeLimit = static_cast<unsigned long long>(size);

                c.retentionDays = static_cast<int>(o.GetNamedNumber(L"DeleteLogByDay",
                    static_cast<double>(c.retentionDays)));
            }
        }
        catch (...)
        {
            // 파싱 실패 → 기본값 유지
        }
    }

    void LogManager::SetMinLevel(LogLevel level) noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_minLevel = level;
    }

    LogLevel LogManager::MinLevel() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_minLevel;
    }

    bool LogManager::IsEnabled(LogLevel level) const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_minLevel != LogLevel::Off && level >= m_minLevel;
    }

    std::wstring LogManager::FilePathFor(Channel const& c, LogChannel ch) const
    {
        std::wstring path = c.dir + L"\\" + std::wstring(ToString(ch)) + L"_" + c.currentDate;
        if (c.index > 0)
            path += L"." + std::to_wstring(c.index);
        path += L".log";
        return path;
    }

    void LogManager::OpenChannelFile(Channel& c, LogChannel ch)
    {
        if (c.file.is_open())
            c.file.close();

        CreateDirectories(c.dir);

        // 크기 한도가 있으면, 한도를 넘지 않은 첫 인덱스 파일을 찾아 이어쓴다.
        if (c.sizeLimit > 0)
        {
            while (FileSize(FilePathFor(c, ch)) >= c.sizeLimit)
                ++c.index;
        }

        const std::wstring path = FilePathFor(c, ch);
        c.file.open(path, std::ios::out | std::ios::app | std::ios::binary);
        c.currentBytes = FileSize(path);
        c.opened = true;
    }

    void LogManager::RollIfNeeded(Channel& c, LogChannel ch)
    {
        const std::wstring today = DateString();
        if (!c.opened || today != c.currentDate)
        {
            c.currentDate = today;
            c.index = 0;
            OpenChannelFile(c, ch);
            RunRetention(c, ch);   // 하루 한 번(파일 새로 열 때) 정리
        }
        else if (c.sizeLimit > 0 && c.currentBytes >= c.sizeLimit)
        {
            ++c.index;
            OpenChannelFile(c, ch);
        }
    }

    void LogManager::RunRetention(Channel& c, LogChannel ch)
    {
        if (c.retentionDays <= 0)
            return;

        // 보관 기준 시각(UTC FILETIME).
        FILETIME nowFt{};
        ::GetSystemTimeAsFileTime(&nowFt);
        ULARGE_INTEGER now{};
        now.LowPart = nowFt.dwLowDateTime;
        now.HighPart = nowFt.dwHighDateTime;
        const unsigned long long span =
            static_cast<unsigned long long>(c.retentionDays) * 24ull * 3600ull * 10000000ull;
        const unsigned long long cutoff = (now.QuadPart > span) ? (now.QuadPart - span) : 0;

        const std::wstring pattern = c.dir + L"\\" + std::wstring(ToString(ch)) + L"_*.log";
        WIN32_FIND_DATAW fd{};
        HANDLE h = ::FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE)
            return;
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            ULARGE_INTEGER wt{};
            wt.LowPart = fd.ftLastWriteTime.dwLowDateTime;
            wt.HighPart = fd.ftLastWriteTime.dwHighDateTime;
            if (wt.QuadPart < cutoff)
                ::DeleteFileW((c.dir + L"\\" + fd.cFileName).c_str());
        } while (::FindNextFileW(h, &fd));
        ::FindClose(h);
    }

    void LogManager::Write(LogChannel ch, LogLevel level, std::wstring_view message) noexcept
    {
        try
        {
            LogRecord rec{};
            std::vector<LogSink> sinks;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_minLevel == LogLevel::Off || level < m_minLevel)
                    return;

                Channel& c = ChannelFor(ch);

                SYSTEMTIME st{};
                ::GetLocalTime(&st);
                wchar_t timeBuf[32]{};
                ::swprintf_s(timeBuf, L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

                std::wstring formatted;
                formatted.reserve(message.size() + 64);
                formatted.append(L"[").append(timeBuf).append(L"] [")
                         .append(ToString(ch)).append(L"] [")
                         .append(LevelText(level)).append(L"] ")
                         .append(message);

                const std::wstring lineWithEol = formatted + L"\r\n";
                ::OutputDebugStringW(lineWithEol.c_str());

                if (c.enabled)
                {
                    RollIfNeeded(c, ch);
                    if (c.file.is_open())
                    {
                        const std::string utf8 = ToUtf8(lineWithEol);
                        c.file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
                        c.file.flush();
                        c.currentBytes += utf8.size();
                    }
                }

                rec.channel = ch;
                rec.level = level;
                rec.time = timeBuf;
                rec.message = std::wstring(message);
                rec.formatted = std::move(formatted);

                for (auto const& kv : m_sinks)
                    sinks.push_back(kv.second);
            }

            for (auto const& sink : sinks)
            {
                try { sink(rec); } catch (...) {}
            }
        }
        catch (...)
        {
            // 로깅이 앱을 중단시키지 않도록 모든 예외 흡수.
        }
    }

    size_t LogManager::AddSink(LogSink sink)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const size_t id = m_nextSinkId++;
        m_sinks.emplace(id, std::move(sink));
        return id;
    }

    void LogManager::RemoveSink(size_t id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sinks.erase(id);
    }

    void LogManager::Flush()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& c : m_channels)
            if (c.file.is_open()) c.file.flush();
    }
}
