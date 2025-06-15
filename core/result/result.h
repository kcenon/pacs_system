#pragma once

#include <string>
#include <functional>
#include <optional>
#include <utility>

namespace pacs {
namespace core {

/**
 * @brief Generic result class for operation results
 * 
 * This class is used to represent the result of operations that can succeed or fail.
 * It provides methods to check the status and extract values or error messages.
 */
template<typename T = void>
class Result {
public:
    /**
     * @brief Create a successful result with a value
     * @param value The result value
     * @return A successful Result instance containing the value
     */
    static Result<T> ok(T value) {
        return Result<T>(std::move(value));
    }

    /**
     * @brief Create a failed result with an error message
     * @param error The error message
     * @return A failed Result instance containing the error message
     */
    static Result<T> error(std::string error) {
        return Result<T>(std::move(error));
    }

    /**
     * @brief Check if the result is successful
     * @return True if successful, false otherwise
     */
    bool isOk() const {
        return !errorMessage_.has_value();
    }

    /**
     * @brief Check if the result is a failure
     * @return True if failed, false otherwise
     */
    bool isError() const {
        return errorMessage_.has_value();
    }

    /**
     * @brief Get the value (only if successful)
     * @return The result value
     * @throws std::runtime_error if the result is an error
     */
    const T& value() const {
        if (isError()) {
            throw std::runtime_error("Cannot get value from error result");
        }
        return *value_;
    }

    /**
     * @brief Get the error message (only if failed)
     * @return The error message
     * @throws std::runtime_error if the result is successful
     */
    const std::string& error() const {
        if (isOk()) {
            throw std::runtime_error("Cannot get error from successful result");
        }
        return *errorMessage_;
    }

    /**
     * @brief Execute a callback if the result is successful
     * @param callback The callback function to execute with the result value
     * @return *this for chaining
     */
    Result& onSuccess(std::function<void(const T&)> callback) {
        if (isOk()) {
            callback(*value_);
        }
        return *this;
    }

    /**
     * @brief Execute a callback if the result is a failure
     * @param callback The callback function to execute with the error message
     * @return *this for chaining
     */
    Result& onError(std::function<void(const std::string&)> callback) {
        if (isError()) {
            callback(*errorMessage_);
        }
        return *this;
    }

private:
    // Constructor for success case
    explicit Result(T value) : value_(std::move(value)), errorMessage_(std::nullopt) {}

    // Constructor for error case
    explicit Result(std::string error) : value_(std::nullopt), errorMessage_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<std::string> errorMessage_;
};

/**
 * @brief Specialization of Result for std::string return type
 */
template<>
class Result<std::string> {
public:
    /**
     * @brief Creates a successful Result with a string value
     * @param value The string value
     * @return A successful Result containing the string
     */
    static Result<std::string> success(const std::string& value) {
        return Result<std::string>(value, true);
    }

    /**
     * @brief Creates a successful Result with a string value (move version)
     * @param value The string value to move
     * @return A successful Result containing the string
     */
    static Result<std::string> success(std::string&& value) {
        return Result<std::string>(std::move(value), true);
    }

    /**
     * @brief Creates an error Result with an error message
     * @param error The error message
     * @return An error Result containing the error message
     */
    static Result<std::string> error(const std::string& error) {
        return Result<std::string>(error, false);
    }

    /**
     * @brief Creates an error Result with an error message (move version)
     * @param error The error message to move
     * @return An error Result containing the error message
     */
    static Result<std::string> error(std::string&& error) {
        return Result<std::string>(std::move(error), false);
    }

    /**
     * @brief Check if the Result is successful
     * @return true if successful, false otherwise
     */
    bool isSuccess() const { return isSuccess_; }

    /**
     * @brief Check if the Result is an error
     * @return true if error, false otherwise
     */
    bool isError() const { return !isSuccess_; }

    /**
     * @brief Get the string value
     * @return The string value
     * @throws std::runtime_error if the Result is an error
     */
    const std::string& value() const {
        if (!isSuccess_) {
            throw std::runtime_error("Cannot get value from error Result");
        }
        return data_;
    }

    /**
     * @brief Get the error message
     * @return The error message
     * @throws std::runtime_error if the Result is successful
     */
    const std::string& error() const {
        if (isSuccess_) {
            throw std::runtime_error("Cannot get error from successful Result");
        }
        return data_;
    }

    /**
     * @brief Get the value or a default value if error
     * @param defaultValue The default value to return if error
     * @return The value or default value
     */
    std::string valueOr(const std::string& defaultValue) const {
        return isSuccess_ ? data_ : defaultValue;
    }

private:
    // Private constructor to disambiguate
    Result(std::string data, bool isSuccess) 
        : data_(std::move(data)), isSuccess_(isSuccess) {}

    std::string data_;
    bool isSuccess_;
};

/**
 * @brief Specialization of Result for void return type
 */
template<>
class Result<void> {
public:
    /**
     * @brief Create a successful result without a value
     * @return A successful Result<void> instance
     */
    static Result<void> ok() {
        return Result<void>(true);
    }

    /**
     * @brief Create a failed result with an error message
     * @param error The error message
     * @return A failed Result<void> instance containing the error message
     */
    static Result<void> error(std::string error) {
        return Result<void>(std::move(error));
    }

    /**
     * @brief Check if the result is successful
     * @return True if successful, false otherwise
     */
    bool isOk() const {
        return !errorMessage_.has_value();
    }

    /**
     * @brief Check if the result is a failure
     * @return True if failed, false otherwise
     */
    bool isError() const {
        return errorMessage_.has_value();
    }

    /**
     * @brief Get the error message (only if failed)
     * @return The error message
     * @throws std::runtime_error if the result is successful
     */
    const std::string& error() const {
        if (isOk()) {
            throw std::runtime_error("Cannot get error from successful result");
        }
        return *errorMessage_;
    }

    /**
     * @brief Execute a callback if the result is successful
     * @param callback The callback function to execute
     * @return *this for chaining
     */
    Result& onSuccess(std::function<void()> callback) {
        if (isOk()) {
            callback();
        }
        return *this;
    }

    /**
     * @brief Execute a callback if the result is a failure
     * @param callback The callback function to execute with the error message
     * @return *this for chaining
     */
    Result& onError(std::function<void(const std::string&)> callback) {
        if (isError()) {
            callback(*errorMessage_);
        }
        return *this;
    }

private:
    // Constructor for success case
    explicit Result(bool) : errorMessage_(std::nullopt) {}

    // Constructor for error case
    explicit Result(std::string error) : errorMessage_(std::move(error)) {}

    std::optional<std::string> errorMessage_;
};

} // namespace core
} // namespace pacs