#include "ordinary_json.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>

namespace ordinaryjson {
    constexpr std::array<char, 4> kTrueCharList = {'t', 'r', 'u', 'e'};
    constexpr std::array<char, 5> kFalseCharList = {'f', 'a', 'l', 's', 'e'};
    constexpr std::array<char, 4> kNullCharList = {'n', 'u', 'l', 'l'};

    constexpr size_t kMaxParseDepth = 100;

    constexpr char kObjectStart = '{';
    constexpr char kObjectEnd = '}';
    constexpr char kArrayStart = '[';
    constexpr char kArrayEnd = ']';
    constexpr char kStringStart = '"';
    constexpr char kStringEnd = '"';
    constexpr char kSeparator = ',';
    constexpr char kKeyValueSep = ':';

    namespace internal {
        class CursorSlice {
        public:
            explicit CursorSlice(const std::string &value)
                : str_(value.data()), size_(value.size()), index_(0) {
            }

            void MoveNext() { ++index_; }
            size_t Index() const { return index_; }
            size_t RemainCount() const { return size_ - index_; }
            bool End() const { return index_ >= size_; }
            bool NotEnd() const { return index_ < size_; }

            char CurrentChar() const {
                assert(NotEnd());
                return str_[index_];
            }

            char Next() {
                assert(NotEnd());
                return str_[index_++];
            }

        private:
            const char *str_;
            size_t size_;
            size_t index_;
        };

        struct NumberResultPack {
            union {
                int64_t integer_data_;
                double double_data_;
            };

            bool is_integer_;
        };

        class OrdinaryParser {
        public:
            OrdinaryParser() = delete;

            static OrdinaryJsonNode ParseValue(CursorSlice &cs, size_t depth = 0);

            static void ParseObject(CursorSlice &cs, JsonObjectType &result,
                                    size_t depth);

            static void ParseArray(CursorSlice &cs, JsonArrayType &result, size_t depth);

            static void ParseString(CursorSlice &cs, JsonStringType &result);

            static void ParseNumberValue(CursorSlice &cs, NumberResultPack &result);

            static bool ParseBoolValue(CursorSlice &cs);

            static void *ParseNullValue(CursorSlice &cs);

            static void SkipWhiteSpace(CursorSlice &cs);

            static void ParseReject(char expect, char actual, size_t pos);

            static void ParseReject(char actual, size_t pos);

            static void ParseReject(const std::string &error_msg, size_t pos);

            static void ParseReject(const std::string &error_msg, char actual,
                                    size_t pos);

        private:
            enum class ParseJsonNumberStatus : uint8_t {
                // expect: '-' || '0' || digit 1-9
                kInitialStatus,
                // expect: '0' || digit 1-9
                kMinusSignStatus,
                // expect: '.' || 'e' || 'E' || end
                kLeadingZeroStatus,
                // expect: digit || '.' || || 'e' || 'E' || end
                kDigitExceptZeroStatus,
                // expect: digit || '.' || 'e' || 'E' || end
                kDigitIntegerStatus,
                // expect: digit
                kDecimalPointStatus,
                // expect: digit || 'e' || 'E' || end
                kDigitAfterDecimalPointStatus,
                // expect: '-' || '+' || digit
                kScientificSignStatus,
                // expect: digit
                kMinusOrPosSignStatus,
                // expect: digit || end
                kDigitAfterScientificStatus,
                kEndStatus,
                kRejectStatus
            };
        };
    } // namespace internal

