#include "protocol.hpp"
#include <iostream>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
std::string os_name() {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#else
    return "Unknown";
#endif
}

std::string arch_name() {
#if defined(_M_X64) || defined(__x86_64__)
    return "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#else
    return "unknown";
#endif
}

std::string host_name() {
#ifdef _WIN32
    wchar_t wide_name[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD length = static_cast<DWORD>(std::size(wide_name));
    if (!GetComputerNameW(wide_name, &length)) {
        return "unknown";
    }

    const int bytes = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, wide_name, static_cast<int>(length),
        nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return "unknown";
    }

    std::string utf8(static_cast<std::size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_name,
                            static_cast<int>(length), utf8.data(), bytes,
                            nullptr, nullptr) <= 0) {
        return "unknown";
    }
    return utf8;
#else
    char name[256]{};
    if (gethostname(name, sizeof(name) - 1) == 0) {
        return name;
    }
    return "unknown";
#endif
}

bool valid_id(const std::string& value) {
    if (value.empty() || value.size() > 64) {
        return false;
    }
    for (const unsigned char c : value) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}
} // namespace

int main(int argc, char* argv[]) {
    std::string operation;
    std::string request_id;
    bool request_id_seen = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--ping" || argument == "--system-info") {
            if (!operation.empty()) {
                std::cerr << "Specify exactly one operation.\n";
                return 2;
            }
            operation = argument;
        } else if (argument == "--request-id") {
            if (request_id_seen || i + 1 >= argc) {
                std::cerr << "--request-id must be supplied once with a value.\n";
                return 2;
            }
            request_id_seen = true;
            request_id = argv[++i];
            if (!valid_id(request_id)) {
                std::cerr << "Invalid request ID.\n";
                return 2;
            }
        } else {
            std::cerr << "Unsupported argument: " << argument << '\n';
            return 2;
        }
    }

    if (operation.empty()) {
        std::cerr << "Usage: cisscanner-agent (--ping|--system-info) [--request-id ID]\n";
        return 2;
    }

    std::cout << "{\"protocol_version\":\"" << cisscanner::kProtocolVersion
              << "\",\"agent_version\":\"" << cisscanner::kAgentVersion
              << "\",\"status\":\"ok\",\"operation\":\""
              << (operation == "--ping" ? "ping" : "system_info")
              << "\",\"request_id\":\""
              << cisscanner::json_escape(request_id) << "\"";

    if (operation == "--system-info") {
        std::cout << ",\"system\":\"" << os_name()
                  << "\",\"architecture\":\"" << arch_name()
                  << "\",\"hostname\":\""
                  << cisscanner::json_escape(host_name()) << "\"";
    }

    std::cout << "}\n";
    return 0;
}
