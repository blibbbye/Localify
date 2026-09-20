#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <winhttp.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"gdiplus.lib")
#pragma comment(lib,"dwmapi.lib")
using namespace Gdiplus;

static const int PORT=4219;
static const char* CLIENT_ID="1550620411740561568";
struct Presence{bool playing=false;std::string song,artist,cover;double pos=0,dur=0;uint64_t version=0;};
struct Cmd{uint64_t seq=0;std::string action;double value=0;};
static HWND H=nullptr;static ULONG_PTR GP=0;static std::mutex M;
static Presence P;static Cmd C;static std::unique_ptr<Bitmap> Cover;static std::string CoverUrl;
static uint64_t CoverToken=0;static std::atomic<bool> Run{true},Server{false},Discord{false};static HANDLE Pipe=INVALID_HANDLE_VALUE;

static std::wstring W(const std::string&s){if(s.empty())return L"";int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),0,0);if(n<=0)return L"";std::wstring r(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n);return r;}
static std::string esc(const std::string&s){std::string r;for(unsigned char c:s){switch(c){case '"':r+="\\\"";break;case '\\':r+="\\\\";break;case '\n':r+="\\n";break;case '\r':r+="\\r";break;default:r.push_back((char)c);}}return r;}
static std::string js(const std::string&j,const char*k){
 std::string q="\"";q+=k;q+="\"";size_t p=j.find(q);if(p==std::string::npos)return"";p=j.find(':',p+q.size());if(p==std::string::npos)return"";++p;while(p<j.size()&&isspace((unsigned char)j[p]))++p;if(p>=j.size()||j[p]!='"')return"";++p;
 std::string r;bool e=false;while(p<j.size()){char c=j[p++];if(e){e=false;if(c=='n')r+='\n';else r+=c;}else if(c=='\\')e=true;else if(c=='"')break;else r+=c;}return r;}
static double jn(const std::string&j,const char*k){std::string q="\"";q+=k;q+="\"";size_t p=j.find(q);if(p==std::string::npos)return 0;p=j.find(':',p+q.size());if(p==std::string::npos)return 0;return atof(j.c_str()+p+1);}
static bool jb(const std::string&j,const char*k,bool d){std::string q="\"";q+=k;q+="\"";size_t p=j.find(q);if(p==std::string::npos)return d;p=j.find(':',p+q.size());if(p==std::string::npos)return d;return j.compare(p+1,4,"true")==0;}

