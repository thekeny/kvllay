#ifndef KVLLAY_RESP_HPP
#define KVLLAY_RESP_HPP

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <charconv>

namespace kvllay {

enum class ParseStatus {
    Success,
    Incomplete,
    Error
};

class Resp {
public:
    static inline void append_ok(std::string& out) {
        out.append("+OK\r\n", 5);
    }

    static inline void append_pong(std::string& out) {
        out.append("+PONG\r\n", 7);
    }

    static inline void append_null(std::string& out, int protocol = 2) {
        if (protocol == 3) {
            out.append("_\r\n", 3);
        } else {
            out.append("$-1\r\n", 5);
        }
    }

    static inline void append_null_bulk_string(std::string& out, int protocol = 2) {
        append_null(out, protocol);
    }

    static inline void append_null_array(std::string& out, int protocol = 2) {
        if (protocol == 3) {
            out.append("_\r\n", 3);
        } else {
            out.append("*-1\r\n", 5);
        }
    }

    static inline void append_empty_array(std::string& out) {
        out.append("*0\r\n", 4);
    }

    static inline void append_simple_string(std::string& out, std::string_view str) {
        if (str == "OK") {
            append_ok(out);
            return;
        }
        if (str == "PONG") {
            append_pong(out);
            return;
        }
        out.push_back('+');
        out.append(str.data(), str.size());
        out.append("\r\n", 2);
    }

    static inline void append_error(std::string& out, std::string_view err) {
        if (err.rfind("ERR ", 0) == 0 || err.rfind("WRONGTYPE ", 0) == 0 || err.rfind("OOM ", 0) == 0) {
            out.push_back('-');
            out.append(err.data(), err.size());
            out.append("\r\n", 2);
            return;
        }
        out.append("-ERR ", 5);
        out.append(err.data(), err.size());
        out.append("\r\n", 2);
    }

    static inline void append_integer(std::string& out, long long val) {
        char buf[32];
        buf[0] = ':';
        auto [ptr, ec] = std::to_chars(buf + 1, buf + sizeof(buf) - 2, val);
        *ptr++ = '\r';
        *ptr++ = '\n';
        out.append(buf, ptr - buf);
    }

    static inline void append_bulk_string(std::string& out, std::string_view val) {
        char buf[32];
        buf[0] = '$';
        auto [ptr, ec] = std::to_chars(buf + 1, buf + sizeof(buf) - 2, val.size());
        *ptr++ = '\r';
        *ptr++ = '\n';
        out.append(buf, ptr - buf);
        out.append(val.data(), val.size());
        out.append("\r\n", 2);
    }

    static inline void append_array_header(std::string& out, size_t count) {
        char buf[32];
        buf[0] = '*';
        auto [ptr, ec] = std::to_chars(buf + 1, buf + sizeof(buf) - 2, count);
        *ptr++ = '\r';
        *ptr++ = '\n';
        out.append(buf, ptr - buf);
    }

    static std::string simple_string(const std::string& str) {
        if (str == "OK") return ok();
        if (str == "PONG") return pong();
        return "+" + str + "\r\n";
    }

    static const std::string& ok() {
        static const std::string s = "+OK\r\n";
        return s;
    }

    static const std::string& pong() {
        static const std::string s = "+PONG\r\n";
        return s;
    }

    static std::string error(const std::string& err) {
        if (err.rfind("ERR ", 0) == 0 || err.rfind("WRONGTYPE ", 0) == 0 || err.rfind("OOM ", 0) == 0) {
            return "-" + err + "\r\n";
        }
        return "-ERR " + err + "\r\n";
    }

    static std::string integer(long long val) {
        return ":" + std::to_string(val) + "\r\n";
    }

    static std::string bulk_string(const std::string& val) {
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    }

    static const std::string& null_bulk_string() {
        static const std::string s = "$-1\r\n";
        return s;
    }

    static const std::string& null_array() {
        static const std::string s = "*-1\r\n";
        return s;
    }

    static const std::string& empty_array() {
        static const std::string s = "*0\r\n";
        return s;
    }

    static std::string array(const std::vector<std::string>& items) {
        std::string res;
        append_array_header(res, items.size());
        for (const auto& item : items) {
            append_bulk_string(res, item);
        }
        return res;
    }

    static std::string array_of_bulk(const std::vector<std::optional<std::string>>& items) {
        std::string res;
        append_array_header(res, items.size());
        for (const auto& item : items) {
            if (item.has_value()) {
                append_bulk_string(res, *item);
            } else {
                append_null_bulk_string(res);
            }
        }
        return res;
    }

    static ParseStatus parse_command(std::string_view buffer, std::vector<std::string_view>& args, size_t& consumed_bytes, std::string& unescape_buf) {
        args.clear();
        consumed_bytes = 0;
        unescape_buf.clear();

        if (buffer.empty()) {
            return ParseStatus::Incomplete;
        }

        if (buffer[0] == '*') {
            return parse_resp_array(buffer, args, consumed_bytes);
        }

        return parse_inline_command(buffer, args, consumed_bytes, unescape_buf);
    }

