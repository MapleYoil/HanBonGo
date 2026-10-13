#include <windows.h>
#include <msctf.h>
#include <ctffunc.h>
#include <string>
#include <vector>
#include <algorithm>
#include "core.h"
#include "identity.h"

// Stable identities shared by the x86 and x64 builds.
static const wchar_t* kName=L"한본어 입력기";
static HINSTANCE module;
static LONG objects=0;
template<class T> static void SafeRelease(T*& p){if(p){p->Release();p=nullptr;}}
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
class Service final:public ITfTextInputProcessorEx,public ITfKeyEventSink,public ITfCompositionSink,public ITfThreadMgrEventSink {
 LONG refs=1;
 ITfThreadMgr* manager=nullptr; TfClientId client=TF_CLIENTID_NULL;
 ITfComposition* composition=nullptr; ITfContext* compositionContext=nullptr;
 DWORD threadCookie=TF_INVALID_COOKIE; bool keyAdvised=false;
 std::wstring keys; std::vector<std::wstring> candidates;size_t selected=0;
 hj::Engine engine; HWND popup=nullptr; RECT anchor{};
 bool focused=true;
 static LRESULT CALLBACK WindowProc(HWND h,UINT m,WPARAM w,LPARAM l){
  auto self=reinterpret_cast<Service*>(GetWindowLongPtrW(h,GWLP_USERDATA));
  if(m==WM_NCCREATE){self=static_cast<Service*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
  if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
  if(m==WM_PAINT&&self){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);FillRect(dc,&r,reinterpret_cast<HBRUSH>(COLOR_WINDOW+1));
   HFONT font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Yu Gothic UI");
   auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
   std::wstring heading=hj::Compose(self->keys)+L" → "+hj::Reading(hj::Compose(self->keys));
   TextOutW(dc,10,8,heading.c_str(),static_cast<int>(heading.size()));
   size_t start=(self->selected/9)*9;
   for(size_t i=start;i<std::min(start+9,self->candidates.size());++i){RECT row{4,36+static_cast<LONG>((i-start)*28),r.right-4,64+static_cast<LONG>((i-start)*28)};
    if(i==self->selected){FillRect(dc,&row,reinterpret_cast<HBRUSH>(COLOR_HIGHLIGHT+1));SetTextColor(dc,GetSysColor(COLOR_HIGHLIGHTTEXT));}else SetTextColor(dc,GetSysColor(COLOR_WINDOWTEXT));
    std::wstring s=std::to_wstring(i-start+1)+L"  "+self->candidates[i];TextOutW(dc,10,row.top+3,s.c_str(),static_cast<int>(s.size()));}
   SelectObject(dc,old);DeleteObject(font);EndPaint(h,&ps);return 0;
  }
  return DefWindowProcW(h,m,w,l);
 }
 void Hide(){if(popup)ShowWindow(popup,SW_HIDE);}
 void Show(){
  if(candidates.empty()||!focused){Hide();return;}
  if(!popup){std::wstring popupClass=std::wstring(L"HangulJpIME.Candidates.")+kSourceVersion;WNDCLASSW wc{};wc.hInstance=module;wc.lpfnWndProc=WindowProc;wc.lpszClassName=popupClass.c_str();wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
   popup=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,wc.lpszClassName,kName,WS_POPUP|WS_BORDER,0,0,420,100,nullptr,nullptr,module,this);}
  RECT work{};MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&info);work=info.rcWork;
  int width=420,height=42+static_cast<int>(std::min<size_t>(9,candidates.size()-(selected/9)*9))*28;
  int x=std::clamp<LONG>(anchor.left,work.left,std::max(work.left,work.right-width));int y=anchor.bottom;
  if(y+height>work.bottom)y=std::max<LONG>(work.top,anchor.top-height);
  SetWindowPos(popup,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);InvalidateRect(popup,nullptr,TRUE);
 }
 void Reset(){keys.clear();candidates.clear();selected=0;Hide();}
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
 HRESULT Finish(ITfContext* c,bool cancel=false){
  if(keys.empty()&&!composition)return S_OK;
  std::wstring s=cancel?L"":candidates.empty()?hj::Reading(hj::Compose(keys)):candidates[selected];
  HRESULT hr=Request(c,s,true);if(SUCCEEDED(hr))Reset();return hr;
 }
 void FinishFocus(){if(compositionContext)Finish(compositionContext);else Reset();Hide();}