static void reply(SOCKET s,const char*st,const char*ct,const std::string&b){
 std::string h="HTTP/1.1 "+std::string(st)+"\r\nContent-Type: "+ct+"\r\nContent-Length: "+std::to_string(b.size())+"\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET,POST,OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nConnection: close\r\n\r\n";
 send(s,h.data(),(int)h.size(),0);if(!b.empty())send(s,b.data(),(int)b.size(),0);
}
static std::string req(SOCKET s){
 std::string d;char b[8192];for(int i=0;i<16;i++){int n=recv(s,b,sizeof(b),0);if(n<=0)break;d.append(b,n);if(d.find("\r\n\r\n")!=std::string::npos)break;}return d;
}
static void coverLoad(std::string url,uint64_t token){
 std::thread([url,token]{
  if(url.empty())return;
  HINTERNET se=WinHttpOpen(L"LocalifyDesktopPlayer/3",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,0,0,0);if(!se)return;
  URL_COMPONENTS u{};u.dwStructSize=sizeof(u);wchar_t host[256]{},path[4096]{};u.lpszHostName=host;u.dwHostNameLength=256;u.lpszUrlPath=path;u.dwUrlPathLength=4096;
  std::wstring wu=W(url);std::vector<BYTE> bytes;HINTERNET co=nullptr,re=nullptr;
  if(WinHttpCrackUrl(wu.c_str(),0,0,&u)){co=WinHttpConnect(se,host,u.nPort,0);if(co)re=WinHttpOpenRequest(co,L"GET",path,0,0,0,(u.nScheme==INTERNET_SCHEME_HTTPS)?WINHTTP_FLAG_SECURE:0);}
  if(re&&WinHttpSendRequest(re,0,0,0,0,0,0)&&WinHttpReceiveResponse(re,0)){
   for(;;){DWORD n=0;if(!WinHttpQueryDataAvailable(re,&n)||!n)break;size_t z=bytes.size();bytes.resize(z+n);DWORD got=0;if(!WinHttpReadData(re,bytes.data()+z,n,&got)){bytes.resize(z);break;}bytes.resize(z+got);if(bytes.size()>8*1024*1024){bytes.clear();break;}}
  }
  std::unique_ptr<Bitmap> b;
  if(!bytes.empty()){HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,bytes.size());if(h){void*p=GlobalLock(h);memcpy(p,bytes.data(),bytes.size());GlobalUnlock(h);IStream*st=nullptr;if(SUCCEEDED(CreateStreamOnHGlobal(h,TRUE,&st))){Bitmap*x=Bitmap::FromStream(st,FALSE);if(x&&x->GetLastStatus()==Ok)b.reset(x);else delete x;st->Release();}else GlobalFree(h);}}
  if(re)WinHttpCloseHandle(re);if(co)WinHttpCloseHandle(co);WinHttpCloseHandle(se);
  if(b){std::lock_guard<std::mutex>l(M);if(token==CoverToken)Cover=std::move(b);}
  InvalidateRect(H,nullptr,FALSE);
 }).detach();
}
static void issue(const char*a,double v=0){std::lock_guard<std::mutex>l(M);++C.seq;C.action=a;C.value=v;}
static void http(SOCKET s){
 std::string r=req(s);size_t e=r.find("\r\n");if(e==std::string::npos){closesocket(s);return;}std::string line=r.substr(0,e);size_t a=line.find(' '),b=line.find(' ',a+1);if(a==std::string::npos||b==std::string::npos){closesocket(s);return;}std::string m=line.substr(0,a),t=line.substr(a+1,b-a-1);
 if(m=="OPTIONS"){reply(s,"204 No Content","text/plain","");closesocket(s);return;}
 if(m=="GET"&&t.rfind("/health",0)==0){reply(s,"200 OK","application/json","{\"ok\":true,\"desktopPlayer\":true,\"discordConnected\":true,\"port\":4219,\"version\":\"3.0\"}");closesocket(s);return;}
 if(m=="GET"&&t.rfind("/control",0)==0){uint64_t q=0;size_t p=t.find("since=");if(p!=std::string::npos)q=_strtoui64(t.c_str()+p+6,0,10);std::lock_guard<std::mutex>l(M);std::string z=C.seq>q?"{\"seq\":"+std::to_string(C.seq)+",\"action\":\""+esc(C.action)+"\",\"value\":"+std::to_string(C.value)+"}":"{\"seq\":"+std::to_string(q)+"}";reply(s,"200 OK","application/json",z);closesocket(s);return;}
 if(m=="POST"&&t=="/presence"){size_t p=r.find("\r\n\r\n");if(p==std::string::npos){reply(s,"400 Bad Request","text/plain","bad");closesocket(s);return;}std::string body=r.substr(p+4);Presence n;n.playing=jb(body,"playing",true);n.song=js(body,"song");n.artist=js(body,"artist");n.cover=js(body,"coverUrl");n.pos=std::max(0.0,jn(body,"currentTime"));n.dur=std::max(0.0,jn(body,"duration"));
  bool nc=false;uint64_t tok=0;{std::lock_guard<std::mutex>l(M);n.version=P.version+1;P=n;if(CoverUrl!=n.cover){CoverUrl=n.cover;++CoverToken;tok=CoverToken;nc=true;}}
  if(nc){std::lock_guard<std::mutex>l(M);Cover.reset();InvalidateRect(H,nullptr,FALSE);if(!n.cover.empty())coverLoad(n.cover,tok);}
  else InvalidateRect(H,nullptr,FALSE);
  reply(s,"204 No Content","text/plain","");closesocket(s);return;}
 reply(s,"404 Not Found","text/plain","");closesocket(s);
}
static void server(){
 SOCKET l=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(l==INVALID_SOCKET)return;sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(PORT);int yes=1;setsockopt(l,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(yes));if(bind(l,(sockaddr*)&a,sizeof(a))==SOCKET_ERROR||listen(l,8)==SOCKET_ERROR){closesocket(l);return;}Server=true;InvalidateRect(H,0,FALSE);while(Run){SOCKET s=accept(l,0,0);if(s!=INVALID_SOCKET)std::thread(http,s).detach();}closesocket(l);
}
static std::string pkt(uint32_t op,const std::string&j){std::string x(8+j.size(),'\0');uint32_t n=(uint32_t)j.size();memcpy(x.data(),&op,4);memcpy(x.data()+4,&n,4);memcpy(x.data()+8,j.data(),j.size());return x;}
static bool pw(uint32_t op,const std::string&j){if(Pipe==INVALID_HANDLE_VALUE)return false;std::string x=pkt(op,j);DWORD w=0;if(!WriteFile(Pipe,x.data(),(DWORD)x.size(),&w,0)||w!=x.size()){CloseHandle(Pipe);Pipe=INVALID_HANDLE_VALUE;return false;}return true;}
static bool connectDiscord(){
 if(Pipe!=INVALID_HANDLE_VALUE)return true;
 for(int i=0;i<10&&Run;i++){wchar_t n[64];swprintf(n,64,L"\\\\.\\pipe\\discord-ipc-%d",i);HANDLE h=CreateFileW(n,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)continue;Pipe=h;std::string hello="{\"v\":1,\"client_id\":\""+std::string(CLIENT_ID)+"\"}";if(pw(0,hello)){Discord=true;InvalidateRect(H,0,FALSE);return true;}CloseHandle(Pipe);Pipe=INVALID_HANDLE_VALUE;}return false;
}
static void disconnectDiscord(){if(Pipe!=INVALID_HANDLE_VALUE){CloseHandle(Pipe);Pipe=INVALID_HANDLE_VALUE;}}
static void activity(const Presence&p){
 if(!connectDiscord())return;long long start=(long long)(GetTickCount64()/1000ULL-p.pos),end=p.dur>0?start+(long long)p.dur:0;
 std::string q="{\"type\":2,\"name\":\"Localify\",\"details\":\""+esc(p.song.empty()?"Unknown song":p.song)+"\",\"state\":\""+esc(p.artist.empty()?"Unknown Artist":p.artist)+"\",\"status_display_type\":1,\"instance\":false";
 if(end>0)q+=",\"timestamps\":{\"start\":"+std::to_string(start*1000)+",\"end\":"+std::to_string(end*1000)+"}";
 q+="}";std::string z="{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":"+std::to_string(GetCurrentProcessId())+",\"activity\":"+q+"},\"nonce\":\""+std::to_string(GetTickCount64())+"\"}";
 if(!pw(1,z)){disconnectDiscord();}
}
static void rpcLoop(){uint64_t sent=0;bool active=false;while(Run){Presence p;{std::lock_guard<std::mutex>l(M);p=P;}if(p.playing){if(p.version!=sent){activity(p);sent=p.version;}active=true;}else if(active){if(connectDiscord())pw(1,"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":"+std::to_string(GetCurrentProcessId())+",\"activity\":null},\"nonce\":\"clear\"}");active=false;sent=p.version;}else connectDiscord();Sleep(1000);}}
static void fill(Graphics&g,float x,float y,float w,float h,float r,const Color&c){GraphicsPath p;p.AddArc(x,y,r,r,180,90);p.AddArc(x+w-r,y,r,r,270,90);p.AddArc(x+w-r,y+h-r,r,r,0,90);p.AddArc(x,y+h-r,r,r,90,90);p.CloseFigure();SolidBrush b(c);g.FillPath(&b,&p);}
static void txt(Graphics&g,const wchar_t*s,float x,float y,float w,float h,float size,const Color&c,bool bold=false){FontFamily ff(L"Segoe UI");Font f(&ff,size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush b(c);StringFormat sf;sf.SetFormatFlags(StringFormatFlagsNoWrap);sf.SetTrimming(StringTrimmingEllipsisCharacter);g.DrawString(s,-1,&f,RectF(x,y,w,h),&sf,&b);}
static void center(Graphics&g,const wchar_t*s,float x,float y,float w,float h,float size,const Color&c,bool bold=false){FontFamily ff(L"Segoe UI");Font f(&ff,size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush b(c);StringFormat sf;sf.SetAlignment(StringAlignmentCenter);sf.SetLineAlignment(StringAlignmentCenter);g.DrawString(s,-1,&f,RectF(x,y,w,h),&sf,&b);}
static RECT closeR(){return{430,8,468,38};}static RECT minR(){return{400,8,430,38};}
static void paint(HDC out){
 RECT rr;GetClientRect(H,&rr);int w=rr.right,h=rr.bottom;HDC m=CreateCompatibleDC(out);HBITMAP bm=CreateCompatibleBitmap(out,w,h);HGDIOBJ old=SelectObject(m,bm);Graphics g(m);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
 SolidBrush bg(Color(255,10,12,16));g.FillRectangle(&bg,0,0,w,h);fill(g,8,8,w-16,h-16,22,Color(255,18,21,28));
 SolidBrush glow(Color(18,125,211,252));g.FillEllipse(&glow,-90,-100,350,180);txt(g,L"LOCALIFY",24,18,110,22,12,Color(255,125,211,252),true);center(g,L"—",398,8,30,32,17,Color(255,120,130,145));center(g,L"×",430,7,34,33,16,Color(255,120,130,145));
 Presence p;{std::lock_guard<std::mutex>l(M);p=P;}
 fill(g,22,52,92,92,18,Color(255,30,34,42));{
  std::lock_guard<std::mutex>l(M);if(Cover){g.DrawImage(Cover.get(),RectF(22,52,92,92));}else center(g,L"♪",22,52,92,92,36,Color(255,125,211,252),true);
 }
 std::wstring s=W(p.song.empty()?(p.playing?"Unknown song":"Nothing playing"):p.song),a=W(p.artist.empty()?"Unknown Artist":p.artist);txt(g,s.c_str(),130,54,320,30,19,Color(255,247,248,250),true);txt(g,a.c_str(),130,84,320,23,12,Color(255,157,165,179));txt(g,p.playing?L"PLAYING":L"PAUSED",130,112,90,18,9,p.playing?Color(255,125,211,252):Color(255,145,153,168),true);
 double ratio=p.dur>0?std::clamp(p.pos/p.dur,0.0,1.0):0;fill(g,130,142,310,6,3,Color(255,43,48,58));if(ratio>0)fill(g,130,142,(float)(310*ratio),6,3,Color(255,125,211,252));
 center(g,L"PREV",206,168,65,34,9,Color(255,200,207,217),true);fill(g,276,164,58,44,14,Color(255,125,211,252));center(g,p.playing?L"Ⅱ":L"▶",276,164,58,44,18,Color(255,7,16,24),true);center(g,L"NEXT",339,168,65,34,9,Color(255,200,207,217),true);
 BitBlt(out,0,0,w,h,m,0,0,SRCCOPY);SelectObject(m,old);DeleteObject(bm);DeleteDC(m);
}
static LRESULT CALLBACK wnd(HWND h,UINT msg,WPARAM w,LPARAM l){
 switch(msg){
  case WM_ERASEBKGND:return 1;
  case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);paint(dc);EndPaint(h,&ps);return 0;}
  case WM_LBUTTONDOWN:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};if(p.x>=430&&p.y<40){DestroyWindow(h);return 0;}if(p.x>=400&&p.x<430&&p.y<40){ShowWindow(h,SW_MINIMIZE);return 0;}if(p.y>=164&&p.y<208){if(p.x>=276&&p.x<334)issue("toggle");else if(p.x>=200&&p.x<270)issue("prev");else if(p.x>=334&&p.x<404)issue("next");}if(p.x>=130&&p.x<=440&&p.y>=132&&p.y<=160){double d;{std::lock_guard<std::mutex>l2(M);d=P.dur;}if(d>0)issue("seek",std::clamp((p.x-130)/310.0,0.0,1.0)*d);}return 0;}
  case WM_NCHITTEST:{POINT p;GetCursorPos(&p);ScreenToClient(h,&p);if(p.x>=400&&p.y<40)return HTCLIENT;return HTCAPTION;}
  case WM_TIMER:InvalidateRect(h,nullptr,FALSE);return 0;
  case WM_DESTROY:Run=false;PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int show){
 GdiplusStartupInput gi;if(GdiplusStartup(&GP,&gi,0)!=Ok)return 1;WSADATA wd;WSAStartup(MAKEWORD(2,2),&wd);
 WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=hi;wc.lpfnWndProc=wnd;wc.lpszClassName=L"LocalifyPlayer3";wc.hCursor=LoadCursor(0,IDC_ARROW);RegisterClassExW(&wc);
 H=CreateWindowExW(WS_EX_APPWINDOW,L"LocalifyPlayer3",L"Localify Desktop Player",WS_POPUP,CW_USEDEFAULT,CW_USEDEFAULT,480,220,0,0,hi,0);if(!H)return 1;
 SetWindowRgn(H,CreateRoundRectRgn(0,0,480,220,28,28),TRUE);BOOL dark=TRUE;DwmSetWindowAttribute(H,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
 RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);SetWindowPos(H,HWND_TOP,work.right-505,work.bottom-260,480,220,SWP_SHOWWINDOW);ShowWindow(H,show);UpdateWindow(H);SetTimer(H,1,500,0);
 std::thread(server).detach();std::thread(rpcLoop).detach();MSG msg;while(GetMessageW(&msg,0,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}Run=false;disconnectDiscord();WSACleanup();GdiplusShutdown(GP);return 0;
}