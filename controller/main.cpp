#include "protocol.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
struct Options {
    std::string host;
    std::string user;
    std::string identity_file;
    std::string known_hosts_file;
    int port = 22;
    int timeout_seconds = 15;
};

bool valid_host(const std::string& value) {
    if (value.empty()) return false;
    for (const unsigned char c : value) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-' ||
              c == '_' || c == ':' || c == '[' || c == ']' || c == '%')) {
            return false;
        }
    }
    return true;
}

bool valid_user(const std::string& value) {
    if (value.empty()) return false;
    for (const unsigned char c : value) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' ||
              c == '-' || c == '\\')) {
            return false;
        }
    }
    return true;
}

bool parse_integer(const std::string& value, int& result) {
    if (value.empty()) return false;
    int parsed = 0;
    for (const unsigned char c : value) {
        if (c < '0' || c > '9') return false;
        if (parsed > 1000000) return false;
        parsed = parsed * 10 + (c - '0');
    }
    result = parsed;
    return true;
}

bool parse_arguments(int argc, char** argv, Options& options) {
    bool host_seen = false;
    bool user_seen = false;
    bool identity_seen = false;
    bool port_seen = false;
    bool known_hosts_seen = false;
    bool timeout_seen = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (i + 1 >= argc) return false;
        const std::string value = argv[++i];

        if (argument == "--host" && !host_seen) {
            host_seen = true;
            options.host = value;
        } else if (argument == "--user" && !user_seen) {
            user_seen = true;
            options.user = value;
        } else if (argument == "--identity" && !identity_seen) {
            identity_seen = true;
            options.identity_file = value;
        } else if (argument == "--known-hosts" && !known_hosts_seen) {
            known_hosts_seen = true;
            options.known_hosts_file = value;
        } else if (argument == "--port" && !port_seen) {
            port_seen = true;
            if (!parse_integer(value, options.port)) return false;
        } else if (argument == "--timeout" && !timeout_seen) {
            timeout_seen = true;
            if (!parse_integer(value, options.timeout_seconds)) return false;
        } else {
            return false;
        }
    }

    return host_seen && user_seen && identity_seen &&
           valid_host(options.host) && valid_user(options.user) &&
           !options.identity_file.empty() &&
           (!known_hosts_seen || !options.known_hosts_file.empty()) &&
           options.port >= 1 && options.port <= 65535 &&
           options.timeout_seconds >= 1 && options.timeout_seconds <= 300;
}

std::string make_request_id() {
    static constexpr char hex[] = "0123456789abcdef";
    std::random_device random;
    std::string value = "req-";
    for (int i = 0; i < 16; ++i) {
        value += hex[random() & 0x0f];
    }
    return value;
}

#ifdef _WIN32
std::wstring utf8_to_wide(const std::string& value) {
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           value.c_str(), -1, nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1,
                            result.data(), length) <= 0) {
        return {};
    }
    result.pop_back();
    return result;
}

std::wstring quote_windows_argument(const std::wstring& argument) {
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted += character;
            backslashes = 0;
        } else {
            quoted.append(backslashes, L'\\');
            backslashes = 0;
            quoted += character;
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted += L'"';
    return quoted;
}

