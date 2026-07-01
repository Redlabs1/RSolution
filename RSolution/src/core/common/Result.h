#pragma once

#include <string>
#include <optional>
#include <utility>

// 설계서 6.3: 모듈 경계에서는 예외 대신 Result/Error 코드로 변환해 호출자에게 전달한다.
namespace rs::core
{
    struct Error
    {
        int         code{ 0 };
        std::string message;
    };

    // 성공 시 값(T)을, 실패 시 Error 를 담는 모듈 경계 반환 타입.
    template <typename T>
    class Result
    {
    public:
        Result(T value) : m_value(std::move(value)) {}
        Result(Error error) : m_error(std::move(error)) {}

        bool ok() const noexcept { return m_value.has_value(); }
        explicit operator bool() const noexcept { return ok(); }

        T const& value() const { return *m_value; }
        T& value() { return *m_value; }
        Error const& error() const { return m_error; }

    private:
        std::optional<T> m_value;
        Error            m_error;
    };

    // 값이 없는(성공/실패만 있는) 연산용 특수화.
    template <>
    class Result<void>
    {
    public:
        Result() : m_ok(true) {}
        Result(Error error) : m_ok(false), m_error(std::move(error)) {}

        bool ok() const noexcept { return m_ok; }
        explicit operator bool() const noexcept { return m_ok; }
        Error const& error() const { return m_error; }

        static Result Success() { return Result{}; }

    private:
        bool  m_ok{ true };
        Error m_error;
    };

    using Status = Result<void>;
}
