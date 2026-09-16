#ifndef KVLLAY_COMMANDS_HPP
#define KVLLAY_COMMANDS_HPP

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <chrono>
#include <charconv>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <constants.hpp>
#include <resp.hpp>
#include <store.hpp>
#include <allocator.hpp>
#include <snapshot.hpp>
#include <aof.hpp>

namespace kvllay {

struct CommandResult {
    std::string response;
    bool should_close = false;
};

struct ClientSession {
    uint64_t id = 0;
    int protocol = 2;
    bool name_set = false;
    std::string name;
    std::string lib_name;
    std::string lib_ver;
};

class CommandHandler {
public:
    explicit CommandHandler(Store& store, SnapshotManager* snapshot_mgr = nullptr, AofManager* aof_mgr = nullptr)
        : store_(store), snapshot_mgr_(snapshot_mgr), aof_mgr_(aof_mgr),
          start_time_(std::chrono::steady_clock::now()) {}

    void set_snapshot_manager(SnapshotManager* mgr) { snapshot_mgr_ = mgr; }
    void set_aof_manager(AofManager* mgr) { aof_mgr_ = mgr; }

    void register_client(const ClientSession& client) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_[client.id] = client;
    }

    void unregister_client(uint64_t id) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(id);
    }

    static inline bool iequals(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::toupper(static_cast<unsigned char>(a[i])) != b[i]) {
                return false;
            }
        }
        return true;
    }

    void dispatch(const std::vector<std::string_view>& args, std::string& out, bool& authenticated, const std::string& server_password, bool& should_close, ClientSession& client) {
        if (args.empty()) {
            Resp::append_error(out, "empty command");
            return;
        }

        std::string_view cmd = args[0];

        if (iequals(cmd, "QUIT")) {
            Resp::append_ok(out);
            should_close = true;
            return;
        }

        if (iequals(cmd, "AUTH")) {
            handle_auth_sv(args, authenticated, server_password, out);
            return;
        }

        // HELLO is allowed before authentication because it can carry the
        // AUTH subcommand itself (HELLO 3 AUTH default password).
        if (iequals(cmd, "HELLO")) {
            handle_hello_sv(args, authenticated, server_password, client, out);
            return;
        }

        if (!server_password.empty() && !authenticated) {
            Resp::append_error(out, "NOAUTH Authentication required.");
            return;
        }

        size_t pre_out_len = out.size();
        bool is_mutating = false;
        std::vector<std::string> aof_args;

        switch (cmd.size()) {
        case 3: {
            char c0 = std::toupper(static_cast<unsigned char>(cmd[0]));
            char c1 = std::toupper(static_cast<unsigned char>(cmd[1]));
            char c2 = std::toupper(static_cast<unsigned char>(cmd[2]));
            if (c0 == 'G' && c1 == 'E' && c2 == 'T') {
                handle_get_sv(args, out, client.protocol);
            } else if (c0 == 'S' && c1 == 'E' && c2 == 'T') {
                is_mutating = handle_set_sv(args, out, client.protocol);
            } else if (c0 == 'D' && c1 == 'E' && c2 == 'L') {
                is_mutating = handle_del_sv(args, out);
            } else if (c0 == 'T' && c1 == 'T' && c2 == 'L') {
                handle_ttl_sv(args, out);
            } else {
                Resp::append_error(out, "unknown command '" + std::string(cmd) + "'");
            }
            break;
        }
        case 4: {
            char c0 = std::toupper(static_cast<unsigned char>(cmd[0]));
            char c1 = std::toupper(static_cast<unsigned char>(cmd[1]));
            char c2 = std::toupper(static_cast<unsigned char>(cmd[2]));
            char c3 = std::toupper(static_cast<unsigned char>(cmd[3]));
            if (c0 == 'P' && c1 == 'I' && c2 == 'N' && c3 == 'G') {
                handle_ping_sv(args, out);
            } else if (c0 == 'I' && c1 == 'N' && c2 == 'C' && c3 == 'R') {
                if (!store_.check_memory_and_evict()) {
                    Resp::append_error(out, "OOM command not allowed when used memory > 'maxmemory'.");
                } else {
                    is_mutating = true;
                    handle_incr_sv(args, out);
                }
            } else if (c0 == 'M' && c1 == 'G' && c2 == 'E' && c3 == 'T') {
                handle_mget_sv(args, out, client.protocol);
            } else if (c0 == 'M' && c1 == 'S' && c2 == 'E' && c3 == 'T') {
                if (!store_.check_memory_and_evict()) {
                    Resp::append_error(out, "OOM command not allowed when used memory > 'maxmemory'.");
                } else {
                    is_mutating = true;
                    handle_mset_sv(args, out);
                }
            } else if (c0 == 'D' && c1 == 'E' && c2 == 'C' && c3 == 'R') {
                if (!store_.check_memory_and_evict()) {
                    Resp::append_error(out, "OOM command not allowed when used memory > 'maxmemory'.");
                } else {
                    is_mutating = true;
                    handle_decr_sv(args, out);
                }
            } else if (c0 == 'L' && c1 == 'P' && c2 == 'O' && c3 == 'P') {
                is_mutating = true;
                handle_lpop_sv(args, out, client.protocol);
            } else if (c0 == 'R' && c1 == 'P' && c2 == 'O' && c3 == 'P') {
                is_mutating = true;
                handle_rpop_sv(args, out, client.protocol);
            } else if (c0 == 'L' && c1 == 'L' && c2 == 'E' && c3 == 'N') {
                handle_llen_sv(args, out);
            } else if (c0 == 'T' && c1 == 'Y' && c2 == 'P' && c3 == 'E') {
                handle_type_sv(args, out);
            } else if (c0 == 'I' && c1 == 'N' && c2 == 'F' && c3 == 'O') {
                handle_info_sv(args, out);
            } else if (c0 == 'E' && c1 == 'C' && c2 == 'H' && c3 == 'O') {
                handle_echo_sv(args, out);
            } else if (c0 == 'P' && c1 == 'T' && c2 == 'T' && c3 == 'L') {
                handle_pttl_sv(args, out);
            } else if (c0 == 'S' && c1 == 'A' && c2 == 'V' && c3 == 'E') {
                handle_save_sv(args, out);
            } else if (c0 == 'K' && c1 == 'E' && c2 == 'Y' && c3 == 'S') {
                handle_keys_sv(args, out);
            } else {
                Resp::append_error(out, "unknown command '" + std::string(cmd) + "'");
            }
            break;
        }
        case 5: {
            if (iequals(cmd, "LPUSH")) {
                if (!store_.check_memory_and_evict()) {
                    Resp::append_error(out, "OOM command not allowed when used memory > 'maxmemory'.");
                } else {
                    is_mutating = true;
                    handle_lpush_sv(args, out);
                }
            } else if (iequals(cmd, "RPUSH")) {
                if (!store_.check_memory_and_evict()) {
                    Resp::append_error(out, "OOM command not allowed when used memory > 'maxmemory'.");
                } else {
                    is_mutating = true;
                    handle_rpush_sv(args, out);
                }
            } else if (iequals(cmd, "SETEX")) {
                if (!store_.check_memory_and_evict()) {
                    Resp::append_error(out, "OOM command not allowed when used memory > 'maxmemory'.");
                } else {
                    is_mutating = true;
                    handle_setex_sv(args, out);
                }
            } else {
                Resp::append_error(out, "unknown command '" + std::string(cmd) + "'");
            }
            break;
        }
        case 6: {
            if (iequals(cmd, "EXISTS")) {
                handle_exists_sv(args, out);
            } else if (iequals(cmd, "INCRBY")) {
                if (!store_.check_memory_and_evict()) {
                    Resp::append_error(out, "OOM command not allowed when used memory > 'maxmemory'.");
                } else {
                    is_mutating = true;
                    handle_incrby_sv(args, out);
                }
            } else if (iequals(cmd, "DECRBY")) {
                if (!store_.check_memory_and_evict()) {
                    Resp::append_error(out, "OOM command not allowed when used memory > 'maxmemory'.");
                } else {
                    is_mutating = true;
                    handle_decrby_sv(args, out);
                }
            } else if (iequals(cmd, "EXPIRE")) {
                is_mutating = true;
                handle_expire_sv(args, out, aof_args, is_mutating);
            } else if (iequals(cmd, "LRANGE")) {
                handle_lrange_sv(args, out);
            } else if (iequals(cmd, "LINDEX")) {
                handle_lindex_sv(args, out, client.protocol);
            } else if (iequals(cmd, "DBSIZE")) {
                handle_dbsize_sv(args, out);
            } else if (iequals(cmd, "CONFIG")) {
                handle_config_sv(args, out);
            } else if (iequals(cmd, "CLIENT")) {
                handle_client_sv(args, client, out);
            } else if (iequals(cmd, "BGSAVE")) {
                handle_bgsave_sv(args, out);
            } else if (iequals(cmd, "SELECT")) {
                handle_select_sv(args, out);
            } else {
                Resp::append_error(out, "unknown command '" + std::string(cmd) + "'");
            }
            break;
        }
        case 7: {
            if (iequals(cmd, "COMMAND")) {
                handle_command_sv(args, out);
            } else if (iequals(cmd, "PEXPIRE")) {
                is_mutating = true;
                handle_pexpire_sv(args, out, aof_args, is_mutating);
            } else if (iequals(cmd, "PERSIST")) {
                is_mutating = true;
                handle_persist_sv(args, out, is_mutating);
            } else if (iequals(cmd, "FLUSHDB")) {
                is_mutating = true;
                handle_flushdb_sv(args, out);
            } else {
                Resp::append_error(out, "unknown command '" + std::string(cmd) + "'");
            }
            break;
        }
        case 8: {
            if (iequals(cmd, "LASTSAVE")) {
                handle_lastsave_sv(args, out);
            } else if (iequals(cmd, "FLUSHALL")) {
                is_mutating = true;
                handle_flushdb_sv(args, out);
            } else {
                Resp::append_error(out, "unknown command '" + std::string(cmd) + "'");
            }
            break;
        }
        case 12: {
            if (iequals(cmd, "BGREWRITEAOF")) {
                handle_bgrewriteaof_sv(args, out);
            } else {
                Resp::append_error(out, "unknown command '" + std::string(cmd) + "'");
            }
            break;
        }
        default: {
            Resp::append_error(out, "unknown command '" + std::string(cmd) + "'");
            break;
        }
        }

        if (is_mutating && aof_mgr_ && aof_mgr_->is_enabled()) {
            std::string_view written(out.data() + pre_out_len, out.size() - pre_out_len);
            if (written.rfind("-ERR", 0) != 0 && written.rfind("-WRONG", 0) != 0 && written.rfind("-OOM", 0) != 0) {
                if (aof_args.empty()) {
                    aof_mgr_->append(args);
                } else {
                    aof_mgr_->append(aof_args);
                }
            }
        }
    }

    void dispatch(const std::vector<std::string_view>& args, std::string& out, bool& authenticated, const std::string& server_password, bool& should_close) {
        ClientSession client;
        dispatch(args, out, authenticated, server_password, should_close, client);
    }

    CommandResult dispatch(const std::vector<std::string>& args, bool& authenticated, const std::string& server_password) {
        std::vector<std::string_view> sv_args;
        sv_args.reserve(args.size());
        for (const auto& a : args) {
            sv_args.emplace_back(a);
        }
        std::string out;
        bool should_close = false;
        dispatch(sv_args, out, authenticated, server_password, should_close);
        return {std::move(out), should_close};
    }