public:
 Service(){InterlockedIncrement(&objects);}
 ~Service(){Deactivate();if(popup)DestroyWindow(popup);InterlockedDecrement(&objects);}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p) override{
  if(!p)return E_POINTER;*p=nullptr;
  if(id==IID_IUnknown||id==IID_ITfTextInputProcessor||id==IID_ITfTextInputProcessorEx)*p=static_cast<ITfTextInputProcessorEx*>(this);
  else if(id==IID_ITfKeyEventSink)*p=static_cast<ITfKeyEventSink*>(this);
  else if(id==IID_ITfCompositionSink)*p=static_cast<ITfCompositionSink*>(this);
  else if(id==IID_ITfThreadMgrEventSink)*p=static_cast<ITfThreadMgrEventSink*>(this);
  else return E_NOINTERFACE;AddRef();return S_OK;
 }
 ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
 ULONG STDMETHODCALLTYPE Release() override{ULONG r=InterlockedDecrement(&refs);if(!r)delete this;return r;}
 HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr* m,TfClientId id) override{return ActivateEx(m,id,0);}
 HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr* m,TfClientId id,DWORD) override{
  if(manager)return E_UNEXPECTED;manager=m;manager->AddRef();client=id;
  ITfKeystrokeMgr* km=nullptr;HRESULT hr=manager->QueryInterface(IID_ITfKeystrokeMgr,reinterpret_cast<void**>(&km));
  if(SUCCEEDED(hr)){hr=km->AdviseKeyEventSink(client,this,TRUE);keyAdvised=SUCCEEDED(hr);km->Release();}
  if(FAILED(hr)){Deactivate();return hr;}
  ITfSource* source=nullptr;hr=manager->QueryInterface(IID_ITfSource,reinterpret_cast<void**>(&source));
  if(SUCCEEDED(hr)){hr=source->AdviseSink(IID_ITfThreadMgrEventSink,static_cast<ITfThreadMgrEventSink*>(this),&threadCookie);source->Release();}
  if(FAILED(hr)){Deactivate();return hr;}return S_OK;
 }
 HRESULT STDMETHODCALLTYPE Deactivate() override{
  if(manager){FinishFocus();ITfKeystrokeMgr* km=nullptr;if(keyAdvised&&SUCCEEDED(manager->QueryInterface(IID_ITfKeystrokeMgr,reinterpret_cast<void**>(&km)))){km->UnadviseKeyEventSink(client);km->Release();}
   ITfSource* s=nullptr;if(threadCookie!=TF_INVALID_COOKIE&&SUCCEEDED(manager->QueryInterface(IID_ITfSource,reinterpret_cast<void**>(&s)))){s->UnadviseSink(threadCookie);s->Release();}
  }
  SafeRelease(composition);SafeRelease(compositionContext);SafeRelease(manager);client=TF_CLIENTID_NULL;threadCookie=TF_INVALID_COOKIE;keyAdvised=false;Reset();return S_OK;
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
 HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL yes) override{focused=yes!=FALSE;if(!yes)FinishFocus();return S_OK;}
 HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext*,WPARAM k,LPARAM,BOOL* eaten) override{
  bool boundary=!keys.empty()&&k!=VK_SHIFT&&k!=VK_LSHIFT&&k!=VK_RSHIFT&&k!=VK_CONTROL&&k!=VK_MENU&&k!=VK_LWIN&&k!=VK_RWIN;
  *eaten=Wants(static_cast<UINT>(k))||boundary;return S_OK;
 }
 HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext*,WPARAM,LPARAM,BOOL* eaten) override{*eaten=FALSE;return S_OK;}
 HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext*,WPARAM,LPARAM,BOOL* eaten) override{*eaten=FALSE;return S_OK;}
 HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext*,REFGUID,BOOL* eaten) override{*eaten=FALSE;return S_OK;}
 HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext* c,WPARAM key,LPARAM,BOOL* eaten) override{
  UINT k=static_cast<UINT>(key);*eaten=FALSE;
  if(!Wants(k)){if(!keys.empty())Finish(c);return S_OK;}
  auto oldKeys=keys;auto oldCandidates=candidates;auto oldSelected=selected;HRESULT hr=S_OK;
  if(k==VK_RETURN)hr=Finish(c);
  else if(k==VK_ESCAPE){if(!candidates.empty()){candidates.clear();hr=Request(c,hj::Reading(hj::Compose(keys)),false);}else hr=Finish(c,true);}
  else if(k==VK_BACK){candidates.clear();keys.pop_back();hr=Request(c,hj::Reading(hj::Compose(keys)),keys.empty());}
  else if(k==VK_SPACE){if(candidates.empty()){candidates=engine.Convert(hj::Reading(hj::Compose(keys)));selected=0;}else selected=(selected+1)%candidates.size();hr=Request(c,candidates[selected],false);}
  else if(k==VK_OEM_MINUS){
   if(!candidates.empty()){hr=Finish(c);if(FAILED(hr))return S_OK;}
   if(keys.size()>=256){*eaten=TRUE;return S_OK;}keys+=L'-';hr=Request(c,hj::Reading(hj::Compose(keys)),false);
  }
  else if(!candidates.empty()&&(k>='1'&&k<='9')){size_t index=(selected/9)*9+k-'1';if(index<candidates.size()){selected=index;hr=Finish(c);}}
  else if(!candidates.empty()&&(k==VK_UP||k==VK_LEFT||k==VK_DOWN||k==VK_RIGHT||k==VK_PRIOR||k==VK_NEXT)){
   int step=(k==VK_PRIOR||k==VK_NEXT)?9:1;int dir=(k==VK_UP||k==VK_LEFT||k==VK_PRIOR)?-1:1;
   selected=static_cast<size_t>((static_cast<int>(selected)+dir*step+static_cast<int>(candidates.size())*9)%static_cast<int>(candidates.size()));hr=Request(c,candidates[selected],false);
  }else if(k==VK_F6||k==VK_F7){candidates={k==VK_F7?hj::Katakana(hj::Reading(hj::Compose(keys))):hj::Reading(hj::Compose(keys))};selected=0;hr=Request(c,candidates[0],false);}
  else if(auto j=hj::KeyJamo(k,(GetKeyState(VK_SHIFT)&0x8000)!=0)){
   if(!candidates.empty()){hr=Finish(c);if(FAILED(hr))return S_OK;}
   if(keys.size()>=256){*eaten=TRUE;return S_OK;}keys+=j;hr=Request(c,hj::Reading(hj::Compose(keys)),false);
  }
  if(SUCCEEDED(hr))*eaten=TRUE;else {keys=std::move(oldKeys);candidates=std::move(oldCandidates);selected=oldSelected;Show();}return S_OK;
 }
 HRESULT STDMETHODCALLTYPE OnCompositionTerminated(TfEditCookie,ITfComposition* comp) override{if(comp==composition){SafeRelease(composition);SafeRelease(compositionContext);Reset();}return S_OK;}
 HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr*) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr*) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE OnSetFocus(ITfDocumentMgr*,ITfDocumentMgr*) override{FinishFocus();return S_OK;}
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
 HKEY key=nullptr;std::wstring cls=ClassKey();LSTATUS error=RegCreateKeyExW(HKEY_LOCAL_MACHINE,(cls+L"\\InprocServer32").c_str(),0,nullptr,0,KEY_WRITE,nullptr,&key,nullptr);
 if(error!=ERROR_SUCCESS)return HRESULT_FROM_WIN32(error);
 error=RegSetValueExW(key,nullptr,0,REG_SZ,reinterpret_cast<const BYTE*>(path),(length+1)*sizeof(wchar_t));
 if(error==ERROR_SUCCESS)error=RegSetValueExW(key,L"ThreadingModel",0,REG_SZ,reinterpret_cast<const BYTE*>(L"Apartment"),sizeof(L"Apartment"));RegCloseKey(key);
 if(error!=ERROR_SUCCESS)return HRESULT_FROM_WIN32(error);
 HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)return init;
 ITfInputProcessorProfiles* profiles=nullptr;HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&profiles));
 if(SUCCEEDED(hr)){hr=profiles->Register(kService);if(SUCCEEDED(hr))hr=profiles->AddLanguageProfile(kService,0x0411,kProfile,kName,static_cast<ULONG>(wcslen(kName)),path,length,static_cast<ULONG>(-101));
  if(SUCCEEDED(hr))hr=profiles->EnableLanguageProfile(kService,0x0411,kProfile,TRUE);profiles->Release();}
 ITfCategoryMgr* categories=nullptr;if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,IID_ITfCategoryMgr,reinterpret_cast<void**>(&categories));
 if(SUCCEEDED(hr)){hr=categories->RegisterCategory(kService,GUID_TFCAT_TIP_KEYBOARD,kService);if(SUCCEEDED(hr))hr=categories->RegisterCategory(kService,GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,kService);categories->Release();}
 if(SUCCEEDED(init))CoUninitialize();if(FAILED(hr))RegDeleteTreeW(HKEY_LOCAL_MACHINE,cls.c_str());return hr;
}
extern "C" HRESULT WINAPI DllUnregisterServer(){
 HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)return init;
 HRESULT result=S_OK;ITfInputProcessorProfiles* p=nullptr;HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&p));
 if(SUCCEEDED(hr)){hr=p->RemoveLanguageProfile(kService,0x0411,kProfile);if(FAILED(hr))result=hr;hr=p->Unregister(kService);if(FAILED(hr))result=hr;p->Release();}else result=hr;
 ITfCategoryMgr* c=nullptr;if(SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,IID_ITfCategoryMgr,reinterpret_cast<void**>(&c)))){c->UnregisterCategory(kService,GUID_TFCAT_TIP_KEYBOARD,kService);c->UnregisterCategory(kService,GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,kService);c->Release();}
 LSTATUS error=RegDeleteTreeW(HKEY_LOCAL_MACHINE,ClassKey().c_str());if(error!=ERROR_SUCCESS&&error!=ERROR_FILE_NOT_FOUND)result=HRESULT_FROM_WIN32(error);
 if(SUCCEEDED(init))CoUninitialize();return result;
}