    static ParseStatus parse_command(std::string_view buffer, std::vector<std::string>& args, size_t& consumed_bytes) {
        std::vector<std::string_view> sv_args;
        std::string unescape_buf;
        ParseStatus status = parse_command(buffer, sv_args, consumed_bytes, unescape_buf);
        if (status == ParseStatus::Success) {
            args.clear();
            args.reserve(sv_args.size());
            for (const auto& a : sv_args) {
                args.emplace_back(a);
            }
        }
        return status;
    }

private:
    static ParseStatus parse_resp_array(std::string_view buffer, std::vector<std::string_view>& args, size_t& consumed_bytes) {
        size_t pos = buffer.find("\r\n");
        if (pos == std::string_view::npos) {
            return ParseStatus::Incomplete;
        }

        std::string_view count_sv = buffer.substr(1, pos - 1);
        long long count = 0;
        auto [ptr1, ec1] = std::from_chars(count_sv.data(), count_sv.data() + count_sv.size(), count);
        if (ec1 != std::errc() || ptr1 != count_sv.data() + count_sv.size()) {
            return ParseStatus::Error;
        }

        if (count < 0) {
            consumed_bytes = pos + 2;
            return ParseStatus::Success;
        }

        size_t current = pos + 2;
        args.reserve(count);

        for (long long i = 0; i < count; ++i) {
            if (current >= buffer.size()) {
                return ParseStatus::Incomplete;
            }

            if (buffer[current] != '$') {
                return ParseStatus::Error;
            }

            size_t crlf = buffer.find("\r\n", current);
            if (crlf == std::string_view::npos) {
                return ParseStatus::Incomplete;
            }

            std::string_view len_sv = buffer.substr(current + 1, crlf - (current + 1));
            long long str_len = 0;
            auto [ptr2, ec2] = std::from_chars(len_sv.data(), len_sv.data() + len_sv.size(), str_len);
            if (ec2 != std::errc() || ptr2 != len_sv.data() + len_sv.size()) {
                return ParseStatus::Error;
            }

            if (str_len < 0) {
                args.emplace_back("");
                current = crlf + 2;
                continue;
            }

            size_t data_start = crlf + 2;
            size_t data_end = data_start + str_len;
            if (buffer.size() < data_end + 2) {
                return ParseStatus::Incomplete;
            }

            if (buffer[data_end] != '\r' || buffer[data_end + 1] != '\n') {
                return ParseStatus::Error;
            }

            args.emplace_back(buffer.data() + data_start, str_len);
            current = data_end + 2;
        }

        consumed_bytes = current;
        return ParseStatus::Success;
    }

    static ParseStatus parse_inline_command(std::string_view buffer, std::vector<std::string_view>& args, size_t& consumed_bytes, std::string& unescape_buf) {
        size_t line_end = buffer.find("\r\n");
        size_t delim_len = 2;
        if (line_end == std::string_view::npos) {
            line_end = buffer.find('\n');
            delim_len = 1;
        }

        if (line_end == std::string_view::npos) {
            return ParseStatus::Incomplete;
        }

        std::string_view line = buffer.substr(0, line_end);
        consumed_bytes = line_end + delim_len;

        size_t idx = 0;
        while (idx < line.size()) {
            while (idx < line.size() && std::isspace(static_cast<unsigned char>(line[idx]))) {
                idx++;
            }
            if (idx >= line.size()) break;

            if (line[idx] == '"' || line[idx] == '\'') {
                char quote = line[idx++];
                size_t start = idx;
                bool has_escape = false;
                while (idx < line.size() && line[idx] != quote) {
                    if (line[idx] == '\\') {
                        has_escape = true;
                        idx++;
                    }
                    if (idx < line.size()) {
                        idx++;
                    }
                }
                if (!has_escape) {
                    args.emplace_back(line.data() + start, idx - start);
                    if (idx < line.size() && line[idx] == quote) {
                        idx++;
                    }
                } else {
                    size_t unescape_start = unescape_buf.size();
                    for (size_t i = start; i < idx; ++i) {
                        if (line[i] == '\\' && i + 1 < idx) {
                            i++;
                        }
                        unescape_buf.push_back(line[i]);
                    }
                    if (idx < line.size() && line[idx] == quote) {
                        idx++;
                    }
                    args.emplace_back(unescape_buf.data() + unescape_start, unescape_buf.size() - unescape_start);
                }
            } else {
                size_t start = idx;
                while (idx < line.size() && !std::isspace(static_cast<unsigned char>(line[idx]))) {
                    idx++;
                }
                args.emplace_back(line.data() + start, idx - start);
            }
        }

        return ParseStatus::Success;
    }
};

}

#endif // KVLLAY_RESP_HPP
