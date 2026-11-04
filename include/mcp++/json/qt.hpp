// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#ifndef QT_NO_KEYWORDS
#define QT_NO_KEYWORDS
#endif
#include <mcp++/json.hpp>
#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mcp {

struct qt_json_value {
    using object_type = std::map<std::string, qt_json_value, std::less<>>;
    using array_type = std::vector<qt_json_value>;
    using storage_type = std::variant<
        std::monostate,
        object_type,
        array_type,
        std::string,
        bool,
        std::int64_t,
        std::uint64_t,
        double
    >;

    storage_type data{};

    QJsonValue to_qjson() const {
        if (std::holds_alternative<std::monostate>(data))
            return QJsonValue(QJsonValue::Null);
        if (const auto *b = std::get_if<bool>(&data))
            return QJsonValue(*b);
        if (const auto *i = std::get_if<std::int64_t>(&data))
            return QJsonValue(static_cast<qint64>(*i));
        if (const auto *u = std::get_if<std::uint64_t>(&data)) {
            if (*u <= static_cast<std::uint64_t>(std::numeric_limits<qint64>::max()))
                return QJsonValue(static_cast<qint64>(*u));
            return QJsonValue(static_cast<double>(*u));
        }
        if (const auto *d = std::get_if<double>(&data))
            return QJsonValue(*d);
        if (const auto *s = std::get_if<std::string>(&data))
            return QJsonValue(QString::fromUtf8(s->data(), static_cast<qsizetype>(s->size())));
        if (const auto *a = std::get_if<array_type>(&data)) {
            QJsonArray arr;
            for (const auto &item : *a)
                arr.append(item.to_qjson());
            return arr;
        }
        if (const auto *o = std::get_if<object_type>(&data)) {
            QJsonObject obj;
            for (const auto &[k, v] : *o)
                obj.insert(QString::fromUtf8(k.data(), static_cast<qsizetype>(k.size())), v.to_qjson());
            return obj;
        }
        return QJsonValue();
    }

    static qt_json_value from_qjson(const QJsonValue &qv) {
        qt_json_value out;
        switch (qv.type()) {
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            out.data = std::monostate{};
            break;
        case QJsonValue::Bool:
            out.data = qv.toBool();
            break;
        case QJsonValue::Double: {
            const double d = qv.toDouble();
            if (!std::isfinite(d))
                throw parse_error("Non-finite number in QJsonValue");
            const qint64 i = qv.toInteger();
            if (static_cast<double>(i) == d)
                out.data = static_cast<std::int64_t>(i);
            else
                out.data = d;
            break;
        }
        case QJsonValue::String: {
            const QByteArray utf8 = qv.toString().toUtf8();
            out.data = std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
            break;
        }
        case QJsonValue::Array: {
            array_type arr;
            const QJsonArray qa = qv.toArray();
            arr.reserve(static_cast<std::size_t>(qa.size()));
            for (const QJsonValue &item : qa)
                arr.push_back(from_qjson(item));
            out.data = std::move(arr);
            break;
        }
        case QJsonValue::Object: {
            object_type obj;
            const QJsonObject qo = qv.toObject();
            for (auto it = qo.constBegin(); it != qo.constEnd(); ++it) {
                const QByteArray k = it.key().toUtf8();
                obj.insert_or_assign(std::string(k.constData(), static_cast<std::size_t>(k.size())),
                                     from_qjson(it.value()));
            }
            out.data = std::move(obj);
            break;
        }
        }
        return out;
    }
};

struct qt_backend {
    using value = qt_json_value;

private:
    struct parser {
        std::string_view src;
        std::size_t pos = 0;

        void skip_ws() {
            while (pos < src.size()) {
                const char c = src[pos];
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                    ++pos;
                else
                    break;
            }
        }

        char peek() const {
            if (pos >= src.size())
                throw parse_error("Unexpected end of JSON input");
            return src[pos];
        }

        char get() {
            char c = peek();
            ++pos;
            return c;
        }

        void expect_literal(std::string_view lit) {
            if (src.substr(pos, lit.size()) != lit)
                throw parse_error("Invalid JSON literal");
            pos += lit.size();
        }

