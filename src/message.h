#pragma once
#include <charconv> // Required for std::from_chars
#include <iostream>
#include <string_view>
#include <vector>
#include "circularbuffer.h"

/**
 * @brief Enum representing possible outcomes of message parsing
 */
enum class ParseResult {
    FINISHED,   // Message was successfully parsed
    ERROR,      // Error occurred during parsing
    CONTINUE    // Need more data to complete parsing
};

/**
 * @brief Represents a FIX protocol message with zero-copy parsing
 *
 * This class handles the parsing and storage of FIX protocol messages,
 * using string_view to avoid copying data where possible.
 */
class Message {
public:
    // Core FIX message fields with improved descriptions
    std::string_view beginString;     // FIX version identifier (tag 8) e.g. "FIX.4.2"
    std::string_view bodyLength;      // Message body length in bytes (tag 9)
    std::string_view checkSum;        // 3-digit message checksum (tag 10)
    std::string_view msgType;         // Message type (tag 35) e.g. "A" for Logon
    std::string_view senderCompID;    // Message sender's ID (tag 49)
    std::string_view targetCompID;    // Message recipient's ID (tag 56) 
    std::string_view clOrdID;         // Unique client order ID (tag 11)
    std::string_view seqNumber;       // Message sequence number (tag 34)
    std::vector<std::pair<std::string_view, std::string_view>> otherFields;  // Additional FIX fields
    bool finished = false;            // Flag indicating complete message parse
    size_t _checksum;                 // Cumulative checksum calculation

    Message() : _checksum(0) {};
    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;
    Message(Message&&) = default;
    Message& operator=(Message&&) = default;

    /**
     * @brief Standard FIX protocol constants
     */
    static constexpr char SOH = '\x01';  // Start of header - FIX field delimiter
    static constexpr std::string_view BeginString = "8=";
    static constexpr std::string_view BodyLength = "9=";
    static constexpr std::string_view MsgType = "35=";
    static constexpr std::string_view CheckSum = "10=";
    static constexpr std::string_view SenderCompID = "49=";
    static constexpr std::string_view TargetCompID = "56=";
    static constexpr std::string_view ClOrdID =  "11=";
    static constexpr std::string_view SeqNumber = "34=";

    /**
     * @brief Sets a field in the message based on tag-value pair
     *
     * @param message Message object to modify
     * @param tag Field tag
     * @param taglength Length of the tag
     * @param value Field value
     * @param valuelength Length of the value
     */
    static void setMessageField(Message& message, const char* tag, size_t taglength,
                              const char* value, size_t valuelength) {

        int _tag;
        auto result = std::from_chars(tag, tag + taglength, _tag);
        if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
            std::cerr << "Error: Invalid tag format." << std::endl;
            return; // Or throw an exception, depending on your error handling policy
        }

        switch (_tag)
        {
        case 8:
            message.beginString = std::string_view(value, valuelength);
            break;
        case 9:
            message.bodyLength = std::string_view(value, valuelength);
            break;
        case 10:
            message.checkSum = std::string_view(value, valuelength);
            message._checksum -= '1' + '0' + '=' + SOH;
            for(size_t i = 0; i < valuelength; i++) {
                message._checksum -= value[i];
            }
            std::cout << "Final Checksum: " << message._checksum % 256 << std::endl;
            message.finished = true;
            break;
        case 35:
            message.msgType = std::string_view(value, valuelength);
            break;
        case 49:
            message.senderCompID = std::string_view(value, valuelength);
            break;
        case 56:
            message.targetCompID = std::string_view(value, valuelength);
            break;
        case 11:
            message.clOrdID = std::string_view(value, valuelength);
            break;
        case 34:
            message.seqNumber = std::string_view(value, valuelength);
            break;
        default:
            message.otherFields.emplace_back(std::string_view(tag, taglength), std::string_view(value, valuelength));
            break;
        }
    }

    /**
     * @brief Parses a FIX message from a circular buffer
     *
     * Performs zero-copy parsing of FIX messages, maintaining a running checksum
     * and validating the message format.
     *
     * @param buffer Circular buffer containing message data
     * @param message Message object to populate
     * @return ParseResult indicating parsing outcome
     */
    static ParseResult parseFixMessage(CircularBuffer& buffer, Message& message) {
                size_t start;
        char* data;
        size_t length;
        if (!buffer.getReadView(start, data, length)) {
            return ParseResult::CONTINUE;
        }

        size_t pos = 0;
        while (pos < length) {
            // get field tag
            size_t tagstart = pos;
            while('=' != data[pos] && pos < length) {
                if(data[pos] < '0' || data[pos] > '9') {
                    std::cout << "Invalid tag character: " << data[pos] << std::endl;
                    // buffer.consume(pos); // Consume invalid data
                    return ParseResult::ERROR;
                }
                message._checksum += data[pos++];
            }
            if (pos == length) {
                std::cout << "Incomplete tag." << std::endl;
                return ParseResult::CONTINUE; // Need more data
            }
            size_t taglen = pos - tagstart;

            message._checksum += data[pos++]; // Consume '='

            size_t valuestart = pos;
            while(SOH != data[pos] && pos < length) {
                message._checksum += data[pos++];
            }
            if (pos == length) {
                std::cout << "Incomplete value." << std::endl;
                return ParseResult::CONTINUE; // Need more data
            }

            message._checksum += data[pos++]; // Consume SOH
            setMessageField(message, data + tagstart, taglen, data + valuestart, pos - valuestart - 1);
            buffer.consume(pos - tagstart);
            if(message.finished){
                auto calculated_checksum = static_cast<long int>(message._checksum % 256);
                int received_checksum;
                auto result = std::from_chars(message.checkSum.data(), message.checkSum.data() + message.checkSum.length(), received_checksum);
                if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
                    std::cerr << "Error: Invalid tag format." << std::endl;
                    return ParseResult::ERROR;
                }

                if(calculated_checksum != received_checksum) {
                    std::cout << "Checksum error: Calculated " 
                    << calculated_checksum << ", received " 
                    << received_checksum << std::endl;
                    return ParseResult::ERROR;
                }
                return ParseResult::FINISHED;
            }
        }
        std::cout << "Message not finished." << std::endl;
        return ParseResult::CONTINUE;
    }

    // Add required field validation flags
    bool hasRequiredFields() const {
        return !beginString.empty() && !bodyLength.empty() && 
               !msgType.empty() && !senderCompID.empty() && 
               !targetCompID.empty() && !seqNumber.empty();
    }

    // Add method to reset message state
    void reset() {
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
};