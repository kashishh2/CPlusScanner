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
 char b[MAX_COMPUTERNAME_LENGTH+1]{}; DWORD n=sizeof(b);
 if(GetComputerNameA(b,&n)) return b;
#else
 char b[256]{}; if(gethostname(b,sizeof(b)-1)==0) return b;
#endif
 return "unknown";
}
bool valid_id(const std::string& s) {
 if(s.empty() || s.size()>64) return false;
 for(unsigned char c:s) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')) return false;
 return true;
}
}
int main(int argc,char* argv[]) {
 std::string op,id;
 for(int i=1;i<argc;++i) {
  std::string a=argv[i];
  if(a=="--ping"||a=="--system-info") { if(!op.empty()){std::cerr<<"Only one operation allowed.\n";return 2;} op=a; }
  else if(a=="--request-id"&&i+1<argc) { id=argv[++i]; if(!valid_id(id)){std::cerr<<"Invalid request ID.\n";return 2;} }
  else {std::cerr<<"Unsupported argument: "<<a<<"\n";return 2;}
 }
 if(op.empty()){std::cerr<<"Usage: cisscanner-agent (--ping|--system-info) [--request-id ID]\n";return 2;}
 std::cout<<"{\"protocol_version\":\""<<cisscanner::kProtocolVersion
          <<"\",\"agent_version\":\""<<cisscanner::kAgentVersion
          <<"\",\"status\":\"ok\",\"operation\":\""
          <<(op=="--ping"?"ping":"system_info")<<"\",\"request_id\":\""
          <<cisscanner::json_escape(id)<<"\"";
 if(op=="--system-info")
  std::cout<<",\"system\":\""<<os_name()<<"\",\"architecture\":\""<<arch_name()
           <<"\",\"hostname\":\""<<cisscanner::json_escape(host_name())<<"\"";
 std::cout<<"}"<<std::endl;
 return 0;
}