private:
    Store& store_;
    SnapshotManager* snapshot_mgr_;
    AofManager* aof_mgr_;
    std::chrono::steady_clock::time_point start_time_;
    mutable std::mutex clients_mutex_;
    std::unordered_map<uint64_t, ClientSession> clients_;

    std::vector<ClientSession> client_snapshot() const {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::vector<ClientSession> result;
        result.reserve(clients_.size());
        for (const auto& entry : clients_) result.push_back(entry.second);
        return result;
    }

    void handle_client_sv(const std::vector<std::string_view>& args, ClientSession& client, std::string& out) {
        if (args.size() < 2) {
            Resp::append_error(out, "wrong number of arguments for 'client' command");
            return;
        }

        if (iequals(args[1], "ID")) {
            if (args.size() != 2) {
                Resp::append_error(out, "wrong number of arguments for 'client|id' command");
                return;
            }
            Resp::append_integer(out, static_cast<long long>(client.id));
            return;
        }

        if (iequals(args[1], "SETNAME")) {
            if (args.size() != 3) {
                Resp::append_error(out, "wrong number of arguments for 'client|setname' command");
                return;
            }
            if (args[2].size() > 512) {
                Resp::append_error(out, "client name cannot be longer than 512 characters");
                return;
            }
            if (args[2].find_first_of(" \t\r\n") != std::string_view::npos) {
                Resp::append_error(out, "client name cannot contain spaces, newlines or special characters");
                return;
            }
            client.name.assign(args[2]);
            client.name_set = true;
            if (client.id != 0) {
                register_client(client);
            }
            Resp::append_ok(out);
            return;
        }

        if (iequals(args[1], "GETNAME")) {
            if (args.size() != 2) {
                Resp::append_error(out, "wrong number of arguments for 'client|getname' command");
                return;
            }
            if (!client.name_set) Resp::append_null_bulk_string(out, client.protocol);
            else Resp::append_bulk_string(out, client.name);
            return;
        }

        if (iequals(args[1], "SETINFO")) {
            if (args.size() != 4) {
                Resp::append_error(out, "wrong number of arguments for 'client|setinfo' command");
                return;
            }
            if (iequals(args[2], "LIB-NAME")) client.lib_name.assign(args[3]);
            else if (iequals(args[2], "LIB-VER")) client.lib_ver.assign(args[3]);
            else {
                Resp::append_error(out, "unknown option or number of arguments for CLIENT SETINFO");
                return;
            }
            if (client.id != 0) {
                register_client(client);
            }
            Resp::append_ok(out);
            return;
        }

        if (iequals(args[1], "LIST")) {
            if (args.size() != 2) {
                Resp::append_error(out, "wrong number of arguments for 'client|list' command");
                return;
            }
            auto clients = client_snapshot();
            if (client.id != 0 && clients.empty()) clients.push_back(client);
            std::string body;
            for (const auto& item : clients) {
                body += "id=" + std::to_string(item.id);
                body += " addr=127.0.0.1:0 fd=" + std::to_string(item.id);
                body += " name=" + (item.name.empty() ? std::string("") : item.name);
                body += " db=0 flags=N lib-name=" + item.lib_name + " lib-ver=" + item.lib_ver + "\r\n";
            }
            Resp::append_bulk_string(out, body);
            return;
        }

        Resp::append_error(out, "unknown subcommand or wrong number of arguments for 'client'");
    }

    void handle_auth_sv(const std::vector<std::string_view>& args, bool& authenticated, const std::string& server_password, std::string& out) {
        if (args.size() < 2 || args.size() > 3) {
            Resp::append_error(out, "wrong number of arguments for 'auth' command");
            return;
        }

        if (server_password.empty()) {
            authenticated = true;
            Resp::append_ok(out);
            return;
        }

        std::string_view provided_password = (args.size() == 2) ? args[1] : args[2];
        if (provided_password == server_password) {
            authenticated = true;
            Resp::append_ok(out);
            return;
        }

        Resp::append_error(out, "WRONGPASS invalid username-password pair or user is disabled.");
    }

    void append_hello_info_resp2(std::string& out, uint64_t client_id) {
        // Redis represents the HELLO map as a flat array when RESP2 is
        // selected. Keep the fields stable so clients can inspect them.
        Resp::append_array_header(out, 14);
        Resp::append_bulk_string(out, "server");
        Resp::append_bulk_string(out, constants::SERVER_NAME);
        Resp::append_bulk_string(out, "version");
        Resp::append_bulk_string(out, constants::VERSION);
        Resp::append_bulk_string(out, "proto");
        Resp::append_integer(out, 2);
        Resp::append_bulk_string(out, "id");
        Resp::append_integer(out, static_cast<long long>(client_id));
        Resp::append_bulk_string(out, "mode");
        Resp::append_bulk_string(out, "standalone");
        Resp::append_bulk_string(out, "role");
        Resp::append_bulk_string(out, "master");
        Resp::append_bulk_string(out, "modules");
        Resp::append_empty_array(out);
    }

    void append_hello_info_resp3(std::string& out, uint64_t client_id) {
        // RESP3 map: server, version, proto, id, mode, role and modules.
        out += "%7\r\n";
        Resp::append_bulk_string(out, "server");
        Resp::append_bulk_string(out, constants::SERVER_NAME);
        Resp::append_bulk_string(out, "version");
        Resp::append_bulk_string(out, constants::VERSION);
        Resp::append_bulk_string(out, "proto");
        Resp::append_integer(out, 3);
        Resp::append_bulk_string(out, "id");
        Resp::append_integer(out, static_cast<long long>(client_id));
        Resp::append_bulk_string(out, "mode");
        Resp::append_bulk_string(out, "standalone");
        Resp::append_bulk_string(out, "role");
        Resp::append_bulk_string(out, "master");
        Resp::append_bulk_string(out, "modules");
        Resp::append_empty_array(out);
    }

    void handle_hello_sv(const std::vector<std::string_view>& args,
                         bool& authenticated,
                         const std::string& server_password,
                         ClientSession& client,
                         std::string& out) {
        if (args.size() > 1) {
            long long version = 0;
            auto [ptr, ec] = std::from_chars(args[1].data(), args[1].data() + args[1].size(), version);
            if (ec != std::errc() || ptr != args[1].data() + args[1].size() || (version != 2 && version != 3)) {
                Resp::append_error(out, "NOPROTO HELLO supports RESP2 and RESP3");
                return;
            }
            client.protocol = static_cast<int>(version);
        }

        size_t i = 2;
        while (i < args.size()) {
            if (iequals(args[i], "AUTH")) {
                if (i + 2 >= args.size() || !iequals(args[i + 1], "DEFAULT")) {
                    Resp::append_error(out, "syntax error");
                    return;
                }
                if (!server_password.empty() && args[i + 2] != server_password) {
                    Resp::append_error(out, "WRONGPASS invalid username-password pair or user is disabled.");
                    return;
                }
                authenticated = true;
                i += 3;
            } else if (iequals(args[i], "SETNAME")) {
                if (i + 1 >= args.size() || args[i + 1].size() > 512) {
                    Resp::append_error(out, "syntax error");
                    return;
                }
                if (args[i + 1].find_first_of(" \t\r\n") != std::string_view::npos) {
                    Resp::append_error(out, "syntax error");
                    return;
                }
                client.name.assign(args[i + 1]);
                client.name_set = true;
                if (client.id != 0) {
                    register_client(client);
                }
                i += 2;
            } else {
                Resp::append_error(out, "syntax error");
                return;
            }
        }

        if (!server_password.empty() && !authenticated) {
            Resp::append_error(out, "NOAUTH Authentication required.");
            return;
        }

        if (client.protocol == 3) append_hello_info_resp3(out, client.id);
        else append_hello_info_resp2(out, client.id);
    }

    void handle_ping_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() == 1) {
            Resp::append_pong(out);
        } else if (args.size() == 2) {
            Resp::append_bulk_string(out, args[1]);
        } else {
            Resp::append_error(out, "wrong number of arguments for 'ping' command");
        }
    }

    struct SetOptions {
        uint64_t ttl_ms = 0;
        bool has_expiry = false;
        bool keep_ttl = false;
        bool nx = false;
        bool xx = false;
    };

    bool parse_set_options(const std::vector<std::string_view>& args, SetOptions& options, std::string& out) {
        for (size_t i = 3; i < args.size(); ++i) {
            if (iequals(args[i], "NX")) {
                if (options.nx || options.xx) {
                    Resp::append_error(out, "syntax error");
                    return false;
                }
                options.nx = true;
            } else if (iequals(args[i], "XX")) {
                if (options.nx || options.xx) {
                    Resp::append_error(out, "syntax error");
                    return false;
                }
                options.xx = true;
            } else if (iequals(args[i], "KEEPTTL")) {
                if (options.keep_ttl) {
                    Resp::append_error(out, "syntax error");
                    return false;
                }
                options.keep_ttl = true;
            } else if (iequals(args[i], "EX") || iequals(args[i], "PX")) {
                if (options.has_expiry || i + 1 >= args.size()) {
                    Resp::append_error(out, "syntax error");
                    return false;
                }

                std::string_view dur_sv = args[i + 1];
                if (!dur_sv.empty() && dur_sv[0] == '+') {
                    dur_sv.remove_prefix(1);
                }
                long long duration = 0;
                auto [ptr, ec] = std::from_chars(dur_sv.data(),
                                                 dur_sv.data() + dur_sv.size(),
                                                 duration);
                if (ec != std::errc() || ptr != dur_sv.data() + dur_sv.size() || dur_sv.empty()) {
                    Resp::append_error(out, "value is not an integer or out of range");
                    return false;
                }
                if (duration <= 0) {
                    Resp::append_error(out, "invalid expire time in 'set' command");
                    return false;
                }

                uint64_t duration_ms = static_cast<uint64_t>(duration);
                if (iequals(args[i], "EX")) {
                    if (duration_ms > std::numeric_limits<uint64_t>::max() / 1000) {
                        Resp::append_error(out, "value is not an integer or out of range");
                        return false;
                    }
                    duration_ms *= 1000;
                }
                options.ttl_ms = duration_ms;
                options.has_expiry = true;
                ++i;
            } else {
                Resp::append_error(out, "syntax error");
                return false;
            }
        }

        if (options.keep_ttl && options.has_expiry) {
            Resp::append_error(out, "syntax error");
            return false;
        }
        return true;
    }

    bool handle_set_sv(const std::vector<std::string_view>& args, std::string& out, int protocol = 2) {
        if (args.size() < 3) {
            Resp::append_error(out, "wrong number of arguments for 'set' command");
            return false;
        }

        SetOptions options;
        if (!parse_set_options(args, options, out)) {
            return false;
        }
        if (!store_.check_memory_and_evict()) {
            Resp::append_error(out, "OOM command not allowed when used memory > 'maxmemory'.");
            return false;
        }

        auto status = store_.set_with_options(args[1], args[2], options.ttl_ms,
                                              options.keep_ttl, options.nx, options.xx);
        if (status == Store::SetStatus::NotApplied) {
            Resp::append_null_bulk_string(out, protocol);
            return false;
        }
        Resp::append_ok(out);
        return true;
    }

    void handle_get_sv(const std::vector<std::string_view>& args, std::string& out, int protocol = 2) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'get' command");
            return;
        }
        store_.get_and_append(args[1], out, protocol);
    }

    bool handle_del_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() < 2) {
            Resp::append_error(out, "wrong number of arguments for 'del' command");
            return false;
        }
        size_t count = 0;
        if (args.size() == 2) {
            count = store_.del_one(args[1]);
        } else {
            std::vector<std::string_view> keys(args.begin() + 1, args.end());
            count = store_.del(keys);
        }
        Resp::append_integer(out, count);
        return count > 0;
    }

    void handle_exists_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() < 2) {
            Resp::append_error(out, "wrong number of arguments for 'exists' command");
            return;
        }
        if (args.size() == 2) {
            Resp::append_integer(out, store_.exists_one(args[1]));
            return;
        }
        std::vector<std::string_view> keys(args.begin() + 1, args.end());
        Resp::append_integer(out, store_.exists(keys));
    }

    void handle_keys_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'keys' command");
            return;
        }
        auto matched = store_.keys(std::string(args[1]));
        Resp::append_array_header(out, matched.size());
        for (const auto& k : matched) {
            Resp::append_bulk_string(out, k);
        }
    }

    void handle_flushdb_sv(const std::vector<std::string_view>&, std::string& out) {
        store_.flushdb();
        Resp::append_ok(out);
    }

    void handle_dbsize_sv(const std::vector<std::string_view>&, std::string& out) {
        Resp::append_integer(out, store_.size());
    }

    void handle_echo_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'echo' command");
            return;
        }
        Resp::append_bulk_string(out, args[1]);
    }

    void handle_command_sv(const std::vector<std::string_view>&, std::string& out) {
        Resp::append_empty_array(out);
    }

    void handle_select_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'select' command");
            return;
        }
        std::string_view idx_sv = args[1];
        if (!idx_sv.empty() && idx_sv[0] == '+') {
            idx_sv.remove_prefix(1);
        }
        long long index = 0;
        auto [ptr, ec] = std::from_chars(idx_sv.data(), idx_sv.data() + idx_sv.size(), index);
        if (ec != std::errc() || ptr != idx_sv.data() + idx_sv.size() || idx_sv.empty()) {
            Resp::append_error(out, "value is not an integer or out of range");
            return;
        }
        if (index != 0) {
            Resp::append_error(out, "DB index is out of range");
            return;
        }
        Resp::append_ok(out);
    }

    void handle_config_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() < 2) {
            Resp::append_error(out, "wrong number of arguments for 'config' command");
            return;
        }
        if (iequals(args[1], "GET")) {
            if (args.size() != 3) {
                Resp::append_error(out, "wrong number of arguments for 'config|get' command");
                return;
            }
            std::string param(args[2]);
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);
            if (param == "maxmemory") {
                Resp::append_array_header(out, 2);
                Resp::append_bulk_string(out, "maxmemory");
                Resp::append_bulk_string(out, std::to_string(store_.maxmemory()));
            } else if (param == "maxmemory-policy") {
                Resp::append_array_header(out, 2);
                Resp::append_bulk_string(out, "maxmemory-policy");
                Resp::append_bulk_string(out, constants::maxmemory_policy_to_string(store_.maxmemory_policy()));
            } else if (param == "*") {
                Resp::append_array_header(out, 4);
                Resp::append_bulk_string(out, "maxmemory");
                Resp::append_bulk_string(out, std::to_string(store_.maxmemory()));
                Resp::append_bulk_string(out, "maxmemory-policy");
                Resp::append_bulk_string(out, constants::maxmemory_policy_to_string(store_.maxmemory_policy()));
            } else if (param == "save") {
                Resp::append_array_header(out, 2);
                Resp::append_bulk_string(out, "save");
                Resp::append_bulk_string(out, "");
            } else if (param == "appendonly") {
                Resp::append_array_header(out, 2);
                Resp::append_bulk_string(out, "appendonly");
                Resp::append_bulk_string(out, (aof_mgr_ && aof_mgr_->is_enabled()) ? "yes" : "no");
            } else {
                Resp::append_empty_array(out);
            }
        } else if (iequals(args[1], "SET")) {
            if (args.size() != 4) {
                Resp::append_error(out, "wrong number of arguments for 'config|set' command");
                return;
            }
            std::string param(args[2]);
            std::transform(param.begin(), param.end(), param.begin(), ::tolower);
            if (param == "maxmemory") {
                size_t bytes = 0;
                if (!constants::parse_memory_string(std::string(args[3]), bytes)) {
                    Resp::append_error(out, "argument must be an integer or memory string (e.g. 100mb)");
                    return;
                }
                store_.set_maxmemory(bytes);
                Resp::append_ok(out);
            } else if (param == "maxmemory-policy") {
                constants::MaxmemoryPolicy policy;
                if (!constants::parse_maxmemory_policy(std::string(args[3]), policy)) {
                    Resp::append_error(out, "invalid maxmemory policy");
                    return;
                }
                store_.set_maxmemory_policy(policy);
                Resp::append_ok(out);
            } else {
                Resp::append_error(out, "Unsupported CONFIG parameter: " + std::string(args[2]));
            }
        } else if (iequals(args[1], "RESETSTAT")) {
            Resp::append_ok(out);
        } else {
            Resp::append_error(out, "unknown subcommand '" + std::string(args[1]) + "' for 'CONFIG'");
        }
    }

    void handle_info_sv(const std::vector<std::string_view>& args, std::string& out) {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

        bool all = (args.size() <= 1);
        std::string section = all ? "" : std::string(args[1]);
        std::transform(section.begin(), section.end(), section.begin(), ::tolower);

        std::string info;
        if (all || section == "server" || section == "default") {
            info += "# Server\r\n";
            info += "redis_version:" + constants::REDIS_VERSION_STRING + "\r\n";
            info += "kvllay_version:" + std::string(constants::VERSION) + "\r\n";
            info += "uptime_in_seconds:" + std::to_string(uptime) + "\r\n";
        }
        if (all || section == "memory" || section == "default") {
            info += "# Memory\r\n";
            info += "used_memory:" + std::to_string(store_.used_memory()) + "\r\n";
            info += "used_memory_human:" + constants::format_memory_human(store_.used_memory()) + "\r\n";
            size_t rss = allocator::get_rss_bytes();
            info += "used_memory_rss:" + std::to_string(rss) + "\r\n";
            info += "used_memory_rss_human:" + constants::format_memory_human(rss) + "\r\n";
            info += "used_memory_peak:" + std::to_string(store_.used_memory_peak()) + "\r\n";
            info += "used_memory_peak_human:" + constants::format_memory_human(store_.used_memory_peak()) + "\r\n";
            info += "maxmemory:" + std::to_string(store_.maxmemory()) + "\r\n";
            info += "maxmemory_human:" + constants::format_memory_human(store_.maxmemory()) + "\r\n";
            info += "maxmemory_policy:" + constants::maxmemory_policy_to_string(store_.maxmemory_policy()) + "\r\n";
            double frag = allocator::get_fragmentation_ratio(store_.used_memory(), rss);
            char frag_buf[32];
            std::snprintf(frag_buf, sizeof(frag_buf), "%.2f", frag);
            info += "mem_fragmentation_ratio:" + std::string(frag_buf) + "\r\n";
            info += "mem_allocator:" + allocator::get_allocator_name() + "\r\n";
            info += "evicted_keys:" + std::to_string(store_.evicted_keys_count()) + "\r\n";
        }
        if (all || section == "persistence" || section == "default") {
            info += "# Persistence\r\n";
            info += "loading:0\r\n";
            info += "rdb_changes_since_last_save:" + std::to_string(store_.dirty_count()) + "\r\n";
            info += "rdb_bgsave_in_progress:" + std::to_string(snapshot_mgr_ && snapshot_mgr_->is_saving() ? 1 : 0) + "\r\n";
            info += "rdb_last_save_time:" + std::to_string(snapshot_mgr_ ? snapshot_mgr_->last_save_time() : 0) + "\r\n";
            info += "rdb_last_bgsave_status:ok\r\n";
            info += "aof_enabled:" + std::to_string(aof_mgr_ && aof_mgr_->is_enabled() ? 1 : 0) + "\r\n";
            info += "aof_rewrite_in_progress:" + std::to_string(aof_mgr_ && aof_mgr_->is_rewriting() ? 1 : 0) + "\r\n";
        }
        if (all || section == "keyspace" || section == "default") {
            info += "# Keyspace\r\n";
            info += "db0:keys=" + std::to_string(store_.size()) + ",expires=" + std::to_string(store_.expires_size()) + "\r\n";
        }

        Resp::append_bulk_string(out, info);
    }

    void handle_expire_sv(const std::vector<std::string_view>& args, std::string& out,
                          std::vector<std::string>& aof_args, bool& is_mutating) {
        if (args.size() != 3) {
            Resp::append_error(out, "wrong number of arguments for 'expire' command");
            is_mutating = false;
            return;
        }
        long long seconds = 0;
        auto [ptr, ec] = std::from_chars(args[2].data(), args[2].data() + args[2].size(), seconds);
        if (ec != std::errc() || ptr != args[2].data() + args[2].size()) {
            Resp::append_error(out, "value is not an integer or out of range");
            is_mutating = false;
            return;
        }
        uint64_t expire_at = 0;
        uint64_t ttl_ms = seconds > 0 ? static_cast<uint64_t>(seconds) * 1000 : 0;
        int result = store_.expire(std::string(args[1]), ttl_ms, &expire_at);
        Resp::append_integer(out, result);
        if (result == 1) {
            aof_args = {"PEXPIREAT", std::string(args[1]), std::to_string(expire_at)};
        } else {
            is_mutating = false;
        }
    }

    void handle_pexpire_sv(const std::vector<std::string_view>& args, std::string& out,
                           std::vector<std::string>& aof_args, bool& is_mutating) {
        if (args.size() != 3) {
            Resp::append_error(out, "wrong number of arguments for 'pexpire' command");
            is_mutating = false;
            return;
        }
        long long ms = 0;
        auto [ptr, ec] = std::from_chars(args[2].data(), args[2].data() + args[2].size(), ms);
        if (ec != std::errc() || ptr != args[2].data() + args[2].size()) {
            Resp::append_error(out, "value is not an integer or out of range");
            is_mutating = false;
            return;
        }
        uint64_t expire_at = 0;
        uint64_t ttl_ms = ms > 0 ? static_cast<uint64_t>(ms) : 0;
        int result = store_.expire(std::string(args[1]), ttl_ms, &expire_at);
        Resp::append_integer(out, result);
        if (result == 1) {
            aof_args = {"PEXPIREAT", std::string(args[1]), std::to_string(expire_at)};
        } else {
            is_mutating = false;
        }
    }

    void handle_ttl_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'ttl' command");
            return;
        }
        Resp::append_integer(out, store_.ttl(std::string(args[1]), false));
    }

    void handle_pttl_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'pttl' command");
            return;
        }
        Resp::append_integer(out, store_.ttl(std::string(args[1]), true));
    }

    void handle_persist_sv(const std::vector<std::string_view>& args, std::string& out, bool& is_mutating) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'persist' command");
            is_mutating = false;
            return;
        }
        int result = store_.persist(std::string(args[1]));
        Resp::append_integer(out, result);
        if (result == 0) {
            is_mutating = false;
        }
    }

    void handle_setex_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 4) {
            Resp::append_error(out, "wrong number of arguments for 'setex' command");
            return;
        }
        long long seconds = 0;
        auto [ptr, ec] = std::from_chars(args[2].data(), args[2].data() + args[2].size(), seconds);
        if (ec != std::errc() || ptr != args[2].data() + args[2].size()) {
            Resp::append_error(out, "value is not an integer or out of range");
            return;
        }
        if (seconds <= 0) {
            Resp::append_error(out, "invalid expire time in 'setex' command");
            return;
        }
        store_.setex(std::string(args[1]), static_cast<uint64_t>(seconds) * 1000, std::string(args[3]));
        Resp::append_ok(out);
    }

    void format_incr_result_sv(Store::IncrStatus status, int64_t result, std::string& out) {
        if (status == Store::IncrStatus::Success) {
            Resp::append_integer(out, result);
        } else if (status == Store::IncrStatus::WrongType) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
        } else if (status == Store::IncrStatus::NotAnInteger) {
            Resp::append_error(out, "value is not an integer or out of range");
        } else {
            Resp::append_error(out, "increment or decrement would overflow");
        }
    }

    void handle_incr_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'incr' command");
            return;
        }
        int64_t result = 0;
        auto status = store_.incrby(args[1], 1, result);
        format_incr_result_sv(status, result, out);
    }

    void handle_decr_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'decr' command");
            return;
        }
        int64_t result = 0;
        auto status = store_.decrby(args[1], 1, result);
        format_incr_result_sv(status, result, out);
    }

    void handle_incrby_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 3) {
            Resp::append_error(out, "wrong number of arguments for 'incrby' command");
            return;
        }
        int64_t delta = 0;
        auto [ptr, ec] = std::from_chars(args[2].data(), args[2].data() + args[2].size(), delta);
        if (ec != std::errc() || ptr != args[2].data() + args[2].size()) {
            Resp::append_error(out, "value is not an integer or out of range");
            return;
        }
        int64_t result = 0;
        auto status = store_.incrby(args[1], delta, result);
        format_incr_result_sv(status, result, out);
    }

    void handle_decrby_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 3) {
            Resp::append_error(out, "wrong number of arguments for 'decrby' command");
            return;
        }
        int64_t delta = 0;
        auto [ptr, ec] = std::from_chars(args[2].data(), args[2].data() + args[2].size(), delta);
        if (ec != std::errc() || ptr != args[2].data() + args[2].size()) {
            Resp::append_error(out, "value is not an integer or out of range");
            return;
        }
        int64_t result = 0;
        auto status = store_.decrby(args[1], delta, result);
        format_incr_result_sv(status, result, out);
    }

    void handle_mget_sv(const std::vector<std::string_view>& args, std::string& out, int protocol = 2) {
        if (args.size() < 2) {
            Resp::append_error(out, "wrong number of arguments for 'mget' command");
            return;
        }
        std::vector<std::string_view> keys(args.begin() + 1, args.end());
        store_.mget_and_append(keys, out, protocol);
    }

    void handle_mset_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() < 3 || (args.size() - 1) % 2 != 0) {
            Resp::append_error(out, "wrong number of arguments for 'mset' command");
            return;
        }
        std::vector<std::pair<std::string, std::string>> kvs;
        kvs.reserve((args.size() - 1) / 2);
        for (size_t i = 1; i < args.size(); i += 2) {
            kvs.emplace_back(std::string(args[i]), std::string(args[i + 1]));
        }
        store_.mset(kvs);
        Resp::append_ok(out);
    }

    void handle_save_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 1) {
            Resp::append_error(out, "wrong number of arguments for 'save' command");
            return;
        }
        if (!snapshot_mgr_) {
            Resp::append_error(out, "ERR snapshot manager not configured");
            return;
        }
        if (!snapshot_mgr_->save_sync(store_)) {
            Resp::append_error(out, "ERR failed to save snapshot");
            return;
        }
        store_.reset_dirty();
        Resp::append_ok(out);
    }

    void handle_bgsave_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() > 2) {
            Resp::append_error(out, "wrong number of arguments for 'bgsave' command");
            return;
        }
        if (!snapshot_mgr_) {
            Resp::append_error(out, "ERR snapshot manager not configured");
            return;
        }
        if (snapshot_mgr_->is_saving()) {
            Resp::append_error(out, "Background save already in progress");
            return;
        }
        if (!snapshot_mgr_->save_async(store_)) {
            Resp::append_error(out, "ERR failed to start background save");
            return;
        }
        Resp::append_simple_string(out, "Background saving started");
    }

    void handle_lastsave_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 1) {
            Resp::append_error(out, "wrong number of arguments for 'lastsave' command");
            return;
        }
        uint64_t t = snapshot_mgr_ ? snapshot_mgr_->last_save_time() : 0;
        Resp::append_integer(out, static_cast<long long>(t));
    }

    void handle_bgrewriteaof_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 1) {
            Resp::append_error(out, "wrong number of arguments for 'bgrewriteaof' command");
            return;
        }
        if (!aof_mgr_ || !aof_mgr_->is_enabled()) {
            Resp::append_error(out, "Background append only file rewriting not enabled");
            return;
        }
        if (aof_mgr_->is_rewriting()) {
            Resp::append_error(out, "Background append only file rewriting already in progress");
            return;
        }
        if (!aof_mgr_->rewrite_async(store_)) {
            Resp::append_error(out, "ERR failed to start background AOF rewrite");
            return;
        }
        Resp::append_simple_string(out, "Background append only file rewriting started");
    }

    void handle_lpush_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() < 3) {
            Resp::append_error(out, "wrong number of arguments for 'lpush' command");
            return;
        }
        size_t new_len = 0;
        auto status = store_.lpush(args[1], args.begin() + 2, args.end(), new_len);
        if (status == Store::ListPushStatus::WrongType) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
        Resp::append_integer(out, static_cast<long long>(new_len));
    }

    void handle_rpush_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() < 3) {
            Resp::append_error(out, "wrong number of arguments for 'rpush' command");
            return;
        }
        size_t new_len = 0;
        auto status = store_.rpush(args[1], args.begin() + 2, args.end(), new_len);
        if (status == Store::ListPushStatus::WrongType) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
        Resp::append_integer(out, static_cast<long long>(new_len));
    }

    void handle_lpop_sv(const std::vector<std::string_view>& args, std::string& out, int protocol = 2) {
        if (args.size() < 2 || args.size() > 3) {
            Resp::append_error(out, "wrong number of arguments for 'lpop' command");
            return;
        }
        if (args.size() == 2) {
            store_.lpop_one(args[1], out, protocol);
            return;
        }

        int64_t count = 0;
        auto [ptr, ec] = std::from_chars(args[2].data(), args[2].data() + args[2].size(), count);
        if (ec != std::errc() || ptr != args[2].data() + args[2].size() || count < 0) {
            Resp::append_error(out, "value is out of range, must be positive");
            return;
        }
        if (count == 0) {
            size_t len = 0;
            auto lstatus = store_.llen(std::string(args[1]), len);
            if (lstatus == Store::ListLenStatus::WrongType) {
                Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
                return;
            }
            if (len == 0 && store_.exists_one(args[1]) == 0) {
                Resp::append_null_array(out, protocol);
                return;
            }
            Resp::append_empty_array(out);
            return;
        }

        std::vector<std::string> popped;
        auto status = store_.lpop(std::string(args[1]), static_cast<size_t>(count), popped);
        if (status == Store::ListPopStatus::WrongType) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
        if (status == Store::ListPopStatus::NotFound) {
            Resp::append_null_array(out, protocol);
            return;
        }
        Resp::append_array_header(out, popped.size());
        for (const auto& p : popped) {
            Resp::append_bulk_string(out, p);
        }
    }

    void handle_rpop_sv(const std::vector<std::string_view>& args, std::string& out, int protocol = 2) {
        if (args.size() < 2 || args.size() > 3) {
            Resp::append_error(out, "wrong number of arguments for 'rpop' command");
            return;
        }
        if (args.size() == 2) {
            store_.rpop_one(args[1], out, protocol);
            return;
        }

        int64_t count = 0;
        auto [ptr, ec] = std::from_chars(args[2].data(), args[2].data() + args[2].size(), count);
        if (ec != std::errc() || ptr != args[2].data() + args[2].size() || count < 0) {
            Resp::append_error(out, "value is out of range, must be positive");
            return;
        }
        if (count == 0) {
            size_t len = 0;
            auto lstatus = store_.llen(std::string(args[1]), len);
            if (lstatus == Store::ListLenStatus::WrongType) {
                Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
                return;
            }
            if (len == 0 && store_.exists_one(args[1]) == 0) {
                Resp::append_null_array(out, protocol);
                return;
            }
            Resp::append_empty_array(out);
            return;
        }

        std::vector<std::string> popped;
        auto status = store_.rpop(std::string(args[1]), static_cast<size_t>(count), popped);
        if (status == Store::ListPopStatus::WrongType) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
        if (status == Store::ListPopStatus::NotFound) {
            Resp::append_null_array(out, protocol);
            return;
        }
        Resp::append_array_header(out, popped.size());
        for (const auto& p : popped) {
            Resp::append_bulk_string(out, p);
        }
    }

    void handle_llen_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'llen' command");
            return;
        }
        size_t len = 0;
        auto status = store_.llen(std::string(args[1]), len);
        if (status == Store::ListLenStatus::WrongType) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
        Resp::append_integer(out, static_cast<long long>(len));
    }

    void handle_lrange_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 4) {
            Resp::append_error(out, "wrong number of arguments for 'lrange' command");
            return;
        }
        int64_t start = 0;
        int64_t stop = 0;
        auto [ptr1, ec1] = std::from_chars(args[2].data(), args[2].data() + args[2].size(), start);
        auto [ptr2, ec2] = std::from_chars(args[3].data(), args[3].data() + args[3].size(), stop);
        if (ec1 != std::errc() || ptr1 != args[2].data() + args[2].size() ||
            ec2 != std::errc() || ptr2 != args[3].data() + args[3].size()) {
            Resp::append_error(out, "value is not an integer or out of range");
            return;
        }
        std::vector<std::string> elements;
        auto status = store_.lrange(std::string(args[1]), start, stop, elements);
        if (status == Store::ListRangeStatus::WrongType) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
        Resp::append_array_header(out, elements.size());
        for (const auto& elem : elements) {
            Resp::append_bulk_string(out, elem);
        }
    }

    void handle_lindex_sv(const std::vector<std::string_view>& args, std::string& out, int protocol = 2) {
        if (args.size() != 3) {
            Resp::append_error(out, "wrong number of arguments for 'lindex' command");
            return;
        }
        int64_t index = 0;
        auto [ptr, ec] = std::from_chars(args[2].data(), args[2].data() + args[2].size(), index);
        if (ec != std::errc() || ptr != args[2].data() + args[2].size()) {
            Resp::append_error(out, "value is not an integer or out of range");
            return;
        }
        std::string elem;
        auto status = store_.lindex(std::string(args[1]), index, elem);
        if (status == Store::ListRangeStatus::WrongType) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
            return;
        }
        if (status == Store::ListRangeStatus::NotFound) {
            Resp::append_null_bulk_string(out, protocol);
            return;
        }
        Resp::append_bulk_string(out, elem);
    }

    void handle_type_sv(const std::vector<std::string_view>& args, std::string& out) {
        if (args.size() != 2) {
            Resp::append_error(out, "wrong number of arguments for 'type' command");
            return;
        }
        auto t = store_.key_type(std::string(args[1]));
        switch (t) {
            case Store::KeyType::String: Resp::append_simple_string(out, "string"); break;
            case Store::KeyType::List: Resp::append_simple_string(out, "list"); break;
            case Store::KeyType::None:
            default: Resp::append_simple_string(out, "none"); break;
        }
    }
};

}

#endif // KVLLAY_COMMANDS_HPP
