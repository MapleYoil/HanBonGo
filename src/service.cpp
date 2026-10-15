#include <windows.h>
#include <msctf.h>
#include <ctffunc.h>
#include <ctfutb.h>
#include <olectl.h>
#include <string>
#include <vector>
#include <algorithm>
#include "core.h"
#include "identity.h"
#include "live_candidates.h"

// Stable identities shared by the x86 and x64 builds.
static const wchar_t* kName=L"한본어 입력기";
// Candidate conversion keeps this registered HanBonGo profile active.
static HINSTANCE module;
static LONG objects=0;
// Opt-in activation diagnostics for a dedicated test host. Ordinary editor
// processes neither write logs nor expose their composition text.
static void TraceActivation(const wchar_t* stage,HRESULT result){
 wchar_t path[32768];DWORD length=GetEnvironmentVariableW(L"HANBONGO_ACTIVATION_LOG",path,32768);
 if(!length){DWORD bytes=sizeof(path);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\HanBonGo",L"ActivationLog",RRF_RT_REG_SZ,nullptr,path,&bytes)!=ERROR_SUCCESS)return;length=static_cast<DWORD>(wcslen(path));}
 if(!length||length>=32768)return;
 wchar_t line[256];int count=swprintf_s(line,L"pid=%lu %s: 0x%08lX\r\n",GetCurrentProcessId(),stage,static_cast<ULONG>(result));
 HANDLE file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(file,line,count*sizeof(wchar_t),&written,nullptr);CloseHandle(file);}
}
template<class T> static void SafeRelease(T*& p){if(p){p->Release();p=nullptr;}}
static bool IsHanBonGoActive(){
 ITfInputProcessorProfileMgr* profiles=nullptr;TF_INPUTPROCESSORPROFILE profile{};
 HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(&profiles));
 if(SUCCEEDED(hr)){hr=profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&profile);profiles->Release();}
 return SUCCEEDED(hr)&&profile.dwProfileType==TF_PROFILETYPE_INPUTPROCESSOR&&profile.clsid==kService&&profile.guidProfile==kProfile;
}
// The profile icon labels the switcher; this item supplies the live taskbar icon.
class InputModeItem final:public ITfLangBarItemButton,public ITfSource {
 LONG refs=1;ITfLangBarItemSink* sink=nullptr;HWND notifications=nullptr;bool active=true;
 static LRESULT CALLBACK NotifyProc(HWND window,UINT message,WPARAM w,LPARAM l){
  auto self=reinterpret_cast<InputModeItem*>(GetWindowLongPtrW(window,GWLP_USERDATA));
  if(message==WM_NCCREATE){self=static_cast<InputModeItem*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
  if(message==WM_APP+1&&self){self->AddRef();if(self->sink&&self->active)self->sink->OnUpdate(TF_LBI_ICON|TF_LBI_TEXT|TF_LBI_TOOLTIP|TF_LBI_STATUS);self->Release();return 0;}
  return DefWindowProcW(window,message,w,l);
 }
public:
 InputModeItem(){InterlockedIncrement(&objects);}
 ~InputModeItem(){if(notifications)DestroyWindow(notifications);std::wstring name=std::wstring(L"HanBonGo.IconNotifications.")+kSourceVersion;UnregisterClassW(name.c_str(),module);SafeRelease(sink);InterlockedDecrement(&objects);}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p) override{
  if(!p)return E_POINTER;*p=nullptr;
  if(id==IID_IUnknown||id==IID_ITfLangBarItem||id==IID_ITfLangBarItemButton)*p=static_cast<ITfLangBarItemButton*>(this);
  else if(id==IID_ITfSource)*p=static_cast<ITfSource*>(this);else return E_NOINTERFACE;
  AddRef();return S_OK;
 }
 ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() override{ULONG r=InterlockedDecrement(&refs);if(!r)delete this;return r;}
 HRESULT STDMETHODCALLTYPE GetInfo(TF_LANGBARITEMINFO* info) override{
  if(!info)return E_POINTER;*info={};info->clsidService=kService;info->guidItem=GUID_LBI_INPUTMODE;
  info->dwStyle=TF_LBI_STYLE_BTN_BUTTON|TF_LBI_STYLE_HIDDENSTATUSCONTROL;wcscpy_s(info->szDescription,kName);return S_OK;
 }
 HRESULT STDMETHODCALLTYPE GetStatus(DWORD* status) override{if(!status)return E_POINTER;*status=active?0:TF_LBI_STATUS_HIDDEN;return S_OK;}
 HRESULT STDMETHODCALLTYPE Show(BOOL) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE GetTooltipString(BSTR* text) override{return GetText(text);}
 HRESULT STDMETHODCALLTYPE GetText(BSTR* text) override{if(!text)return E_POINTER;*text=SysAllocString(kName);return *text?S_OK:E_OUTOFMEMORY;}
 HRESULT STDMETHODCALLTYPE GetIcon(HICON* icon) override{
  if(!icon)return E_POINTER;*icon=static_cast<HICON>(LoadImageW(module,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0));
  return *icon?S_OK:HRESULT_FROM_WIN32(GetLastError());
 }
 HRESULT STDMETHODCALLTYPE OnClick(TfLBIClick,POINT,const RECT*) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE InitMenu(ITfMenu*) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE OnMenuSelect(UINT) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE AdviseSink(REFIID id,IUnknown* unknown,DWORD* cookie) override{
  if(!unknown||!cookie)return E_POINTER;if(id!=IID_ITfLangBarItemSink)return CONNECT_E_CANNOTCONNECT;if(sink)return CONNECT_E_ADVISELIMIT;
  HRESULT hr=unknown->QueryInterface(id,reinterpret_cast<void**>(&sink));if(SUCCEEDED(hr)){*cookie=1;Refresh();}return hr;
 }
 HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD cookie) override{if(cookie!=1||!sink)return CONNECT_E_NOCONNECTION;SafeRelease(sink);return S_OK;}
 void SetActive(bool value){active=value;if(value)Refresh();else if(sink)sink->OnUpdate(TF_LBI_STATUS);}
 void Refresh(){
  // Recheck on the next message dispatch, with no timed delay.
  if(!notifications){std::wstring name=std::wstring(L"HanBonGo.IconNotifications.")+kSourceVersion;WNDCLASSW wc{};wc.hInstance=module;wc.lpfnWndProc=NotifyProc;wc.lpszClassName=name.c_str();RegisterClassW(&wc);
   notifications=CreateWindowExW(0,name.c_str(),L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module,this);}
  if(notifications)PostMessageW(notifications,WM_APP+1,0,0);
 }
};
class Service;
class Edit final:public ITfEditSession {
 LONG refs=1; Service* owner; ITfContext* context;
 std::wstring text; bool finish;
public:
 Edit(Service*,ITfContext*,std::wstring,bool);
 ~Edit();
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p) override {if(!p)return E_POINTER;*p=nullptr;if(id==IID_IUnknown||id==IID_ITfEditSession){*p=static_cast<ITfEditSession*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() override{ULONG r=InterlockedDecrement(&refs);if(!r)delete this;return r;}
 HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie) override;
};
class Service final:public ITfTextInputProcessorEx,public ITfKeyEventSink,public ITfCompositionSink,public ITfThreadMgrEventSink,public ITfThreadFocusSink,public ITfInputProcessorProfileActivationSink,public ITfActiveLanguageProfileNotifySink {
 LONG refs=1;
 ITfThreadMgr* manager=nullptr; TfClientId client=TF_CLIENTID_NULL;
 ITfComposition* composition=nullptr; ITfContext* compositionContext=nullptr;
 DWORD threadCookie=TF_INVALID_COOKIE; bool keyAdvised=false;
 DWORD focusCookie=TF_INVALID_COOKIE,profileCookie=TF_INVALID_COOKIE;
 DWORD languageCookie=TF_INVALID_COOKIE;
 ITfLangBarItemMgr* langBar=nullptr;InputModeItem* inputMode=nullptr;
 enum class CandidateMode {Hidden,Preview,Browsing,Converted};
 std::wstring keys; std::vector<std::wstring> candidates;size_t selected=0;
 // Browsing changes only the highlighted row. Keep the last explicitly
 // applied conversion separate so focus changes cannot confirm a browsed row.
 std::wstring appliedConversion;
 CandidateMode candidateMode=CandidateMode::Hidden;bool dictionaryReady=false;
 hj::Engine engine;hj::LiveCandidates live;HWND popup=nullptr,resultWindow=nullptr;RECT anchor{};
 HANDLE embeddedFont=nullptr;HFONT bodyFont=nullptr,headingFont=nullptr;UINT fontDpi=0;
 bool focused=true;bool keyForeground=false;
 static int Scale(int value,UINT dpi){return MulDiv(value,static_cast<int>(dpi),96);}
 UINT PopupDpi() const {
  using GetDpiForWindowFn=UINT(WINAPI*)(HWND);
  static auto getDpi=reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));
  return popup&&getDpi?getDpi(popup):96;
 }
 void Fonts(UINT dpi){
  if(bodyFont&&headingFont&&fontDpi==dpi)return;
  if(bodyFont)DeleteObject(bodyFont);if(headingFont)DeleteObject(headingFont);
  bodyFont=CreateFontW(-Scale(20,dpi),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Pretendard JP");
  headingFont=CreateFontW(-Scale(14,dpi),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Pretendard JP");fontDpi=dpi;
 }
 void LoadFont(){
  if(embeddedFont)return;
  HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(102),RT_RCDATA);
  if(!resource)return;HGLOBAL loaded=LoadResource(module,resource);DWORD count=0;
  if(loaded)embeddedFont=AddFontMemResourceEx(LockResource(loaded),SizeofResource(module,resource),nullptr,&count);
 }
 static LRESULT CALLBACK ResultProc(HWND h,UINT m,WPARAM w,LPARAM l){
  auto self=reinterpret_cast<Service*>(GetWindowLongPtrW(h,GWLP_USERDATA));
  if(m==WM_NCCREATE){self=static_cast<Service*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
  if(m==WM_APP+2&&self){
   self->AddRef();bool active=IsHanBonGoActive();
   // COM profile queries can reenter TSF and deactivate this service. Retain
   // the object and recheck all UI state after that query before showing results.
   if(active&&self->candidateMode==CandidateMode::Preview&&self->focused&&self->keyForeground&&self->composition&&self->compositionContext){
    std::vector<std::wstring> results;if(self->live.Take(results)&&!results.empty()){self->candidates=std::move(results);self->selected=0;self->dictionaryReady=true;self->Show();}
   }self->Release();return 0;
  }
  return DefWindowProcW(h,m,w,l);
 }
 void SchedulePreview(){
  if(candidateMode!=CandidateMode::Preview||keys.empty())return;
  if(!resultWindow){std::wstring name=std::wstring(L"HanBonGo.CandidateResults.")+kSourceVersion;WNDCLASSW wc{};wc.hInstance=module;wc.lpfnWndProc=ResultProc;wc.lpszClassName=name.c_str();RegisterClassW(&wc);
   resultWindow=CreateWindowExW(0,name.c_str(),L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module,this);}
  if(resultWindow&&live.Start(module,&objects,resultWindow,WM_APP+2))live.Submit(hj::Reading(hj::Compose(keys)));
 }
 void Preview(){
  candidates=hj::KanaCandidates(hj::Reading(hj::Compose(keys)));
  selected=0;candidateMode=CandidateMode::Preview;appliedConversion.clear();dictionaryReady=false;live.Cancel();
 }
 static LRESULT CALLBACK WindowProc(HWND h,UINT m,WPARAM w,LPARAM l){
  auto self=reinterpret_cast<Service*>(GetWindowLongPtrW(h,GWLP_USERDATA));
  if(m==WM_NCCREATE){self=static_cast<Service*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
  if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
  if(m==WM_DPICHANGED&&self){self->fontDpi=0;self->Show();return 0;}
  if((m==WM_PAINT||m==WM_PRINTCLIENT)&&self){PAINTSTRUCT ps{};HDC dc=m==WM_PAINT?BeginPaint(h,&ps):reinterpret_cast<HDC>(w);if(!dc)return 0;RECT r;GetClientRect(h,&r);FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
   UINT dpi=self->PopupDpi();self->Fonts(dpi);auto old=SelectObject(dc,self->headingFont);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(117,133,154));
   std::wstring heading=hj::Compose(self->keys)+L" → "+hj::Reading(hj::Compose(self->keys));
   RECT headingRect{Scale(14,dpi),Scale(8,dpi),r.right-Scale(14,dpi),Scale(35,dpi)};DrawTextW(dc,heading.c_str(),static_cast<int>(heading.size()),&headingRect,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
   SelectObject(dc,self->bodyFont);
   size_t start=(self->selected/9)*9;
   for(size_t i=start;i<std::min(start+9,self->candidates.size());++i){RECT row{Scale(4,dpi),Scale(38+static_cast<int>((i-start)*34),dpi),r.right-Scale(4,dpi),Scale(72+static_cast<int>((i-start)*34),dpi)};
    bool selected=self->candidateMode!=CandidateMode::Preview&&i==self->selected;
    if(selected){HBRUSH blue=CreateSolidBrush(RGB(0,120,212));FillRect(dc,&row,blue);DeleteObject(blue);}
    SetTextColor(dc,selected?RGB(255,255,255):RGB(142,151,164));
    RECT numberRect{Scale(15,dpi),row.top,Scale(45,dpi),row.bottom};std::wstring number=std::to_wstring(i-start+1);DrawTextW(dc,number.c_str(),static_cast<int>(number.size()),&numberRect,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    SetTextColor(dc,selected?RGB(255,255,255):RGB(28,34,43));RECT textRect{Scale(51,dpi),row.top,r.right-Scale(15,dpi),row.bottom};
    DrawTextW(dc,self->candidates[i].c_str(),static_cast<int>(self->candidates[i].size()),&textRect,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);}
   SelectObject(dc,old);if(m==WM_PAINT)EndPaint(h,&ps);return 0;
  }
  return DefWindowProcW(h,m,w,l);
 }
 void Hide(){if(popup)ShowWindow(popup,SW_HIDE);}
 void Show(){
  if(candidateMode==CandidateMode::Hidden||candidates.empty()||!focused){Hide();return;}
  if(!popup){std::wstring popupClass=std::wstring(L"HangulJpIME.Candidates.")+kSourceVersion;WNDCLASSW wc{};wc.hInstance=module;wc.lpfnWndProc=WindowProc;wc.lpszClassName=popupClass.c_str();wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
   popup=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,wc.lpszClassName,kName,WS_POPUP|WS_BORDER,0,0,420,100,nullptr,nullptr,module,this);}
  RECT work{};MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&info);work=info.rcWork;
  UINT dpi=PopupDpi();Fonts(dpi);
  HDC measure=GetDC(popup);auto old=SelectObject(measure,bodyFont);int contentWidth=0;
  for(const auto& candidate:candidates){SIZE size{};GetTextExtentPoint32W(measure,candidate.c_str(),static_cast<int>(candidate.size()),&size);contentWidth=std::max(contentWidth,static_cast<int>(size.cx));}
  SelectObject(measure,headingFont);std::wstring heading=hj::Compose(keys)+L" → "+hj::Reading(hj::Compose(keys));SIZE headingSize{};GetTextExtentPoint32W(measure,heading.c_str(),static_cast<int>(heading.size()),&headingSize);
  SelectObject(measure,old);ReleaseDC(popup,measure);
  int width=std::max(Scale(360,dpi),std::max(contentWidth+Scale(76,dpi),static_cast<int>(headingSize.cx)+Scale(30,dpi)));
  width=std::min(width,std::min(Scale(760,dpi),static_cast<int>(work.right-work.left)));
  int height=Scale(44+static_cast<int>(std::min<size_t>(9,candidates.size()-(selected/9)*9))*34,dpi);
  int x=std::clamp<LONG>(anchor.left,work.left,std::max(work.left,work.right-width));int y=anchor.bottom;
  if(y+height>work.bottom)y=std::max<LONG>(work.top,anchor.top-height);
  SetWindowPos(popup,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);InvalidateRect(popup,nullptr,TRUE);
 }
 void Reset(){live.Cancel();keys.clear();candidates.clear();selected=0;candidateMode=CandidateMode::Hidden;appliedConversion.clear();dictionaryReady=false;Hide();}
 bool Wants(UINT k){
  if(GetKeyState(VK_CONTROL)&0x8000||GetKeyState(VK_MENU)&0x8000||GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000)return false;
  if(k>='A'&&k<='Z')return true;
  if(k==VK_OEM_MINUS&&!(GetKeyState(VK_SHIFT)&0x8000))return true;
  if(keys.empty())return false;
  return k==VK_SPACE||k==VK_RETURN||k==VK_ESCAPE||k==VK_BACK||k==VK_F6||k==VK_F7||
   (!candidates.empty()&&(k==VK_UP||k==VK_DOWN||k==VK_LEFT||k==VK_RIGHT||k==VK_PRIOR||k==VK_NEXT||(k>='1'&&k<='9')));
 }
 HRESULT Request(ITfContext* c,const std::wstring& text,bool finish){
  if(!c)return E_INVALIDARG;
  auto session=new Edit(this,c,text,finish);HRESULT result=E_FAIL;
  HRESULT hr=c->RequestEditSession(client,session,TF_ES_SYNC|TF_ES_READWRITE,&result);
  // Key callbacks normally allow a synchronous edit. Refuse the key if the
  // host cannot grant it; this avoids stale asynchronous edits in another field.
  session->Release();return FAILED(hr)?hr:result;
 }
 HRESULT Finish(ITfContext* c,bool cancel=false,bool confirmCandidate=false){
  if(keys.empty()&&!composition)return S_OK;
  bool chosen=confirmCandidate&&(candidateMode==CandidateMode::Browsing||candidateMode==CandidateMode::Converted)&&!candidates.empty();
  std::wstring s=cancel?L"":chosen?candidates[selected]:!appliedConversion.empty()?appliedConversion:hj::Reading(hj::Compose(keys));
  HRESULT hr=Request(c,s,true);if(SUCCEEDED(hr))Reset();return hr;
 }
 void FinishFocus(){live.Cancel();if(compositionContext)Finish(compositionContext);else Reset();Hide();}
public:
 Service(){InterlockedIncrement(&objects);LoadFont();}
 ~Service(){Deactivate();if(popup)DestroyWindow(popup);std::wstring name=std::wstring(L"HangulJpIME.Candidates.")+kSourceVersion;UnregisterClassW(name.c_str(),module);if(bodyFont)DeleteObject(bodyFont);if(headingFont)DeleteObject(headingFont);if(embeddedFont)RemoveFontMemResourceEx(embeddedFont);InterlockedDecrement(&objects);}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p) override{
  if(!p)return E_POINTER;*p=nullptr;
  if(id==IID_IUnknown||id==IID_ITfTextInputProcessor||id==IID_ITfTextInputProcessorEx)*p=static_cast<ITfTextInputProcessorEx*>(this);
  else if(id==IID_ITfKeyEventSink)*p=static_cast<ITfKeyEventSink*>(this);
  else if(id==IID_ITfCompositionSink)*p=static_cast<ITfCompositionSink*>(this);
  else if(id==IID_ITfThreadMgrEventSink)*p=static_cast<ITfThreadMgrEventSink*>(this);
  else if(id==IID_ITfThreadFocusSink)*p=static_cast<ITfThreadFocusSink*>(this);
  else if(id==IID_ITfInputProcessorProfileActivationSink)*p=static_cast<ITfInputProcessorProfileActivationSink*>(this);
  else if(id==IID_ITfActiveLanguageProfileNotifySink)*p=static_cast<ITfActiveLanguageProfileNotifySink*>(this);
  else return E_NOINTERFACE;AddRef();return S_OK;
 }
 ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() override{ULONG r=InterlockedDecrement(&refs);if(!r)delete this;return r;}
 HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr* m,TfClientId id) override{return ActivateEx(m,id,0);}
 HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr* m,TfClientId id,DWORD flags) override{
  TraceActivation(L"ActivateEx flags",static_cast<HRESULT>(flags));
  if(manager)return E_UNEXPECTED;manager=m;manager->AddRef();client=id;
  ITfKeystrokeMgr* km=nullptr;HRESULT hr=manager->QueryInterface(IID_ITfKeystrokeMgr,reinterpret_cast<void**>(&km));
  if(SUCCEEDED(hr)){hr=km->AdviseKeyEventSink(client,this,TRUE);TraceActivation(L"AdviseKeyEventSink",hr);keyAdvised=SUCCEEDED(hr);km->Release();}
  if(FAILED(hr)){Deactivate();return hr;}
  ITfSource* source=nullptr;hr=manager->QueryInterface(IID_ITfSource,reinterpret_cast<void**>(&source));
  if(SUCCEEDED(hr)){
   hr=source->AdviseSink(IID_ITfThreadMgrEventSink,static_cast<ITfThreadMgrEventSink*>(this),&threadCookie);
   TraceActivation(L"ThreadMgrEventSink",hr);
   if(SUCCEEDED(hr))hr=source->AdviseSink(IID_ITfThreadFocusSink,static_cast<ITfThreadFocusSink*>(this),&focusCookie);
   TraceActivation(L"ThreadFocusSink",hr);
   if(SUCCEEDED(hr))hr=source->AdviseSink(IID_ITfInputProcessorProfileActivationSink,static_cast<ITfInputProcessorProfileActivationSink*>(this),&profileCookie);
   TraceActivation(L"ProfileActivationSink",hr);
   if(SUCCEEDED(hr))hr=source->AdviseSink(IID_ITfActiveLanguageProfileNotifySink,static_cast<ITfActiveLanguageProfileNotifySink*>(this),&languageCookie);
   TraceActivation(L"ActiveLanguageProfileNotifySink",hr);
   source->Release();}
  if(FAILED(hr)){Deactivate();return hr;}
  hr=manager->QueryInterface(IID_ITfLangBarItemMgr,reinterpret_cast<void**>(&langBar));
  if(SUCCEEDED(hr)){inputMode=new InputModeItem;hr=langBar->AddItem(inputMode);TraceActivation(L"LanguageBar AddItem",hr);}
  if(FAILED(hr)){Deactivate();return hr;}inputMode->Refresh();return S_OK;
 }
 HRESULT STDMETHODCALLTYPE Deactivate() override{
  TraceActivation(L"Deactivate",S_OK);
  live.Stop();if(resultWindow){DestroyWindow(resultWindow);resultWindow=nullptr;}
  std::wstring resultClass=std::wstring(L"HanBonGo.CandidateResults.")+kSourceVersion;UnregisterClassW(resultClass.c_str(),module);
  if(langBar&&inputMode)langBar->RemoveItem(inputMode);
  SafeRelease(inputMode);SafeRelease(langBar);
  if(manager){FinishFocus();ITfKeystrokeMgr* km=nullptr;if(keyAdvised&&SUCCEEDED(manager->QueryInterface(IID_ITfKeystrokeMgr,reinterpret_cast<void**>(&km)))){km->UnadviseKeyEventSink(client);km->Release();}
   ITfSource* s=nullptr;if(SUCCEEDED(manager->QueryInterface(IID_ITfSource,reinterpret_cast<void**>(&s)))){
    if(threadCookie!=TF_INVALID_COOKIE)s->UnadviseSink(threadCookie);
    if(focusCookie!=TF_INVALID_COOKIE)s->UnadviseSink(focusCookie);
    if(profileCookie!=TF_INVALID_COOKIE)s->UnadviseSink(profileCookie);
    if(languageCookie!=TF_INVALID_COOKIE)s->UnadviseSink(languageCookie);s->Release();}
  }
  SafeRelease(composition);SafeRelease(compositionContext);SafeRelease(manager);client=TF_CLIENTID_NULL;threadCookie=focusCookie=profileCookie=languageCookie=TF_INVALID_COOKIE;keyAdvised=false;keyForeground=false;Reset();return S_OK;
 }
 HRESULT Apply(ITfContext* c,TfEditCookie ec,const std::wstring& text,bool finish){
  if(compositionContext&&compositionContext!=c)return E_UNEXPECTED;
  ITfRange* range=nullptr;
  if(!composition){
   TF_SELECTION selection{};ULONG fetched=0;HRESULT hr=c->GetSelection(ec,TF_DEFAULT_SELECTION,1,&selection,&fetched);if(FAILED(hr)||!fetched)return FAILED(hr)?hr:E_FAIL;
   range=selection.range;ITfContextComposition* cc=nullptr;hr=c->QueryInterface(IID_ITfContextComposition,reinterpret_cast<void**>(&cc));
   if(SUCCEEDED(hr)){hr=cc->StartComposition(ec,range,this,&composition);cc->Release();}
   if(FAILED(hr)||!composition){range->Release();return FAILED(hr)?hr:E_FAIL;}
   compositionContext=c;c->AddRef();
  }else {HRESULT hr=composition->GetRange(&range);if(FAILED(hr))return hr;}
  HRESULT hr=range->SetText(ec,0,text.c_str(),static_cast<LONG>(text.size()));
  if(SUCCEEDED(hr)){
   ITfContextView* view=nullptr;if(SUCCEEDED(c->GetActiveView(&view))){BOOL clipped;RECT r;if(SUCCEEDED(view->GetTextExt(ec,range,&r,&clipped)))anchor=r;view->Release();}
   ITfRange* caret=nullptr;if(SUCCEEDED(range->Clone(&caret))){caret->Collapse(ec,TF_ANCHOR_END);TF_SELECTION sel{caret,{TF_AE_NONE,FALSE}};c->SetSelection(ec,1,&sel);caret->Release();}
   if(finish){auto comp=composition;composition=nullptr;SafeRelease(compositionContext);hr=comp->EndComposition(ec);comp->Release();}
  }
  range->Release();if(SUCCEEDED(hr)){if(finish)Hide();else Show();}return hr;
 }
 HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL yes) override{TraceActivation(yes?L"Key focus gained":L"Key focus lost",S_OK);keyForeground=yes!=FALSE;focused=yes!=FALSE;if(!yes)FinishFocus();else if(inputMode)inputMode->SetActive(true);return S_OK;}
 HRESULT STDMETHODCALLTYPE OnSetThreadFocus() override{focused=true;if(inputMode)inputMode->Refresh();return S_OK;}
 HRESULT STDMETHODCALLTYPE OnKillThreadFocus() override{focused=false;FinishFocus();return S_OK;}
 HRESULT STDMETHODCALLTYPE OnActivated(DWORD type,LANGID,REFCLSID clsid,REFGUID category,REFGUID profile,HKL,DWORD flags) override{
  TraceActivation(clsid==kService?L"Own profile notification":L"Other profile notification",static_cast<HRESULT>(flags));
  if(category!=GUID_TFCAT_TIP_KEYBOARD)return S_OK;
  if(flags&TF_IPSINK_FLAG_ACTIVE){
   if(type==TF_PROFILETYPE_INPUTPROCESSOR&&clsid==kService&&profile==kProfile){if(inputMode)inputMode->SetActive(true);}
   else {keyForeground=false;if(inputMode)inputMode->SetActive(false);FinishFocus();}
  }else if(clsid==kService&&profile==kProfile){keyForeground=false;if(inputMode)inputMode->SetActive(false);FinishFocus();}return S_OK;
 }
 HRESULT STDMETHODCALLTYPE OnActivated(REFCLSID clsid,REFGUID profile,BOOL yes) override{
  TraceActivation(clsid==kService?L"Own language notification":L"Other language notification",static_cast<HRESULT>(yes));
  if(clsid==kService&&profile==kProfile){if(inputMode)inputMode->SetActive(yes!=FALSE);if(!yes){keyForeground=false;FinishFocus();}}return S_OK;
 }
 HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext*,WPARAM k,LPARAM,BOOL* eaten) override{
  if(!keyForeground||!IsHanBonGoActive()){*eaten=FALSE;return S_OK;}
  bool boundary=!keys.empty()&&k!=VK_SHIFT&&k!=VK_LSHIFT&&k!=VK_RSHIFT&&k!=VK_CONTROL&&k!=VK_MENU&&k!=VK_LWIN&&k!=VK_RWIN;
  *eaten=Wants(static_cast<UINT>(k))||boundary;return S_OK;
 }
 HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext*,WPARAM,LPARAM,BOOL* eaten) override{*eaten=FALSE;return S_OK;}
 HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext*,WPARAM,LPARAM,BOOL* eaten) override{*eaten=FALSE;return S_OK;}
 HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext*,REFGUID,BOOL* eaten) override{*eaten=FALSE;return S_OK;}
 HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext* c,WPARAM key,LPARAM,BOOL* eaten) override{
  UINT k=static_cast<UINT>(key);*eaten=FALSE;
  if(!keyForeground||!IsHanBonGoActive())return S_OK;
  if(!Wants(k)){if(!keys.empty())Finish(c);return S_OK;}
  auto oldKeys=keys;auto oldCandidates=candidates;auto oldSelected=selected;auto oldMode=candidateMode;auto oldApplied=appliedConversion;bool oldReady=dictionaryReady;HRESULT hr=S_OK;bool schedule=false;
  if(k==VK_RETURN)hr=Finish(c,false,true);
  else if(k==VK_ESCAPE){
   live.Cancel();if(candidateMode!=CandidateMode::Hidden){candidates.clear();selected=0;candidateMode=CandidateMode::Hidden;appliedConversion.clear();dictionaryReady=false;hr=Request(c,hj::Reading(hj::Compose(keys)),false);}else hr=Finish(c,true);
  }
  else if(k==VK_BACK){
   live.Cancel();keys.pop_back();if(keys.empty()){candidates.clear();candidateMode=CandidateMode::Hidden;hr=Request(c,L"",true);if(SUCCEEDED(hr))Reset();}
   else {Preview();hr=Request(c,hj::Reading(hj::Compose(keys)),false);schedule=true;}
  }
  else if(k==VK_SPACE){
   live.Cancel();if(candidateMode==CandidateMode::Browsing){
    // Space applies the row the user is browsing; arrows alone never do.
    candidateMode=CandidateMode::Converted;dictionaryReady=true;
   }else if(candidateMode!=CandidateMode::Converted){
    // Conversion is explicit. A preview never silently changes document text.
    if(!dictionaryReady)candidates=engine.Convert(hj::Reading(hj::Compose(keys)));
    selected=0;candidateMode=CandidateMode::Converted;dictionaryReady=true;
   }else selected=(selected+1)%candidates.size();appliedConversion=candidates[selected];hr=Request(c,appliedConversion,false);
  }
  else if(k==VK_OEM_MINUS){
   if(!appliedConversion.empty()){hr=Finish(c);if(FAILED(hr))return S_OK;}
   if(keys.size()>=256){*eaten=TRUE;return S_OK;}keys+=L'-';Preview();hr=Request(c,hj::Reading(hj::Compose(keys)),false);schedule=true;
  }
  else if(!candidates.empty()&&(k>='1'&&k<='9')){size_t index=(selected/9)*9+k-'1';if(index<candidates.size()){live.Cancel();candidateMode=CandidateMode::Browsing;selected=index;hr=Finish(c,false,true);}}
  else if(!candidates.empty()&&(k==VK_UP||k==VK_LEFT||k==VK_DOWN||k==VK_RIGHT||k==VK_PRIOR||k==VK_NEXT)){
   live.Cancel();bool unselected=candidateMode==CandidateMode::Preview;candidateMode=CandidateMode::Browsing;
   int step=(k==VK_PRIOR||k==VK_NEXT)?9:1;int dir=(k==VK_UP||k==VK_LEFT||k==VK_PRIOR)?-1:1;
   selected=unselected?(dir>0?0:candidates.size()-1):static_cast<size_t>((static_cast<int>(selected)+dir*step+static_cast<int>(candidates.size())*9)%static_cast<int>(candidates.size()));Show();
  }else if(k==VK_F6||k==VK_F7){live.Cancel();candidateMode=CandidateMode::Converted;dictionaryReady=true;candidates={k==VK_F7?hj::Katakana(hj::Reading(hj::Compose(keys))):hj::Reading(hj::Compose(keys))};selected=0;appliedConversion=candidates[0];hr=Request(c,appliedConversion,false);}
  else if(auto j=hj::KeyJamo(k,(GetKeyState(VK_SHIFT)&0x8000)!=0)){
   if(!appliedConversion.empty()){hr=Finish(c);if(FAILED(hr))return S_OK;}
   if(keys.size()>=256){*eaten=TRUE;return S_OK;}keys+=j;Preview();hr=Request(c,hj::Reading(hj::Compose(keys)),false);schedule=true;
  }
  if(SUCCEEDED(hr)){*eaten=TRUE;if(schedule)SchedulePreview();}
  else {keys=std::move(oldKeys);candidates=std::move(oldCandidates);selected=oldSelected;candidateMode=oldMode;appliedConversion=std::move(oldApplied);dictionaryReady=oldReady;Show();if(candidateMode==CandidateMode::Preview)SchedulePreview();}return S_OK;
 }
 HRESULT STDMETHODCALLTYPE OnCompositionTerminated(TfEditCookie,ITfComposition* comp) override{if(comp==composition){SafeRelease(composition);SafeRelease(compositionContext);Reset();}return S_OK;}
 HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr*) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr*) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE OnSetFocus(ITfDocumentMgr*,ITfDocumentMgr*) override{FinishFocus();if(inputMode)inputMode->Refresh();return S_OK;}
 HRESULT STDMETHODCALLTYPE OnPushContext(ITfContext*) override{FinishFocus();return S_OK;}
 HRESULT STDMETHODCALLTYPE OnPopContext(ITfContext* c) override{if(c==compositionContext)FinishFocus();return S_OK;}
};
Edit::Edit(Service* s,ITfContext* c,std::wstring t,bool f):owner(s),context(c),text(std::move(t)),finish(f){owner->AddRef();context->AddRef();InterlockedIncrement(&objects);}
Edit::~Edit(){owner->Release();context->Release();InterlockedDecrement(&objects);}
HRESULT Edit::DoEditSession(TfEditCookie ec){return owner->Apply(context,ec,text,finish);}
class Factory final:public IClassFactory {
 LONG refs=1;
public:
 Factory(){InterlockedIncrement(&objects);}~Factory(){InterlockedDecrement(&objects);}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p) override{if(!p)return E_POINTER;*p=nullptr;if(id==IID_IUnknown||id==IID_IClassFactory){*p=static_cast<IClassFactory*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() override{ULONG r=InterlockedDecrement(&refs);if(!r)delete this;return r;}
 HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID id,void** p) override{if(outer)return CLASS_E_NOAGGREGATION;auto s=new Service;HRESULT hr=s->QueryInterface(id,p);s->Release();return hr;}
 HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override{if(lock)InterlockedIncrement(&objects);else InterlockedDecrement(&objects);return S_OK;}
};
extern "C" BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=h;DisableThreadLibraryCalls(h);}return TRUE;}
extern "C" HRESULT WINAPI DllCanUnloadNow(){return objects==0?S_OK:S_FALSE;}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID cls,REFIID id,void** p){if(cls!=kService)return CLASS_E_CLASSNOTAVAILABLE;auto f=new Factory;HRESULT hr=f->QueryInterface(id,p);f->Release();return hr;}
static std::wstring ClassKey(){wchar_t guid[40];StringFromGUID2(kService,guid,40);return std::wstring(L"Software\\Classes\\CLSID\\")+guid;}
extern "C" HRESULT WINAPI DllRegisterServer(){
 wchar_t path[32768];DWORD length=GetModuleFileNameW(module,path,32768);if(!length||length>=32768)return E_FAIL;
 HKEY key=nullptr;std::wstring cls=ClassKey();LSTATUS error=RegCreateKeyExW(HKEY_LOCAL_MACHINE,cls.c_str(),0,nullptr,0,KEY_WRITE,nullptr,&key,nullptr);
 if(error!=ERROR_SUCCESS)return HRESULT_FROM_WIN32(error);
 error=RegSetValueExW(key,nullptr,0,REG_SZ,reinterpret_cast<const BYTE*>(kName),static_cast<DWORD>((wcslen(kName)+1)*sizeof(wchar_t)));RegCloseKey(key);
 if(error!=ERROR_SUCCESS)return HRESULT_FROM_WIN32(error);
 error=RegCreateKeyExW(HKEY_LOCAL_MACHINE,(cls+L"\\InprocServer32").c_str(),0,nullptr,0,KEY_WRITE,nullptr,&key,nullptr);
 if(error!=ERROR_SUCCESS)return HRESULT_FROM_WIN32(error);
 error=RegSetValueExW(key,nullptr,0,REG_SZ,reinterpret_cast<const BYTE*>(path),(length+1)*sizeof(wchar_t));
 if(error==ERROR_SUCCESS)error=RegSetValueExW(key,L"ThreadingModel",0,REG_SZ,reinterpret_cast<const BYTE*>(L"Apartment"),sizeof(L"Apartment"));RegCloseKey(key);
 if(error!=ERROR_SUCCESS)return HRESULT_FROM_WIN32(error);
 HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)return init;
 ITfInputProcessorProfileMgr* profiles=nullptr;HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(&profiles));
 if(SUCCEEDED(hr)){hr=profiles->RegisterProfile(kService,0x0411,kProfile,kName,static_cast<ULONG>(wcslen(kName)),path,length,static_cast<ULONG>(-101),nullptr,0,TRUE,0);profiles->Release();}
 ITfCategoryMgr* categories=nullptr;if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,IID_ITfCategoryMgr,reinterpret_cast<void**>(&categories));
 if(SUCCEEDED(hr)){hr=categories->RegisterCategory(kService,GUID_TFCAT_TIP_KEYBOARD,kService);if(SUCCEEDED(hr))hr=categories->RegisterCategory(kService,GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,kService);if(SUCCEEDED(hr))hr=categories->RegisterCategory(kService,GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,kService);if(SUCCEEDED(hr))hr=categories->RegisterCategory(kService,GUID_TFCAT_TIPCAP_COMLESS,kService);categories->Release();}
 if(SUCCEEDED(init))CoUninitialize();if(FAILED(hr))RegDeleteTreeW(HKEY_LOCAL_MACHINE,cls.c_str());return hr;
}
extern "C" HRESULT WINAPI DllUnregisterServer(){
 HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)return init;
 HRESULT result=S_OK;ITfInputProcessorProfileMgr* p=nullptr;HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(&p));
 if(SUCCEEDED(hr)){hr=p->UnregisterProfile(kService,0x0411,kProfile,0);if(FAILED(hr))result=hr;p->Release();}else result=hr;
 ITfCategoryMgr* c=nullptr;if(SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,IID_ITfCategoryMgr,reinterpret_cast<void**>(&c)))){c->UnregisterCategory(kService,GUID_TFCAT_TIP_KEYBOARD,kService);c->UnregisterCategory(kService,GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,kService);c->UnregisterCategory(kService,GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,kService);c->UnregisterCategory(kService,GUID_TFCAT_TIPCAP_COMLESS,kService);c->Release();}
 LSTATUS error=RegDeleteTreeW(HKEY_LOCAL_MACHINE,ClassKey().c_str());if(error!=ERROR_SUCCESS&&error!=ERROR_FILE_NOT_FOUND)result=HRESULT_FROM_WIN32(error);
 if(SUCCEEDED(init))CoUninitialize();return result;
}
