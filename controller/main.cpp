#include "protocol.hpp"
#include <chrono>
#include <iostream>
#include <random>
#include <string>
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
struct Options { std::string host,user,key,known; int port=22,timeout=15; };
bool safe_atom(const std::string& s) {
 if(s.empty()) return false;
 for(unsigned char c:s) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c=='_'||c==':'||c=='['||c==']')) return false;
 return true;
}
bool parse(int argc,char** argv,Options& o) {
 for(int i=1;i<argc;++i) {
  std::string a=argv[i]; if(i+1>=argc) return false; std::string v=argv[++i];
  try {
   if(a=="--host") o.host=v;
   else if(a=="--user") o.user=v;
   else if(a=="--identity") o.key=v;
   else if(a=="--known-hosts") o.known=v;
   else if(a=="--port") o.port=std::stoi(v);
   else if(a=="--timeout") o.timeout=std::stoi(v);
   else return false;
  } catch(...) { return false; }
 }
 return safe_atom(o.host)&&safe_atom(o.user)&&!o.key.empty()&&o.port>=1&&o.port<=65535&&o.timeout>=1&&o.timeout<=300;
}
std::string request_id() {
 static constexpr char h[]="0123456789abcdef"; std::random_device rd; std::string s="req-";
 for(int i=0;i<16;++i) s+=h[rd()&15]; return s;
}
#ifdef _WIN32
std::wstring wide(const std::string& s) {
 int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);
 std::wstring w(n,L'\0'); MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,w.data(),n);
 if(!w.empty()) w.pop_back(); return w;
}
std::wstring quote(const std::wstring& a) {
 std::wstring q=L"\""; unsigned sl=0;
 for(wchar_t c:a) { if(c==L'\\'){++sl;continue;} if(c==L'\"'){q.append(sl*2+1,L'\\');q+=c;sl=0;continue;} q.append(sl,L'\\');sl=0;q+=c; }
 q.append(sl*2,L'\\');q+=L'\"'; return q;
}
bool run_ssh(const std::vector<std::string>& args,int timeout,std::string& out) {
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE}; HANDLE rd=nullptr,wr=nullptr;
 if(!CreatePipe(&rd,&wr,&sa,0)) return false;
 SetHandleInformation(rd,HANDLE_FLAG_INHERIT,0);
 std::wstring cmd=L"ssh"; for(const auto& a:args) cmd+=L" "+quote(wide(a));
 STARTUPINFOW si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES;
 si.hStdOutput=wr; si.hStdError=GetStdHandle(STD_ERROR_HANDLE); si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
 PROCESS_INFORMATION pi{};
 BOOL ok=CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);
 CloseHandle(wr); if(!ok){CloseHandle(rd);return false;}
 DWORD w=WaitForSingleObject(pi.hProcess,static_cast<DWORD>(timeout*1000));
 if(w==WAIT_TIMEOUT){TerminateProcess(pi.hProcess,124);WaitForSingleObject(pi.hProcess,INFINITE);}
 DWORD code=1; GetExitCodeProcess(pi.hProcess,&code); char b[4096]; DWORD n=0;
 while(ReadFile(rd,b,sizeof(b),&n,nullptr)&&n) out.append(b,n);
 CloseHandle(rd); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
 if(w==WAIT_TIMEOUT){std::cerr<<"SSH timed out.\n";return false;} return code==0;
}
#else
bool run_ssh(const std::vector<std::string>& args,int timeout,std::string& out) {
 int p[2]; if(pipe(p)!=0) return false; pid_t pid=fork();
 if(pid<0){close(p[0]);close(p[1]);return false;}
 if(pid==0) {
  dup2(p[1],STDOUT_FILENO); close(p[0]); close(p[1]);
  std::vector<char*> av{const_cast<char*>("ssh")};
  for(const auto& a:args) av.push_back(const_cast<char*>(a.c_str()));
  av.push_back(nullptr); execvp("ssh",av.data()); _exit(127);
 }
 close(p[1]); int flags=fcntl(p[0],F_GETFL,0); if(flags>=0) fcntl(p[0],F_SETFL,flags|O_NONBLOCK);
 auto end=std::chrono::steady_clock::now()+std::chrono::seconds(timeout);
 int status=0; bool done=false; char b[4096];
 while(std::chrono::steady_clock::now()<end) {
  ssize_t n=read(p[0],b,sizeof(b)); if(n>0) out.append(b,static_cast<size_t>(n));
  pid_t w=waitpid(pid,&status,WNOHANG); if(w==pid){done=true;break;}
  if(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR) break;
  if(n<=0) usleep(10000);
 }
 if(!done){kill(pid,SIGKILL);waitpid(pid,&status,0);std::cerr<<"SSH timed out.\n";}
 ssize_t n; while((n=read(p[0],b,sizeof(b)))>0) out.append(b,static_cast<size_t>(n)); close(p[0]);
 return done&&WIFEXITED(status)&&WEXITSTATUS(status)==0;
}
#endif
bool valid_response(const std::string& s,const std::string& id) {
 return s.find("\"protocol_version\":\"1\"")!=std::string::npos&&
 s.find("\"agent_version\":\"0.1.0\"")!=std::string::npos&&
 s.find("\"status\":\"ok\"")!=std::string::npos&&
 s.find("\"operation\":\"ping\"")!=std::string::npos&&
 s.find("\"request_id\":\""+id+"\"")!=std::string::npos;
}
}
int main(int argc,char** argv) {
 Options o;
 if(!parse(argc,argv,o)){
  std::cerr<<"Usage: cisscanner-controller --host HOST --user USER --identity KEYFILE [--port 22] [--known-hosts FILE] [--timeout 15]\n";return 2;
 }
 const auto id=request_id();
 std::vector<std::string> a={"-T","-i",o.key,"-p",std::to_string(o.port),
  "-o","BatchMode=yes","-o","StrictHostKeyChecking=yes","-o","PasswordAuthentication=no",
  "-o","PreferredAuthentications=publickey","-o","ConnectTimeout="+std::to_string(o.timeout)};
 if(!o.known.empty()){a.push_back("-o");a.push_back("UserKnownHostsFile="+o.known);}
 a.push_back(o.user+"@"+o.host);
 a.insert(a.end(),{"cisscanner-agent","--ping","--request-id",id});
 std::string response;
 if(!run_ssh(a,o.timeout+5,response)){
  std::cerr<<"Connection failed. Check network, SSH access, key permissions, host-key trust, and that cisscanner-agent is in the VM account's PATH.\n";return 1;
 }
 if(!valid_response(response,id)){std::cerr<<"SSH connected, but agent response was invalid/incompatible: "<<response<<"\n";return 3;}
 std::cout<<"Connection successful. Protocol "<<cisscanner::kProtocolVersion<<", agent "<<cisscanner::kAgentVersion<<", request "<<id<<".\n";
 return 0;
}
