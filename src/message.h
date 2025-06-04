#pragma once
#include <charconv>
#include <string_view>
#include <vector>
#include <optional>
#include <system_error>

/**
 * @brief Detailed parse result with specific error codes
 */
enum class ParseResult {
    FINISHED,           // Message successfully parsed
    NEED_MORE_DATA,     // Incomplete message, need more data
    INVALID_TAG,        // Invalid tag format
    INVALID_VALUE,      // Invalid value format
    CHECKSUM_MISMATCH,  // Calculated checksum doesn't match
    MISSING_FIELDS,     // Required fields are missing
    INVALID_FORMAT      // General format error
};

/**
 * @brief Message validation state
 */
enum class ValidationState {
    VALID,
    INVALID_BEGIN_STRING,
    INVALID_BODY_LENGTH,
    INVALID_CHECKSUM,
    MISSING_REQUIRED_FIELDS
};

class Message {
public:
    // Logger interface to replace cout/cerr
    class ILogger {
    public:
        virtual ~ILogger() = default;
        virtual void error(const std::string_view& msg) = 0;
        virtual void debug(const std::string_view& msg) = 0;
    };

    struct ParseStats {
        size_t processed_fields{0};
        size_t total_length{0};
        ValidationState state{ValidationState::VALID};
    };

    // Core FIX message fields with improved descriptions
    std::string_view beginString; // FIX version identifier (tag 8) e.g. "FIX.4.2"
    std::string_view bodyLength;  // Message body length in bytes (tag 9)
    std::string_view checkSum;    // 3-digit message checksum (tag 10)
    std::string_view msgType;     // Message type (tag 35) e.g. "A" for Logon
    std::string_view senderCompID; // Message sender's ID (tag 49)
    std::string_view targetCompID; // Message recipient's ID (tag 56)
    std::string_view clOrdID;      // Unique client order ID (tag 11)
    std::string_view seqNumber;    // Message sequence number (tag 34)
    std::vector<std::pair<std::string_view, std::string_view>>
        otherFields;       // Additional FIX fields
    bool finished = false; // Flag indicating complete message parse
    size_t _checksum;      // Cumulative checksum calculation

    Message() noexcept : _checksum(0) {}
    
    // Delete copy operations, allow move
    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;
    Message(Message&&) noexcept = default;
    Message& operator=(Message&&) noexcept = default;

    // Add getter for checksum
    [[nodiscard]] size_t checksum() const noexcept { return _checksum; }

    // Add validation result
    [[nodiscard]] ValidationState validate() const noexcept;

    // Improve field setting with error handling
    static std::error_code setMessageField(
        Message& message,
        std::string_view tag,
        std::string_view value) noexcept;

    // Improve parse method signature
    static ParseResult parseFixMessage(
        CircularBuffer& buffer,
        Message& message,
        ParseStats& stats,
        ILogger* logger = nullptr) noexcept;

    // Reset with validation state
    void reset() noexcept {
        beginString = {};
        bodyLength = {};
        checkSum = {};
        msgType = {};
        senderCompID = {};
        targetCompID = {};
        clOrdID = {};
        seqNumber = {};
        otherFields.clear();
        finished = false;
        _checksum = 0;
    }

    // Add efficient checksum calculation
    static size_t calculateChecksum(std::string_view data) noexcept {
        size_t sum = 0;
        for (unsigned char c : data) {
            sum += c;
        }
        return sum % 256;
    }

private:
    // Add validation helpers
    [[nodiscard]] bool validateBeginString() const noexcept;
    [[nodiscard]] bool validateBodyLength() const noexcept;
    [[nodiscard]] bool validateChecksum() const noexcept;

    // ... existing fields ...
};

// Add inline implementation for validation
inline ValidationState Message::validate() const noexcept {
    if (!validateBeginString()) return ValidationState::INVALID_BEGIN_STRING;
    if (!validateBodyLength()) return ValidationState::INVALID_BODY_LENGTH;
    if (!validateChecksum()) return ValidationState::INVALID_CHECKSUM;
    if (!hasRequiredFields()) return ValidationState::MISSING_REQUIRED_FIELDS;
    return ValidationState::VALID;
}