bool run_ssh(const std::vector<std::string>& arguments, int timeout_seconds,
             std::string& output) {
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;

    if (!CreatePipe(&read_pipe, &write_pipe, &security, 0)) return false;
    if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return false;
    }

    std::wstring command = L"ssh";
    for (const auto& argument : arguments) {
        const std::wstring wide_argument = utf8_to_wide(argument);
        if (wide_argument.empty()) {
            CloseHandle(read_pipe);
            CloseHandle(write_pipe);
            return false;
        }
        command += L" " + quote_windows_argument(wide_argument);
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write_pipe;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(
        nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, nullptr, &startup, &process);
    CloseHandle(write_pipe);

    if (!created) {
        CloseHandle(read_pipe);
        return false;
    }

    constexpr std::size_t kMaxResponseBytes = 65536;
    bool output_too_large = false;
    std::thread reader([&]() {
        char buffer[4096];
        DWORD bytes_read = 0;
        while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) &&
               bytes_read > 0) {
            if (output.size() + bytes_read <= kMaxResponseBytes) {
                output.append(buffer, bytes_read);
            } else {
                output_too_large = true;
            }
        }
    });

    const DWORD wait_result = WaitForSingleObject(
        process.hProcess, static_cast<DWORD>(timeout_seconds * 1000));
    if (wait_result != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 124);
        WaitForSingleObject(process.hProcess, INFINITE);
    }

    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    reader.join();
    CloseHandle(read_pipe);

    if (wait_result == WAIT_TIMEOUT) {
        std::cerr << "SSH timed out.\n";
        return false;
    }
    return wait_result == WAIT_OBJECT_0 && exit_code == 0 && !output_too_large;
}
#else
bool run_ssh(const std::vector<std::string>& arguments, int timeout_seconds,
             std::string& output) {
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) return false;

    const pid_t child = fork();
    if (child < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return false;
    }

    if (child == 0) {
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) _exit(126);
        close(pipe_fds[0]);
        close(pipe_fds[1]);

        std::vector<char*> child_arguments;
        child_arguments.push_back(const_cast<char*>("ssh"));
        for (const auto& argument : arguments) {
            child_arguments.push_back(const_cast<char*>(argument.c_str()));
        }
        child_arguments.push_back(nullptr);
        execvp("ssh", child_arguments.data());
        _exit(127);
    }

    close(pipe_fds[1]);
    const int flags = fcntl(pipe_fds[0], F_GETFL, 0);
    if (flags < 0 || fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        close(pipe_fds[0]);
        kill(child, SIGKILL);
        waitpid(child, nullptr, 0);
        return false;
    }

    constexpr std::size_t kMaxResponseBytes = 65536;
    bool output_too_large = false;
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(timeout_seconds);
    int child_status = 0;
    bool child_finished = false;
    char buffer[4096];

    while (std::chrono::steady_clock::now() < deadline) {
        const ssize_t bytes_read = read(pipe_fds[0], buffer, sizeof(buffer));
        if (bytes_read > 0) {
            if (output.size() + static_cast<std::size_t>(bytes_read) <= kMaxResponseBytes) {
                output.append(buffer, static_cast<std::size_t>(bytes_read));
            } else {
                output_too_large = true;
            }
        }

        const pid_t waited = waitpid(child, &child_status, WNOHANG);
        if (waited == child) {
            child_finished = true;
            break;
        }
        if (waited < 0 && errno != EINTR) break;
        if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
            errno != EINTR) {
            break;
        }
        if (bytes_read <= 0) usleep(10000);
    }

    if (!child_finished) {
        kill(child, SIGKILL);
        waitpid(child, &child_status, 0);
        std::cerr << "SSH timed out or could not be monitored.\n";
    }

    ssize_t bytes_read = 0;
    while ((bytes_read = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {
        if (output.size() + static_cast<std::size_t>(bytes_read) <= kMaxResponseBytes) {
            output.append(buffer, static_cast<std::size_t>(bytes_read));
        } else {
            output_too_large = true;
        }
    }
    close(pipe_fds[0]);

    return child_finished && WIFEXITED(child_status) &&
           WEXITSTATUS(child_status) == 0 && !output_too_large;
}
#endif

bool valid_ping_response(const std::string& response,
                         const std::string& request_id) {
    const std::string expected =
        "{\"protocol_version\":\"" + cisscanner::kProtocolVersion +
        "\",\"agent_version\":\"" + cisscanner::kAgentVersion +
        "\",\"status\":\"ok\",\"operation\":\"ping\","
        "\"request_id\":\"" + request_id + "\"}";
    return response == expected + "\n" || response == expected + "\r\n";
}
} // namespace

int run_controller(int argc, char** argv) {
    Options options;
    if (!parse_arguments(argc, argv, options)) {
        std::cerr
            << "Usage: cisscanner-controller --host HOST --user USER "
               "--identity KEYFILE [--port 22] [--known-hosts FILE] "
               "[--timeout 15]\n";
        return 2;
    }

    const std::string request_id = make_request_id();
    std::vector<std::string> ssh_arguments = {
        "-T",
        "-i", options.identity_file,
        "-p", std::to_string(options.port),
        "-o", "BatchMode=yes",
        "-o", "IdentitiesOnly=yes",
        "-o", "StrictHostKeyChecking=yes",
        "-o", "PasswordAuthentication=no",
        "-o", "KbdInteractiveAuthentication=no",
        "-o", "PreferredAuthentications=publickey",
        "-o", "ConnectTimeout=" + std::to_string(options.timeout_seconds),
    };

    if (!options.known_hosts_file.empty()) {
        ssh_arguments.push_back("-o");
        ssh_arguments.push_back("UserKnownHostsFile=" + options.known_hosts_file);
    }

    ssh_arguments.push_back(options.user + "@" + options.host);
    ssh_arguments.insert(ssh_arguments.end(),
                         {"cisscanner-agent", "--ping", "--request-id", request_id});

    std::string response;
    if (!run_ssh(ssh_arguments, options.timeout_seconds + 5, response)) {
        std::cerr
            << "Connection failed. Check network access, SSH server status, "
               "key permissions, host-key trust, and that cisscanner-agent "
               "is executable in the remote account's PATH.\n";
        return 1;
    }

    if (!valid_ping_response(response, request_id)) {
        std::cerr
            << "SSH completed, but the agent response did not exactly match "
               "the expected protocol-v1 ping response.\n";
        return 3;
    }

    std::cout << "Connection successful. Protocol "
              << cisscanner::kProtocolVersion << ", agent "
              << cisscanner::kAgentVersion << ", request "
              << request_id << ".\n";
    return 0;
}

#ifdef _WIN32
namespace {
std::string wide_to_utf8(const wchar_t* value) {
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                           value, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                            result.data(), length, nullptr, nullptr) <= 0) {
        return {};
    }
    result.pop_back();
    return result;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> utf8_arguments;
    std::vector<char*> argument_pointers;
    utf8_arguments.reserve(static_cast<std::size_t>(argc));
    argument_pointers.reserve(static_cast<std::size_t>(argc));

    for (int i = 0; i < argc; ++i) {
        std::string converted = wide_to_utf8(argv[i]);
        if (converted.empty()) {
            std::cerr << "Could not convert a command-line argument to UTF-8.\n";
            return 2;
        }
        utf8_arguments.push_back(std::move(converted));
    }
    for (auto& argument : utf8_arguments) {
        argument_pointers.push_back(argument.data());
    }
    return run_controller(argc, argument_pointers.data());
}
#else
int main(int argc, char** argv) {
    return run_controller(argc, argv);
}
#endif