    /**
     * @brief Convert a hexadecimal character to its integer value.
     *
     * @param c: The character to convert.
     * @return The integer value in [0, 15], or -1 if the character is not a
     *         hexadecimal digit (0-9, a-f, A-F).
     */
    static inline int HexCharToValue(const char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    /**
     * @brief Parse four hexadecimal digits as a Unicode code point.
     *
     * @param cs: The input string cursor positioned at the first hex digit.
     * @param out: On success, receives the parsed code point.
     * @return true if four valid hex digits were consumed, false otherwise.
     */
    static bool ParseHex4(internal::CursorSlice &cs, uint32_t &out) {
        out = 0;
        for (int i = 0; i < 4; ++i) {
            if (cs.End()) return false;
            const int v = HexCharToValue(cs.CurrentChar());
            if (v < 0) return false;
            out = (out << 4) | static_cast<uint32_t>(v);
            cs.MoveNext();
        }
        return true;
    }

    /**
     * @brief Append a Unicode code point to a string as UTF-8.
     *
     * @param out: The output string.
     * @param cp: The Unicode code point.
     */
    static void AppendUtf8(internal::JsonStringType &out, const uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    /**
     * @brief Check if a given character is the zero character.
     *
     * @param c: The character to check.
     * @return true if the character is '0', false otherwise.
     */

    static inline bool IsZeroChar(const char c) { return c == '0'; }

    /**
     * @brief Check if a given character is a digit character.
     *
     * @param c: The character to check.
     * @return true if the character is a digit character, false otherwise.
     *
     * The digit characters are 0-9.
     */
    static inline bool IsDigitChar(const char c) { return c >= '0' && c <= '9'; }

    /**
     * @brief Check if a given character is a digit character except the zero
     *        character.
     *
     * @param c: The character to check.
     * @return true if the character is a digit character except the zero
     *         character, false otherwise.
     *
     * The digit characters except the zero character are 1-9.
     */
    static inline bool IsDigitExceptZeroChar(const char c) {
        return c >= '1' && c <= '9';
    }

    /**
     * @brief Check if a given character is a whitespace character.
     *
     * @param c: The character to check.
     * @return true if the character is a whitespace character, false otherwise.
     *
     * The whitespace characters are ' ', '\t', '\n', and '\r'.
     */
    static inline bool IsWhiteSpaceChar(const char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    /**
     * @brief Check if a given character is not a control character.
     *
     * @param c: The character to check.
     * @return true if the character is not a control character, false otherwise.
     *
     * A control character is a character with a value of 0x00 to 0x1F (inclusive).
     */
    static inline bool IsNotControlChar(const char c) {
        return static_cast<unsigned char>(c) > 0x1F;
    }

    /**
     * @brief Check if a given character is a scientific notation character.
     *
     * @param c: The character to check.
     * @return true if the character is a scientific notation character, false
     *         otherwise.
     *
     * The scientific notation characters are 'e' and 'E'.
     */
    static inline bool IsScientificChar(const char c) {
        return c == 'e' || c == 'E';
    }

    /**
     * @brief Parse a JSON value from a given input string.
     *
     * @param cs: The input string cursor.
     * @param depth
     * @return The parsed JSON value.
     *
     * The following JSON values are supported.
     *  - Object
     *  - Array
     *  - String (with escape sequences and Unicode code points)
     *  - Boolean
     *  - Null
     *  - Integer
     *  - Double (including scientific notation)
     */
    OrdinaryJsonNode internal::OrdinaryParser::ParseValue(CursorSlice &cs,
                                                          const size_t depth) {
        SkipWhiteSpace(cs);
        if (cs.End()) ParseReject("Unexpected end of input", cs.Index());
        const char c = cs.CurrentChar();
        switch (c) {
            case kObjectStart: {
                JsonObjectType result;
                ParseObject(cs, result, depth + 1);
                SkipWhiteSpace(cs);
                return OrdinaryJsonNode(std::move(result));
            }
            case kArrayStart: {
                JsonArrayType result;
                ParseArray(cs, result, depth + 1);
                SkipWhiteSpace(cs);
                return OrdinaryJsonNode(std::move(result));
            }
            case kStringStart: {
                JsonStringType result;
                ParseString(cs, result);
                SkipWhiteSpace(cs);
                return OrdinaryJsonNode(std::move(result));
            }
            case 't':
            case 'f': {
                const bool result = ParseBoolValue(cs);
                SkipWhiteSpace(cs);
                return OrdinaryJsonNode(result);
            }
            case 'n': {
                void *result = ParseNullValue(cs);
                SkipWhiteSpace(cs);
                return OrdinaryJsonNode(result);
            }
            default:
                if ((c >= '0' && c <= '9') || c == '-') {
                    NumberResultPack result{};
                    ParseNumberValue(cs, result);
                    SkipWhiteSpace(cs);
                    return result.is_integer_
                               ? OrdinaryJsonNode(result.integer_data_)
                               : OrdinaryJsonNode(result.double_data_);
                }
                break;
        }
        ParseReject("The unexpected beginning of the value: ", c, cs.Index());
        return {};
    }

    /**
     * @brief Parse a JSON object.
     *
     * @param cs A CursorSlice object which represents input JSON string.
     * @param result A JsonObjectType object which stores the parsed object.
     * @param depth
     *
     * @throw ParseError if input string is invalid.
     *
     * This function parses a JSON object using a context-free grammar.
     * The grammar is as follows:
     *
     * object: '{' (string ':' value (',' string ':' value)*)? '}'
     * string: a sequence of characters enclosed in double quotes
     * value: object | array | string | number | boolean | null
     *
     * The function assumes that the input string is valid and well-formed.
     * The function does not check for invalid input.
     */

    void internal::OrdinaryParser::ParseObject(CursorSlice &cs,
                                               JsonObjectType &result,
                                               const size_t depth) {
        if (depth > kMaxParseDepth)
            ParseReject("Maximum nesting depth exceeded", cs.Index());
        cs.MoveNext(); // consume '{'
        SkipWhiteSpace(cs);
        if (cs.End()) ParseReject("Unexpected end of input", cs.Index());
        if (cs.CurrentChar() == kObjectEnd) {
            cs.MoveNext();
            return;
        }
        while (true) {
            JsonStringType key;
            ParseString(cs, key);
            SkipWhiteSpace(cs);
            if (cs.End() || cs.CurrentChar() != kKeyValueSep)
                ParseReject(kKeyValueSep, cs.End() ? '\0' : cs.CurrentChar(),
                            cs.Index());
            cs.MoveNext();
            result.emplace(std::move(key), ParseValue(cs, depth));
            if (cs.End()) ParseReject("Unexpected end of input", cs.Index());
            const char c = cs.CurrentChar();
            if (c == kSeparator) {
                cs.MoveNext();
                SkipWhiteSpace(cs);
                if (cs.End()) ParseReject("Unexpected end of input", cs.Index());
                continue;
            }
            if (c == kObjectEnd) {
                cs.MoveNext();
                return;
            }
            ParseReject(c, cs.Index());
        }
    }

    /**
     * @brief Parse a JSON array.
     *
     * @param cs A CursorSlice object which represents input JSON string.
     * @param result A JsonArrayType object which stores the parsed array.
     * @param depth
     *
     * This function parses a JSON array using a context-free grammar.
     * The grammar is as follows:
     *
     * array: '[' (value (',' value)*)? ']'
     * value: object | array | string | number | boolean | null
     *
     * The function assumes that the input string is valid and well-formed.
     * The function does not check for invalid input.
     */
    void internal::OrdinaryParser::ParseArray(CursorSlice &cs,
                                              JsonArrayType &result,
                                              const size_t depth) {
        if (depth > kMaxParseDepth)
            ParseReject("Maximum nesting depth exceeded", cs.Index());
        cs.MoveNext(); // consume '['
        SkipWhiteSpace(cs);
        if (cs.End()) ParseReject("Unexpected end of input", cs.Index());
        if (cs.CurrentChar() == kArrayEnd) {
            cs.MoveNext();
            return;
        }
        while (true) {
            result.emplace_back(ParseValue(cs, depth));
            if (cs.End()) ParseReject("Unexpected end of input", cs.Index());
            const char c = cs.CurrentChar();
            if (c == kSeparator) {
                cs.MoveNext();
                SkipWhiteSpace(cs);
                if (cs.End()) ParseReject("Unexpected end of input", cs.Index());
                continue;
            }
            if (c == kArrayEnd) {
                cs.MoveNext();
                return;
            }
            ParseReject(c, cs.Index());
        }
    }

    /**
     * @brief Parse a JSON string.
     *
     * @param cs A CursorSlice object which represents the input JSON string.
     * @param result A JsonStringType object which stores the parsed string.
     *
     * @throw ParseError if input string is invalid.
     *
     * This function parses a JSON string using a context-free grammar. The string
     * is expected to start and end with double quotes. It handles escape sequences
     * and ensures no control characters are present outside escape sequences.
     * The grammar is as follows:
     *
     * string: '"' characters '"'
     * characters: char | escape_sequence | escape_hex_sequence
     * char: any printable character except '"' and '\\'
     * escape_sequence: '\\' escape_char
     * escape_char: one of '"', '\\', '/', 'b', 'f', 'n', 'r', 't'
     * escape_hex_sequence: '\\u' hex hex hex hex
     * hex: a hexadecimal digit (0-9, a-f, A-F)
     */

    void internal::OrdinaryParser::ParseString(CursorSlice &cs,
                                               JsonStringType &result) {
        if (cs.End() || cs.CurrentChar() != kStringStart)
            ParseReject(kStringStart, cs.End() ? '\0' : cs.CurrentChar(),
                        cs.Index());
        cs.MoveNext(); // consume opening quote
        while (cs.NotEnd()) {
            const char c = cs.CurrentChar();
            if (c == kStringEnd) {
                cs.MoveNext(); // consume closing quote
                return;
            }
            if (c == '\\') {
                cs.MoveNext();
                if (cs.End())
                    ParseReject("Unexpected end of input in string escape", cs.Index());
                const char esc = cs.CurrentChar();
                if (esc == 'u') {
                    cs.MoveNext();
                    uint32_t cp = 0;
                    if (!ParseHex4(cs, cp))
                        ParseReject("Invalid unicode escape sequence", cs.Index());
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (cs.NotEnd() && cs.CurrentChar() == '\\') {
                            cs.MoveNext();
                            if (cs.NotEnd() && cs.CurrentChar() == 'u') {
                                cs.MoveNext();
                                uint32_t low = 0;
                                if (ParseHex4(cs, low) && low >= 0xDC00 && low <= 0xDFFF) {
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                    AppendUtf8(result, cp);
                                    continue;
                                }
                            }
                        }
                        ParseReject("Invalid surrogate pair in unicode escape", cs.Index());
                    }
                    if (cp >= 0xDC00 && cp <= 0xDFFF)
                        ParseReject("Unexpected low surrogate in unicode escape", cs.Index());
                    AppendUtf8(result, cp);
                    continue;
                }
                switch (esc) {
                    case '"': result.push_back('"');
                        break;
                    case '\\': result.push_back('\\');
                        break;
                    case '/': result.push_back('/');
                        break;
                    case 'b': result.push_back('\b');
                        break;
                    case 'f': result.push_back('\f');
                        break;
                    case 'n': result.push_back('\n');
                        break;
                    case 'r': result.push_back('\r');
                        break;
                    case 't': result.push_back('\t');
                        break;
                    default: ParseReject("Invalid escape character", esc, cs.Index());
                }
                cs.MoveNext();
                continue;
            }
            if (IsNotControlChar(c)) {
                result.push_back(c);
                cs.MoveNext();
                continue;
            }
            ParseReject(c, cs.Index());
        }
        ParseReject("Unexpected end of input in string", cs.Index());
    }

    /**
     * @brief Parse a number value from the JSON input.
     *
     * @param cs A CursorSlice object representing the input JSON string.
     * @param result A NumberResultPack object to store the parsed number.
     *
     * This function parses a number from the JSON input, which can be an integer
     * or a floating-point number, including scientific notation. The parsing
     * uses a finite state machine to handle various parts of the number format,
     * such as the sign, integer and fractional parts, and scientific notation.
     *
     * @throw ParseError if the input string is invalid or if conversion
     * from string to number fails due to invalid argument or out of range.
     */
    void internal::OrdinaryParser::ParseNumberValue(CursorSlice &cs,
                                                    NumberResultPack &result) {
        auto parse_status = ParseJsonNumberStatus::kInitialStatus;
        bool is_integer = true;
        std::string number_str_cache;
        while (cs.NotEnd()) {
            const char c = cs.CurrentChar();
            switch (parse_status) {
                case ParseJsonNumberStatus::kInitialStatus:
                    if (c == '-') {
                        parse_status = ParseJsonNumberStatus::kMinusSignStatus;
                    } else if (IsZeroChar(c)) {
                        parse_status = ParseJsonNumberStatus::kLeadingZeroStatus;
                    } else if (IsDigitExceptZeroChar(c)) {
                        parse_status = ParseJsonNumberStatus::kDigitExceptZeroStatus;
                    } else {
                        parse_status = ParseJsonNumberStatus::kRejectStatus;
                        ParseReject(c, cs.Index());
                    }
                    break;
                case ParseJsonNumberStatus::kMinusSignStatus:
                    if (IsZeroChar(c)) {
                        parse_status = ParseJsonNumberStatus::kLeadingZeroStatus;
                    } else if (IsDigitExceptZeroChar(c)) {
                        parse_status = ParseJsonNumberStatus::kDigitExceptZeroStatus;
                    } else {
                        parse_status = ParseJsonNumberStatus::kRejectStatus;
                        ParseReject(c, cs.Index());
                    }
                    break;
                case ParseJsonNumberStatus::kLeadingZeroStatus:
                    if (c == '.') {
                        is_integer = false;
                        parse_status = ParseJsonNumberStatus::kDecimalPointStatus;
                    } else if (IsScientificChar(c)) {
                        is_integer = false;
                        parse_status = ParseJsonNumberStatus::kScientificSignStatus;
                    } else {
                        parse_status = ParseJsonNumberStatus::kEndStatus;
                    }
                    break;
                case ParseJsonNumberStatus::kDigitExceptZeroStatus:
                case ParseJsonNumberStatus::kDigitIntegerStatus:
                    if (IsDigitChar(c)) {
                        parse_status = ParseJsonNumberStatus::kDigitIntegerStatus;
                    } else if (c == '.') {
                        is_integer = false;
                        parse_status = ParseJsonNumberStatus::kDecimalPointStatus;
                    } else if (IsScientificChar(c)) {
                        is_integer = false;
                        parse_status = ParseJsonNumberStatus::kScientificSignStatus;
                    } else {
                        parse_status = ParseJsonNumberStatus::kEndStatus;
                    }
                    break;
                case ParseJsonNumberStatus::kDecimalPointStatus:
                    if (IsDigitChar(c)) {
                        parse_status = ParseJsonNumberStatus::kDigitAfterDecimalPointStatus;
                    } else {
                        parse_status = ParseJsonNumberStatus::kRejectStatus;
                        ParseReject(c, cs.Index());
                    }
                    break;
                case ParseJsonNumberStatus::kDigitAfterDecimalPointStatus:
                    if (IsDigitChar(c)) {
                        parse_status = ParseJsonNumberStatus::kDigitAfterDecimalPointStatus;
                    } else if (IsScientificChar(c)) {
                        parse_status = ParseJsonNumberStatus::kScientificSignStatus;
                    } else {
                        parse_status = ParseJsonNumberStatus::kEndStatus;
                    }
                    break;
                case ParseJsonNumberStatus::kScientificSignStatus:
                    if (c == '-' || c == '+') {
                        parse_status = ParseJsonNumberStatus::kMinusOrPosSignStatus;
                    } else if (IsDigitChar(c)) {
                        parse_status = ParseJsonNumberStatus::kDigitAfterScientificStatus;
                    } else {
                        parse_status = ParseJsonNumberStatus::kRejectStatus;
                        ParseReject(c, cs.Index());
                    }
                    break;
                case ParseJsonNumberStatus::kMinusOrPosSignStatus:
                    if (IsDigitChar(c)) {
                        parse_status = ParseJsonNumberStatus::kDigitAfterScientificStatus;
                    } else {
                        parse_status = ParseJsonNumberStatus::kRejectStatus;
                        ParseReject(c, cs.Index());
                    }
                    break;
                case ParseJsonNumberStatus::kDigitAfterScientificStatus:
                    if (IsDigitChar(c)) {
                        parse_status = ParseJsonNumberStatus::kDigitAfterScientificStatus;
                    } else {
                        parse_status = ParseJsonNumberStatus::kEndStatus;
                    }
                    break;
                case ParseJsonNumberStatus::kEndStatus:
                case ParseJsonNumberStatus::kRejectStatus:
                    break;
            }
            if (parse_status == ParseJsonNumberStatus::kEndStatus) break;
            number_str_cache.push_back(c);
            cs.MoveNext();
        }

        if (parse_status != ParseJsonNumberStatus::kEndStatus) {
            switch (parse_status) {
                case ParseJsonNumberStatus::kLeadingZeroStatus:
                case ParseJsonNumberStatus::kDigitExceptZeroStatus:
                case ParseJsonNumberStatus::kDigitIntegerStatus:
                case ParseJsonNumberStatus::kDigitAfterDecimalPointStatus:
                case ParseJsonNumberStatus::kDigitAfterScientificStatus:
                    break;
                default:
                    ParseReject("Unexpected end of input in number", cs.Index());
            }
        }

        // convert string to number
        try {
            result.is_integer_ = is_integer;
            if (result.is_integer_)
                result.integer_data_ = std::stoll(number_str_cache);
            else
                result.double_data_ = std::stod(number_str_cache);
        } catch (const std::exception &e) {
            ParseReject(std::string("invalid number: ") + e.what(), cs.Index());
        }
    }

    /**
     * @brief Parse a boolean value.
     *
     * @param cs A CursorSlice object which represents input JSON string.
     *
     * @return true if the boolean value is true, false otherwise.
     *
     * @throw ParseError if input string is invalid.
     */
    bool internal::OrdinaryParser::ParseBoolValue(CursorSlice &cs) {
        const bool result = cs.CurrentChar() == kTrueCharList[0];
        const size_t end = result ? kTrueCharList.size() : kFalseCharList.size();
        if (cs.RemainCount() < end)
            ParseReject("Unexpected end of input", cs.Index());
        for (size_t i = 0; i < end; ++i) {
            const char expect = result ? kTrueCharList[i] : kFalseCharList[i];
            const char c = cs.Next();
            if (c != expect) ParseReject(expect, c, cs.Index());
        }
        return result;
    }

    /**
     * @brief Parse a null value.
     *
     * @param cs A CursorSlice object which represents input JSON string.
     *
     * @return nullptr.
     *
     * @throw ParseError if input string is invalid.
     */
    void *internal::OrdinaryParser::ParseNullValue(CursorSlice &cs) {
        if (cs.RemainCount() < kNullCharList.size())
            ParseReject("Unexpected end of input", cs.Index());
        for (const char expect: kNullCharList) {
            const char c = cs.Next();
            if (c != expect) ParseReject(expect, c, cs.Index());
        }
        return nullptr;
    }

    /**
     * @brief Append a string as a JSON string literal (quoted and escaped).
     *
     * @param s: The raw string to serialize.
     * @param out: The output stream to append to.
     */
    static void AppendJsonString(const std::string &s, std::ostream &out) {
        static constexpr char kHexDigits[] = "0123456789abcdef";
        out << kStringStart;
        for (const char c: s) {
            switch (c) {
                case kStringStart: out << "\\\"";
                    break;
                case '\\': out << "\\\\";
                    break;
                case '\b': out << "\\b";
                    break;
                case '\f': out << "\\f";
                    break;
                case '\n': out << "\\n";
                    break;
                case '\r': out << "\\r";
                    break;
                case '\t': out << "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        out << "\\u00"
                                << kHexDigits[(static_cast<unsigned char>(c) >> 4) & 0xF]
                                << kHexDigits[static_cast<unsigned char>(c) & 0xF];
                    } else {
                        out << c;
                    }
                    break;
            }
        }
        out << kStringEnd;
    }

    /**
     * @brief Write the node as a JSON string to the given output stream.
     *
     * @param os: The output stream to write to.
     */
    void OrdinaryJsonNode::StringifyTo(std::ostream &os) const {
        switch (value_type_) {
            case NodeValueTypeEnum::kObject: {
                os << kObjectStart;
                bool first = true;
                for (const auto &kv: *object_data_) {
                    if (!first) os << kSeparator;
                    first = false;
                    AppendJsonString(kv.first, os);
                    os << kKeyValueSep;
                    kv.second.StringifyTo(os);
                }
                os << kObjectEnd;
                break;
            }
            case NodeValueTypeEnum::kArray: {
                os << kArrayStart;
                bool first = true;
                for (const auto &elem: *array_data_) {
                    if (!first) os << kSeparator;
                    first = false;
                    elem.StringifyTo(os);
                }
                os << kArrayEnd;
                break;
            }
            case NodeValueTypeEnum::kString:
                AppendJsonString(*string_data_, os);
                break;
            case NodeValueTypeEnum::kInteger:
                os << integer_data_;
                break;
            case NodeValueTypeEnum::kDouble:
                os << std::setprecision(std::numeric_limits<double>::max_digits10)
                        << double_data_;
                break;
            case NodeValueTypeEnum::kBool:
                os << (boolean_data_ ? "true" : "false");
                break;
            case NodeValueTypeEnum::kNull:
                os << "null";
                break;
            case NodeValueTypeEnum::kUnknown:
                throw TypeError("cannot serialize a node without a value");
        }
    }

    /**
     * @brief Return a string representation of the JSON node.
     *
     * This function returns a string representation of the JSON node, which is
     * a valid JSON string. The string representation is created in a
     * depth-first manner, i.e., the string representation of the node's value
     * is created first, and then the string representation of the node itself
     * is created by concatenating the string representation of the value with
     * the appropriate JSON syntax.
     *
     * @return A string representation of the JSON node.
     *
     * @throw TypeError if the node has no value.
     */
    std::string OrdinaryJsonNode::Stringify() const {
        std::ostringstream oss;
        StringifyTo(oss);
        return oss.str();
    }

    /**
     * @brief Return a string representation of the node type.
     *
     * @return "object", "array", "string", "integer", "double", "bool", "null", or
     *         "unknown".
     */
    std::string OrdinaryJsonNode::GetValueTypeToString() const {
        switch (value_type_) {
            case NodeValueTypeEnum::kObject:
                return "object";
            case NodeValueTypeEnum::kArray:
                return "array";
            case NodeValueTypeEnum::kString:
                return "string";
            case NodeValueTypeEnum::kInteger:
                return "integer";
            case NodeValueTypeEnum::kDouble:
                return "double";
            case NodeValueTypeEnum::kBool:
                return "bool";
            case NodeValueTypeEnum::kNull:
                return "null";
            case NodeValueTypeEnum::kUnknown:
                return "unknown";
        }
        return "unknown";
    }

    void internal::OrdinaryParser::ParseReject(const char expect, const char actual,
                                               const size_t pos) {
        std::ostringstream oss;
        oss << "unexpected character '" << actual << "' (expected '" << expect
                << "')";
        ParseReject(oss.str(), pos);
    }

    void internal::OrdinaryParser::ParseReject(const char actual, const size_t pos) {
        std::ostringstream oss;
        oss << "unexpected character '" << actual << "'";
        ParseReject(oss.str(), pos);
    }

    void internal::OrdinaryParser::ParseReject(const std::string &error_msg,
                                               const char actual, const size_t pos) {
        std::ostringstream oss;
        oss << error_msg << " (got '" << actual << "')";
        ParseReject(oss.str(), pos);
    }

    void internal::OrdinaryParser::ParseReject(const std::string &error_msg,
                                               const size_t pos) {
        std::ostringstream oss;
        oss << "parse error at byte " << pos << ": " << error_msg;
        throw ParseError(oss.str(), pos);
    }

    void internal::OrdinaryParser::SkipWhiteSpace(CursorSlice &cs) {
        while (cs.NotEnd() && IsWhiteSpaceChar(cs.CurrentChar())) cs.MoveNext();
    }

    void OrdinaryJsonNode::RejectMismatchedType(
        const std::string &expected_type, const std::string &actual_type) {
        std::ostringstream oss;
        oss << "cannot access a value of type '" << actual_type << "' as '"
                << expected_type << "'";
        throw TypeError(oss.str());
    }

    /**
     * @brief Parse a JSON string.
     *
     * @param value A JSON string.
     *
     * @return The parsed JSON node.
     *
     * The function throws a ParseError if the input string is invalid.
     */
    OrdinaryJsonNode Parse(const std::string &value) {
        internal::CursorSlice cs(value);
        OrdinaryJsonNode result = internal::OrdinaryParser::ParseValue(cs);
        if (cs.NotEnd()) {
            internal::OrdinaryParser::ParseReject(
                "Unexpected trailing data after the value", cs.Index());
        }
        return result;
    }

    /**
     * @brief Converts an OrdinaryJsonNode to its string representation.
     *
     * @param value The OrdinaryJsonNode to be converted.
     * @return A string representing the JSON node.
     */
    std::string Stringify(const OrdinaryJsonNode &value) {
        return value.Stringify();
    }
} // namespace ordinaryjson
