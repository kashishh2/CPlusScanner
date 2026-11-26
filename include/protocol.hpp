#pragma once
#include <string>
#include <string_view>
namespace cisscanner {
inline constexpr const char* kProtocolVersion = "1";
inline constexpr const char* kAgentVersion = "0.1.0";
inline std::string json_escape(std::string_view s) {
  std::string o;
  for (unsigned char c : s) {
    switch(c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\b': o += "\\b"; break;
      case '\f': o += "\\f"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if(c < 0x20) {
          static constexpr char h[]="0123456789abcdef";
          o += "\\u00"; o += h[c>>4]; o += h[c&15];
        } else o += static_cast<char>(c);
    }
  }
  return o;
}
}