        static void append_utf8(std::string &out, std::uint32_t cp) {
            if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
                throw parse_error("Invalid Unicode code point");
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

        std::uint16_t parse_hex4() {
            if (pos + 4 > src.size())
                throw parse_error("Incomplete unicode escape");
            std::uint16_t val = 0;
            for (int i = 0; i < 4; ++i) {
                char c = src[pos++];
                val <<= 4;
                if (c >= '0' && c <= '9') val |= static_cast<std::uint16_t>(c - '0');
                else if (c >= 'a' && c <= 'f') val |= static_cast<std::uint16_t>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') val |= static_cast<std::uint16_t>(c - 'A' + 10);
                else throw parse_error("Invalid hex digit in unicode escape");
            }
            return val;
        }

        std::string parse_string(int depth) {
            (void)depth;
            if (get() != '"')
                throw parse_error("Expected '\"'");
            std::string out;
            while (pos < src.size()) {
                unsigned char c = static_cast<unsigned char>(src[pos++]);
                if (c == '"')
                    return out;
                if (c < 0x20)
                    throw parse_error("Unescaped control character in JSON string");
                if (c != '\\') {
                    out.push_back(static_cast<char>(c));
                    continue;
                }
                if (pos >= src.size())
                    throw parse_error("Untruncated escape sequence");
                char esc = src[pos++];
                switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    std::uint32_t cp = parse_hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (pos + 2 > src.size() || src[pos] != '\\' || src[pos + 1] != 'u')
                            throw parse_error("Missing low surrogate in JSON string");
                        pos += 2;
                        std::uint32_t low = parse_hex4();
                        if (low < 0xDC00 || low > 0xDFFF)
                            throw parse_error("Invalid low surrogate in JSON string");
                        cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        throw parse_error("Unexpected lone low surrogate");
                    }
                    append_utf8(out, cp);
                    break;
                }
                default:
                    throw parse_error("Invalid escape character in JSON string");
                }
            }
            throw parse_error("Unterminated JSON string");
        }

        value parse_number() {
            const std::size_t start = pos;
            bool negative = false;
            if (src[pos] == '-') {
                negative = true;
                ++pos;
            }
            if (pos >= src.size())
                throw parse_error("Invalid number");
            if (src[pos] == '0') {
                ++pos;
                if (pos < src.size() && src[pos] >= '0' && src[pos] <= '9')
                    throw parse_error("Leading zeros are not allowed in JSON numbers");
            } else if (src[pos] >= '1' && src[pos] <= '9') {
                while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9')
                    ++pos;
            } else {
                throw parse_error("Invalid number syntax");
            }

            bool is_float = false;
            if (pos < src.size() && src[pos] == '.') {
                is_float = true;
                ++pos;
                if (pos >= src.size() || src[pos] < '0' || src[pos] > '9')
                    throw parse_error("Expected digit after decimal point");
                while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9')
                    ++pos;
            }
            if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
                is_float = true;
                ++pos;
                if (pos < src.size() && (src[pos] == '+' || src[pos] == '-'))
                    ++pos;
                if (pos >= src.size() || src[pos] < '0' || src[pos] > '9')
                    throw parse_error("Expected digit in exponent");
                while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9')
                    ++pos;
            }

            std::string_view token = src.substr(start, pos - start);
            if (!is_float) {
                if (negative) {
                    std::int64_t iv = 0;
                    auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), iv);
                    if (ec == std::errc{} && ptr == token.data() + token.size())
                        return value{iv};
                } else {
                    std::uint64_t uv = 0;
                    auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), uv);
                    if (ec == std::errc{} && ptr == token.data() + token.size()) {
                        if (uv <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                            return value{static_cast<std::int64_t>(uv)};
                        return value{uv};
                    }
                }
            }
            double dv = 0.0;
            auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), dv);
            if (ec != std::errc{} || ptr != token.data() + token.size() || !std::isfinite(dv))
                throw parse_error("JSON number overflow or invalid float");
            return value{dv};
        }

        value parse_value(int depth) {
            if (depth > 512)
                throw parse_error("Maximum JSON nesting depth exceeded");
            skip_ws();
            char c = peek();
            if (c == 'n') {
                expect_literal("null");
                return value{std::monostate{}};
            }
            if (c == 't') {
                expect_literal("true");
                return value{true};
            }
            if (c == 'f') {
                expect_literal("false");
                return value{false};
            }
            if (c == '"')
                return value{parse_string(depth)};
            if (c == '[') {
                ++pos;
                skip_ws();
                value::array_type arr;
                if (peek() == ']') {
                    ++pos;
                    return value{std::move(arr)};
                }
                while (true) {
                    arr.push_back(parse_value(depth + 1));
                    skip_ws();
                    char sep = get();
                    if (sep == ']')
                        break;
                    if (sep != ',')
                        throw parse_error("Expected ',' or ']' in array");
                }
                return value{std::move(arr)};
            }
            if (c == '{') {
                ++pos;
                skip_ws();
                value::object_type obj;
                if (peek() == '}') {
                    ++pos;
                    return value{std::move(obj)};
                }
                while (true) {
                    skip_ws();
                    if (peek() != '"')
                        throw parse_error("Expected string key in object");
                    std::string key = parse_string(depth + 1);
                    skip_ws();
                    if (get() != ':')
                        throw parse_error("Expected ':' after object key");
                    value val = parse_value(depth + 1);
                    obj.insert_or_assign(std::move(key), std::move(val));
                    skip_ws();
                    char sep = get();
                    if (sep == '}')
                        break;
                    if (sep != ',')
                        throw parse_error("Expected ',' or '}' in object");
                }
                return value{std::move(obj)};
            }
            if (c == '-' || (c >= '0' && c <= '9'))
                return parse_number();
            throw parse_error("Unexpected character in JSON");
        }
    };

    static void dump_string(std::string &out, std::string_view s) {
        const QByteArrayView bv(s.data(), static_cast<qsizetype>(s.size()));
        if (!bv.isValidUtf8())
            throw std::domain_error("Invalid UTF-8 in JSON output");
        out.push_back('"');
        for (unsigned char c : s) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    constexpr char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0xF]);
                    out.push_back(hex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
            }
        }
        out.push_back('"');
    }

    static void dump_into(std::string &out, const value &v) {
        switch (kind(v)) {
        case json_kind::null:
            out += "null";
            break;
        case json_kind::boolean:
            out += std::get<bool>(v.data) ? "true" : "false";
            break;
        case json_kind::signed_integer:
            out += QByteArray::number(static_cast<qlonglong>(std::get<std::int64_t>(v.data))).toStdString();
            break;
        case json_kind::unsigned_integer:
            out += QByteArray::number(static_cast<qulonglong>(std::get<std::uint64_t>(v.data))).toStdString();
            break;
        case json_kind::number: {
            const double d = std::get<double>(v.data);
            if (!std::isfinite(d))
                throw std::domain_error("Non-finite JSON number");
            char buf[64];
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), d);
            if (ec != std::errc{})
                throw std::domain_error("Failed to format JSON number");
            out.append(buf, ptr);
            break;
        }
        case json_kind::string:
            dump_string(out, std::get<std::string>(v.data));
            break;
        case json_kind::array: {
            out.push_back('[');
            const auto &arr = std::get<value::array_type>(v.data);
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (i > 0)
                    out.push_back(',');
                dump_into(out, arr[i]);
            }
            out.push_back(']');
            break;
        }
        case json_kind::object: {
            out.push_back('{');
            const auto &obj = std::get<value::object_type>(v.data);
            bool first = true;
            for (const auto &[k, val] : obj) {
                if (!first)
                    out.push_back(',');
                first = false;
                dump_string(out, k);
                out.push_back(':');
                dump_into(out, val);
            }
            out.push_back('}');
            break;
        }
        }
    }

