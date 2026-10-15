#include <windows.h>
#include <msctf.h>
#include <ctfutb.h>
#include <ctffunc.h>
#include <textstor.h>
#include <olectl.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include "identity.h"
// Experimental isolated TSF text store: exercises edit locks, composition
// ranges and termination without typing into the user's apps. A test window
// supplies focus; automatic profile activation may be refused by the host OS.
class Store final:public ITextStoreACP {
 ULONG refs=1;ITextStoreACPSink* sink=nullptr;bool locked=false;
 TS_SELECTION_ACP selection{0,0,{TS_AE_NONE,FALSE}};
public:
 HWND window=CreateWindowExW(0,L"STATIC",L"HangulJpIME test",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 std::wstring text;
 ~Store(){if(sink)sink->Release();if(window)DestroyWindow(window);}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{*out=nullptr;if(id==IID_IUnknown||id==IID_ITextStoreACP){*out=static_cast<ITextStoreACP*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE AdviseSink(REFIID id,IUnknown* s,DWORD) override{if(id!=IID_ITextStoreACPSink)return E_NOINTERFACE;if(sink)return CONNECT_E_ADVISELIMIT;return s->QueryInterface(id,reinterpret_cast<void**>(&sink));}
 HRESULT STDMETHODCALLTYPE UnadviseSink(IUnknown*) override{if(sink){sink->Release();sink=nullptr;}return S_OK;}
 HRESULT STDMETHODCALLTYPE RequestLock(DWORD flags,HRESULT* result) override{if(!sink)return E_UNEXPECTED;if(locked){*result=TS_E_SYNCHRONOUS;return S_OK;}locked=true;*result=sink->OnLockGranted(flags);locked=false;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetStatus(TS_STATUS* s) override{*s={0,0};return S_OK;}
 HRESULT STDMETHODCALLTYPE QueryInsert(LONG a,LONG b,ULONG,LONG* start,LONG* end) override{*start=a;*end=b;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetSelection(ULONG index,ULONG count,TS_SELECTION_ACP* s,ULONG* fetched) override{*fetched=0;if(!locked)return TS_E_NOLOCK;if(!count)return S_OK;if(index!=TS_DEFAULT_SELECTION&&index!=0)return E_INVALIDARG;*s=selection;*fetched=1;return S_OK;}
 HRESULT STDMETHODCALLTYPE SetSelection(ULONG count,const TS_SELECTION_ACP* s) override{if(!locked)return TS_E_NOLOCK;if(count!=1)return E_INVALIDARG;selection=*s;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetText(LONG start,LONG end,WCHAR* plain,ULONG requested,ULONG* returned,TS_RUNINFO* runs,ULONG runRequested,ULONG* runReturned,LONG* next) override{
  if(!locked)return TS_E_NOLOCK;if(end==-1)end=static_cast<LONG>(text.size());if(start<0||end<start||end>static_cast<LONG>(text.size()))return TS_E_INVALIDPOS;
  ULONG count=static_cast<ULONG>(end-start);ULONG copied=std::min(count,requested);if(plain&&copied)std::copy_n(text.data()+start,copied,plain);*returned=copied;
  *runReturned=0;if(runRequested&&runs){runs[0]={requested?copied:count,TS_RT_PLAIN};*runReturned=1;}*next=start+static_cast<LONG>(requested?copied:count);return S_OK;
 }
 HRESULT STDMETHODCALLTYPE SetText(DWORD,LONG a,LONG b,const WCHAR* s,ULONG n,TS_TEXTCHANGE* change) override{
  if(!locked)return TS_E_NOLOCK;if(a<0||b<a||b>static_cast<LONG>(text.size()))return TS_E_INVALIDPOS;
  text.replace(static_cast<size_t>(a),static_cast<size_t>(b-a),s,n);*change={a,b,a+static_cast<LONG>(n)};selection.acpStart=selection.acpEnd=change->acpNewEnd;return S_OK;
 }
 HRESULT STDMETHODCALLTYPE GetFormattedText(LONG,LONG,IDataObject** p) override{*p=nullptr;return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE GetEmbedded(LONG,REFGUID,REFIID,IUnknown** p) override{*p=nullptr;return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE QueryInsertEmbedded(const GUID*,const FORMATETC*,BOOL* p) override{*p=FALSE;return S_OK;}
 HRESULT STDMETHODCALLTYPE InsertEmbedded(DWORD,LONG,LONG,IDataObject*,TS_TEXTCHANGE*) override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE InsertTextAtSelection(DWORD flags,const WCHAR* textInput,ULONG count,LONG* a,LONG* b,TS_TEXTCHANGE* change) override{
  LONG start=selection.acpStart,end=selection.acpEnd;
  if(a)*a=start;if(b)*b=flags&TS_IAS_QUERYONLY?end:start+static_cast<LONG>(count);
  if(flags&TS_IAS_QUERYONLY)return S_OK;return SetText(0,start,end,textInput,count,change);
 }
 HRESULT STDMETHODCALLTYPE InsertEmbeddedAtSelection(DWORD,IDataObject*,LONG*,LONG*,TS_TEXTCHANGE*) override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE RequestSupportedAttrs(DWORD,ULONG,const TS_ATTRID*) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE RequestAttrsAtPosition(LONG,ULONG,const TS_ATTRID*,DWORD) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE RequestAttrsTransitioningAtPosition(LONG,ULONG,const TS_ATTRID*,DWORD) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE FindNextAttrTransition(LONG,LONG halt,ULONG,const TS_ATTRID*,DWORD,LONG* next,BOOL* found,LONG* offset) override{*next=halt;*found=FALSE;*offset=0;return S_OK;}
 HRESULT STDMETHODCALLTYPE RetrieveRequestedAttrs(ULONG,TS_ATTRVAL*,ULONG* fetched) override{*fetched=0;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetEndACP(LONG* end) override{*end=static_cast<LONG>(text.size());return S_OK;}
 HRESULT STDMETHODCALLTYPE GetActiveView(TsViewCookie* view) override{*view=0;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetACPFromPoint(TsViewCookie,const POINT*,DWORD,LONG*) override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE GetTextExt(TsViewCookie,LONG,LONG,RECT*,BOOL*) override{return TS_E_NOLAYOUT;}
 HRESULT STDMETHODCALLTYPE GetScreenExt(TsViewCookie,RECT* r) override{*r={0,0,0,0};return S_OK;}
 HRESULT STDMETHODCALLTYPE GetWnd(TsViewCookie,HWND* h) override{*h=window;return S_OK;}
};
template<class T> class Ptr {
 T* p=nullptr;
public:
 ~Ptr(){if(p)p->Release();}T* operator->(){return p;}T* get(){return p;}T** out(){return &p;}
};
static void Require(HRESULT hr,const char* stage){if(FAILED(hr)){std::wcerr<<L"HRESULT 0x"<<std::hex<<hr<<L"\n";throw std::runtime_error(stage);}}
static void Pump(){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
class HostGuard {
 ITfThreadMgr* manager;HWND previous;
 CLSID previousJapanese{};GUID previousJapaneseProfile{};bool hasJapaneseDefault=false;
public:
 explicit HostGuard(ITfThreadMgr* value):manager(value),previous(GetForegroundWindow()){
  manager->AddRef();ITfInputProcessorProfiles* profiles=nullptr;
  if(SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&profiles)))){
   hasJapaneseDefault=profiles->GetDefaultLanguageProfile(0x0411,GUID_TFCAT_TIP_KEYBOARD,&previousJapanese,&previousJapaneseProfile)==S_OK;profiles->Release();
  }
 }
 ~HostGuard(){
  manager->SetFocus(nullptr);ITfInputProcessorProfileMgr* profiles=nullptr;
  if(SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(&profiles)))){
   profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0411,kService,kProfile,nullptr,TF_IPPMF_FORPROCESS);
   profiles->ReleaseInputProcessor(kService,0);profiles->Release();
  }
  manager->Deactivate();manager->Release();if(previous&&IsWindow(previous))SetForegroundWindow(previous);
  ITfInputProcessorProfiles* defaults=nullptr;
  if(hasJapaneseDefault&&SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&defaults)))){
   defaults->SetDefaultLanguageProfile(0x0411,previousJapanese,previousJapaneseProfile);defaults->Release();
  }
 }
};
int Integration(const wchar_t* path,bool externalSwitch=false){
 HMODULE dll=LoadLibraryW(path);if(!dll){std::wcerr<<L"DLL load failed\n";return 1;}int result=0;
 {
  try{
   const CLSID serviceId=kService;
   auto getFactory=reinterpret_cast<HRESULT(WINAPI*)(REFCLSID,REFIID,void**)>(GetProcAddress(dll,"DllGetClassObject"));if(!getFactory)throw std::runtime_error("Factory export missing");
   Ptr<IClassFactory> factory;Require(getFactory(serviceId,IID_IClassFactory,reinterpret_cast<void**>(factory.out())),"Factory");
   Ptr<ITfThreadMgrEx> manager;Require(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_ITfThreadMgrEx,reinterpret_cast<void**>(manager.out())),"Thread manager");
   TfClientId client;Require(manager->ActivateEx(&client,0),"Manager activation");
   HostGuard cleanup(manager.get());
   Ptr<ITfDocumentMgr> document;Require(manager->CreateDocumentMgr(document.out()),"Document");
   Ptr<ITextStoreACP> storeOwner;*storeOwner.out()=new Store;auto store=static_cast<Store*>(storeOwner.get());
   Ptr<ITfContext> context;TfEditCookie cookie;Require(document->CreateContext(client,0,storeOwner.get(),context.out(),&cookie),"Context");
   Require(document->Push(context.get()),"Push");Require(manager->SetFocus(document.get()),"Focus");
   ITfDocumentMgr* previous=nullptr;Require(manager->AssociateFocus(store->window,document.get(),&previous),"AssociateFocus");if(previous)previous->Release();
   DWORD foregroundThread=GetWindowThreadProcessId(GetForegroundWindow(),nullptr);
   BOOL attached=AttachThreadInput(GetCurrentThreadId(),foregroundThread,TRUE);
   ShowWindow(store->window,SW_SHOW);SetForegroundWindow(store->window);SetFocus(store->window);
   if(attached)AttachThreadInput(GetCurrentThreadId(),foregroundThread,FALSE);
   Ptr<ITfInputProcessorProfileMgr> profiles;Require(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(profiles.out())),"Profile manager");
   const GUID profileId=kProfile;
   Ptr<ITfInputProcessorProfiles> oldProfiles;Require(profiles->QueryInterface(IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(oldProfiles.out())),"Language profiles");
   if(externalSwitch){
    HKL korean=LoadKeyboardLayoutW(L"00000412",KLF_NOTELLSHELL);if(!korean)throw std::runtime_error("Korean keyboard layout missing");
    ActivateKeyboardLayout(korean,0);Require(oldProfiles->ChangeCurrentLanguage(0x0412),"Initial Korean input language");Pump();
    wchar_t executable[32768];GetModuleFileNameW(nullptr,executable,32768);
    std::wstring command=L"\""+std::wstring(executable)+L"\" --activate";
    STARTUPINFOW start{sizeof(start)};PROCESS_INFORMATION process{};
    if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&start,&process))throw std::runtime_error("External keyboard switch process failed");
    CloseHandle(process.hThread);DWORD began=GetTickCount();
    while(WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT&&GetTickCount()-began<5000){Pump();Sleep(10);}
    DWORD code=STILL_ACTIVE;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
    if(code!=0)throw std::runtime_error("External keyboard switch failed or timed out");
   }else{
    HKL japanese=LoadKeyboardLayoutW(L"00000411",KLF_NOTELLSHELL);if(!japanese)throw std::runtime_error("Japanese keyboard layout missing");
    ActivateKeyboardLayout(japanese,0);Require(oldProfiles->ChangeCurrentLanguage(0x0411),"Japanese language");
    HRESULT activation=profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0411,serviceId,profileId,nullptr,TF_IPPMF_FORPROCESS|TF_IPPMF_ENABLEPROFILE);
    if(activation!=S_OK){Require(activation,"Profile activation");throw std::runtime_error("Profile disabled");}
   }
   Pump();Require(manager->SetFocus(document.get()),"Refocus after language change");
   TF_INPUTPROCESSORPROFILE active{};Require(profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&active),"Active keyboard");
   if(active.clsid!=serviceId||active.guidProfile!=profileId)throw std::runtime_error("Wrong active input processor");
   Ptr<ITfKeystrokeMgr> keyboard;Require(manager->QueryInterface(IID_ITfKeystrokeMgr,reinterpret_cast<void**>(keyboard.out())),"Key manager");
   // A fresh profile is loaded asynchronously by Windows. Wait for its real
   // foreground key sink; no manually activated service or captured callbacks.
   DWORD started=GetTickCount();BOOL ready=FALSE;
   do{Pump();Require(keyboard->TestKeyDown('D',0,&ready),"Wait for native key sink");if(!ready)Sleep(10);}while(!ready&&GetTickCount()-started<3000);
   if(!ready)throw std::runtime_error("Windows did not activate the HanBonGo key sink");
   int checks=0;
   auto press=[&](UINT k,bool shouldEat=true){BOOL eaten=FALSE;Require(keyboard->TestKeyDown(k,0,&eaten),"TestKeyDown");if(!eaten)throw std::runtime_error("Test key rejected");
    Require(keyboard->KeyDown(k,0,&eaten),"KeyDown");if((eaten!=FALSE)!=shouldEat)throw std::runtime_error("Key eaten mismatch");};
   auto type=[&](const char* sequence){for(auto p=sequence;*p;++p)press(static_cast<UINT>(*p));};
   auto expect=[&](const std::wstring& wanted){++checks;if(store->text!=wanted){std::wcerr<<L"ACTUAL: "<<store->text<<L" EXPECTED: "<<wanted<<L"\n";throw std::runtime_error("Text mismatch");}};
   type("DKFLXK");expect(L"ありた");press(VK_BACK);expect(L"ありっ");press(VK_ESCAPE);expect(L"ありっ");press(VK_ESCAPE);expect(L"");
   type("SLGHDRH");expect(L"にほんご");
   std::wstring popupClass=std::wstring(L"HangulJpIME.Candidates.")+kSourceVersion;
   HWND preview=FindWindowW(popupClass.c_str(),nullptr);
   if(!preview||!IsWindowVisible(preview))throw std::runtime_error("Live preview missing before Space");++checks;
   DWORD previewStarted=GetTickCount();do{Pump();Sleep(10);}while(GetTickCount()-previewStarted<300);
   if(!IsWindowVisible(preview))throw std::runtime_error("Live preview hidden after dictionary result");++checks;
   press(VK_ESCAPE);press(VK_ESCAPE);expect(L"");
   Ptr<ITfLangBarItemMgr> items;Require(manager->QueryInterface(IID_ITfLangBarItemMgr,reinterpret_cast<void**>(items.out())),"Language bar manager");
   Ptr<ITfLangBarItem> item;HRESULT itemHr=items->GetItem(GUID_LBI_INPUTMODE,item.out());if(itemHr!=S_OK||!item.get())throw std::runtime_error("Native language bar item missing");
   TF_LANGBARITEMINFO info{};Require(item->GetInfo(&info),"Language bar information");
   if(info.clsidService!=serviceId)throw std::runtime_error("Another keyboard owns the language bar");
   Ptr<ITfLangBarItemButton> button;Require(item->QueryInterface(IID_ITfLangBarItemButton,reinterpret_cast<void**>(button.out())),"Input mode button");
   HICON icon=nullptr;Require(button->GetIcon(&icon),"HB taskbar icon");if(!icon)throw std::runtime_error("HB icon missing");DestroyIcon(icon);
   type("ZH");press(VK_OEM_MINUS);type("GL");press(VK_OEM_MINUS);expect(L"こーひー");press(VK_ESCAPE);press(VK_ESCAPE);expect(L"");
   type("DKFLRKXH");expect(L"ありがと");press(VK_RETURN);expect(L"ありがと");
   type("DND");press(VK_ESCAPE);press(VK_ESCAPE);expect(L"ありがと");
   type("SLGHDRH");expect(L"ありがとにほんご");press(VK_SPACE);expect(L"ありがと日本語");press('1');expect(L"ありがと日本語");
   type("DK");press(VK_F7);expect(L"ありがと日本語ア");press(VK_F6);expect(L"ありがと日本語あ");press(VK_ESCAPE);press(VK_ESCAPE);expect(L"ありがと日本語");
   type("DK");press(VK_OEM_PERIOD,false);expect(L"ありがと日本語あ");
   type("SK");Require(manager->SetFocus(nullptr),"Focus leave");expect(L"ありがと日本語あな");
   Require(manager->SetFocus(document.get()),"Focus return");
   type("THSHTMQPXPDH");expect(L"ありがと日本語あなそのすべてお");press(VK_DOWN);expect(L"ありがと日本語あなそのすべてお");press(VK_DOWN);expect(L"ありがと日本語あなそのすべてお");press(VK_RETURN);expect(L"ありがと日本語あなそのすべてを");
   Require(document->Pop(TF_POPF_ALL),"Pop");
   std::wcout<<(externalSwitch?L"CROSS-PROCESS TSF PASS (":L"TSF PASS (")<<checks<<L" checks)\n";
  }catch(const std::exception& e){std::wstring s;for(auto c:std::string(e.what()))s+=static_cast<wchar_t>(c);std::wcerr<<L"TSF FAIL: "<<s<<L"\n";result=1;}
 }
 auto canUnload=reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(dll,"DllCanUnloadNow"));
 DWORD stopped=GetTickCount();while(canUnload&&canUnload()!=S_OK&&GetTickCount()-stopped<3000){Pump();Sleep(10);}
 if(canUnload&&canUnload()!=S_OK){std::wcerr<<L"COM objects remain after test\n";result=1;}
 // Never unmap live COM vtables if a failed test left an active service.
 if(!result)FreeLibrary(dll);return result;
}
