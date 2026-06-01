#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace vsr {

struct Error {
    std::string code;
    std::string message;
    std::string details;
};

template <typename T>
class Result {
    static_assert(!std::is_same_v<std::remove_cv_t<T>, Error>, "Result<Error> is not supported.");

public:
    static Result Ok(T value) { return Result(ValueTag{}, std::move(value)); }
    static Result Fail(Error error) { return Result(ErrorTag{}, std::move(error)); }

    bool ok() const { return value_.has_value(); }
    const T& value() const { return value_.value(); }
    T& value() { return value_.value(); }
    const Error& error() const { return error_.value(); }

private:
    struct ValueTag {};
    struct ErrorTag {};

    Result(ValueTag, T value) : value_(std::move(value)) {}
    Result(ErrorTag, Error error) : error_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<Error> error_;
};

template <>
class Result<void> {
public:
    static Result Ok() { return Result(); }
    static Result Fail(Error error) { return Result(std::move(error)); }

    bool ok() const { return !error_.has_value(); }
    const Error& error() const { return error_.value(); }

private:
    Result() = default;
    explicit Result(Error error) : error_(std::move(error)) {}

    std::optional<Error> error_;
};

} // namespace vsr