public:
    static value parse(std::string_view s) {
        const QByteArrayView bv(s.data(), static_cast<qsizetype>(s.size()));
        if (!bv.isValidUtf8())
            throw parse_error("Invalid UTF-8 in JSON input");
        parser p{s, 0};
        value result = p.parse_value(0);
        p.skip_ws();
        if (p.pos != s.size())
            throw parse_error("Trailing data after JSON value");
        return result;
    }

    static std::string dump(const value &v) {
        std::string out;
        dump_into(out, v);
        return out;
    }

    static json_kind kind(const value &v) {
        if (std::holds_alternative<value::object_type>(v.data)) return json_kind::object;
        if (std::holds_alternative<value::array_type>(v.data)) return json_kind::array;
        if (std::holds_alternative<std::string>(v.data)) return json_kind::string;
        if (std::holds_alternative<bool>(v.data)) return json_kind::boolean;
        if (std::holds_alternative<std::int64_t>(v.data)) return json_kind::signed_integer;
        if (std::holds_alternative<std::uint64_t>(v.data)) return json_kind::unsigned_integer;
        if (std::holds_alternative<double>(v.data)) return json_kind::number;
        return json_kind::null;
    }

    static value object() { return value{value::object_type{}}; }
    static value array() { return value{value::array_type{}}; }
    static value string(std::string_view s) { return value{std::string(s)}; }
    static value integer(std::int64_t n) { return value{n}; }
    static value number(double n) {
        if (!std::isfinite(n))
            throw std::domain_error("Non-finite JSON number");
        return value{n};
    }
    static value boolean(bool b) { return value{b}; }

    static const value *find(const value &v, std::string_view name) {
        const auto *obj = std::get_if<value::object_type>(&v.data);
        if (!obj)
            return nullptr;
        auto it = obj->find(name);
        return it == obj->end() ? nullptr : &it->second;
    }

    static std::vector<std::string> keys(const value &v) {
        std::vector<std::string> result;
        const auto &obj = std::get<value::object_type>(v.data);
        result.reserve(obj.size());
        for (const auto &[k, _] : obj)
            result.push_back(k);
        return result;
    }

    static std::string_view as_string(const value &v) {
        return std::get<std::string>(v.data);
    }

    static std::int64_t as_i64(const value &v) {
        return std::get<std::int64_t>(v.data);
    }

    static std::uint64_t as_u64(const value &v) {
        if (const auto *u = std::get_if<std::uint64_t>(&v.data))
            return *u;
        return static_cast<std::uint64_t>(std::get<std::int64_t>(v.data));
    }

    static double as_double(const value &v) {
        if (const auto *d = std::get_if<double>(&v.data))
            return *d;
        if (const auto *i = std::get_if<std::int64_t>(&v.data))
            return static_cast<double>(*i);
        return static_cast<double>(std::get<std::uint64_t>(v.data));
    }

    static bool as_bool(const value &v) {
        return std::get<bool>(v.data);
    }

    static void set(value &v, std::string_view name, value field) {
        std::get<value::object_type>(v.data).insert_or_assign(std::string(name), std::move(field));
    }

    static void append(value &v, value item) {
        std::get<value::array_type>(v.data).push_back(std::move(item));
    }
};

static_assert(json_backend<qt_backend>);

} // namespace mcp
