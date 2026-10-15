#include "core.h"
#include <algorithm>
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <msctf.h>
#include <shellapi.h>
#include <ctfutb.h>
#include <ctffunc.h>
#include "identity.h"
int Integration(const wchar_t* path,bool externalSwitch=false);
struct LayoutOrTipProfile {
 DWORD type;LANGID language;CLSID service;GUID profile;GUID category;
 DWORD substituteLayout;DWORD flags;WCHAR id[MAX_PATH];
};
static int UserInstall(bool install,bool verifyOnly=false){
 HMODULE input=LoadLibraryExW(L"input.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!input){std::wcerr<<L"System input.dll unavailable: "<<GetLastError()<<L"\n";return 1;}
 auto installTip=reinterpret_cast<BOOL(WINAPI*)(LPCWSTR,DWORD)>(GetProcAddress(input,"InstallLayoutOrTip"));
 auto enumerate=reinterpret_cast<UINT(WINAPI*)(LPCWSTR,LPCWSTR,LPCWSTR,LayoutOrTipProfile*,UINT)>(GetProcAddress(input,"EnumEnabledLayoutOrTip"));
 int result=1;wchar_t cls[40],profile[40];StringFromGUID2(kService,cls,40);StringFromGUID2(kProfile,profile,40);
 std::wstring tip=std::wstring(L"0x0411:")+cls+profile;
 if(installTip&&enumerate&&(verifyOnly||installTip(tip.c_str(),install?0:0x00000001))){
  if(install&&!verifyOnly){
   // Machine-wide enabled-by-default metadata is not a user enablement.
   // Explicitly enable this profile for the signing-in user without activating
   // it or changing their default keyboard.
   ITfInputProcessorProfiles* profiles=nullptr;
   HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&profiles));
   if(SUCCEEDED(hr)){hr=profiles->EnableLanguageProfile(kService,0x0411,kProfile,TRUE);profiles->Release();}
   if(hr!=S_OK){std::wcerr<<L"Enable user keyboard profile failed: 0x"<<std::hex<<hr<<L"\n";FreeLibrary(input);return 1;}
  }
  if(!install){result=0;std::wcout<<L"User keyboard uninstalled\n";}
  else {
   UINT count=enumerate(nullptr,nullptr,nullptr,nullptr,0);
   if(count>0&&count<4096){std::vector<LayoutOrTipProfile> entries(count);UINT fetched=enumerate(nullptr,nullptr,nullptr,entries.data(),count);
    bool listed=false;for(UINT i=0;i<std::min(fetched,count);++i)if(entries[i].service==kService&&entries[i].profile==kProfile)listed=true;
    ITfInputProcessorProfiles* profiles=nullptr;BOOL enabled=FALSE;
    HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&profiles));
    if(SUCCEEDED(hr)){hr=profiles->IsEnabledLanguageProfile(kService,0x0411,kProfile,&enabled);profiles->Release();}
    std::wcout<<L"User installed keyboard listed: "<<(listed?L"yes":L"no")<<L"\n"<<L"TSF user profile enabled: "<<(enabled?L"yes":L"no")<<L"\n";
    if(listed&&enabled&&SUCCEEDED(hr))result=0;
   }
  }
 }else std::wcerr<<L"InstallLayoutOrTip failed: "<<GetLastError()<<L"\n";
 FreeLibrary(input);return result;
}
int wmain(int argc,wchar_t** argv){
 _setmode(_fileno(stdout),_O_U8TEXT);_setmode(_fileno(stderr),_O_U8TEXT);
 if(argc<2){std::wcerr<<L"--test | --probe | --convert <hangul> | --register <dll> | --unregister <dll>\n";return 2;}
 std::wstring command=argv[1];
 if(command==L"--version"){wchar_t id[40];StringFromGUID2(kService,id,40);std::wcout<<id<<L"\n"<<kSourceVersion<<L"\n";return 0;}
 if(command==L"--input-tip"){wchar_t service[40],profile[40];StringFromGUID2(kService,service,40);StringFromGUID2(kProfile,profile,40);std::wcout<<L"0411:"<<service<<profile<<L"\n";return 0;}
 if(command==L"--test"){
  int failures=0,checks=0;auto check=[&](const std::wstring& actual,const std::wstring& expected){++checks;if(actual!=expected){++failures;std::wcerr<<L"FAIL: "<<actual<<L" != "<<expected<<L"\n";}};
  check(hj::Compose(L"ㄱㅗㄴㄴㅣㅊㅣㅇㅘ"),L"곤니치와");
  check(hj::Compose(L"ㄱㅏㄴㅏ"),L"가나");check(hj::Compose(L"ㄷㅏㄹㄱㅏ"),L"달가");
  check(hj::Compose(L"ㄷㅏㄹㄱ"),L"닭");check(hj::Compose(L"ㅂㅜㅅㅣ"),L"부시");
  check(hj::Reading(L"곤니치와"),L"ごんにちわ");check(hj::Reading(L"아리가토"),L"ありがと");
  check(hj::Reading(L"아리가토우"),L"ありがとう");check(hj::Reading(L"오하요"),L"おはよ");
  check(hj::Reading(L"니홍고"),L"にほんご");check(hj::Reading(L"갓코우"),L"がっこう");
  check(hj::Reading(L"쿄우"),L"きょう");check(hj::Katakana(L"にほんご"),L"ニホンゴ");
  check(std::wstring(1,hj::KeyJamo('R',false)),L"ㄱ");check(std::wstring(1,hj::KeyJamo('R',true)),L"ㄲ");
  check(hj::Reading(hj::Compose(L"ㅋㅗ-ㅎㅣ-")),L"こーひー");
  check(hj::Reading(hj::Compose(L"ㅋㅗ-ㅎㅣ")),L"こーひ");
  auto checkCandidates=[&](const std::wstring& reading,const std::vector<std::wstring>& expected){
   ++checks;auto actual=hj::KanaCandidates(reading);if(actual!=expected){++failures;std::wcerr<<L"FAIL candidate order: "<<reading<<L"\n";
    for(const auto& value:actual)std::wcerr<<L" ["<<value<<L"]";std::wcerr<<L"\n";}
  };
  check(hj::Reading(L"소노스베테오"),L"そのすべてお");
  checkCandidates(L"そのすべてお",{L"そのすべてお",L"そのすべてを",L"ソノスベテオ",L"ソノスベテヲ"});
  checkCandidates(L"あお",{L"あお",L"あを",L"アオ",L"アヲ"});
  checkCandidates(L"おお",{L"おお",L"おを",L"をお",L"をを",L"オオ",L"オヲ",L"ヲオ",L"ヲヲ"});
  checkCandidates(L"おーお",{L"おーお",L"おーを",L"をーお",L"をーを",L"オーオ",L"オーヲ",L"ヲーオ",L"ヲーヲ"});
  checkCandidates(L"そのすべてを",{L"そのすべてを",L"ソノスベテヲ"});
  checkCandidates(L"にほんご",{L"にほんご",L"ニホンゴ"});
  checkCandidates(L"ソノスベテオ",{L"ソノスベテオ"});
  checkCandidates(L"abc",{L"abc"});
  checkCandidates(L"",{L""});
  const std::wstring repeatedO(40,L'お');const auto bounded=hj::KanaCandidates(repeatedO);
  auto verify=[&](bool passed,const wchar_t* description){++checks;if(!passed){++failures;std::wcerr<<L"FAIL: "<<description<<L"\n";}};
  verify(!bounded.empty()&&bounded.front()==repeatedO,L"optional o/wo candidates preserve original reading first");
  verify(bounded.size()<=16,L"optional o/wo candidates stay bounded with many ambiguous characters");
  verify(std::count(bounded.begin(),bounded.end(),std::wstring(40,L'を'))==1,L"bounded candidates retain all-wo option exactly once");
  for(size_t i=0;i<bounded.size();++i)verify(std::find(bounded.begin(),bounded.begin()+i,bounded[i])==bounded.begin()+i,L"optional o/wo candidates contain no duplicates");
  std::wcout<<(failures?L"FAILED":L"PASS")<<L" ("<<checks<<L" checks)\n";return failures?1:0;
 }
 HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(init)){std::wcerr<<L"COM: "<<std::hex<<init<<L"\n";return 1;}
 int exitCode=0;
 if(command==L"--registration"){
  ITfInputProcessorProfiles* profiles=nullptr;BSTR description=nullptr;
  HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&profiles));
  if(SUCCEEDED(hr))hr=profiles->GetLanguageProfileDescription(kService,0x0411,kProfile,&description);
  bool named=hr==S_OK&&description&&std::wstring(description)==L"한본어 입력기";
  std::wcout<<L"Windows profile name: "<<(named?description:L"unavailable")<<L"\n";
  if(description)SysFreeString(description);if(profiles)profiles->Release();
  ITfTextInputProcessorEx* service=nullptr;hr=CoCreateInstance(kService,nullptr,CLSCTX_INPROC_SERVER,IID_ITfTextInputProcessorEx,reinterpret_cast<void**>(&service));
  bool loadable=hr==S_OK&&service;std::wcout<<L"Registered input processor loads: "<<(loadable?L"yes":L"no")<<L" (0x"<<std::hex<<hr<<L")\n";
  if(service)service->Release();exitCode=named&&loadable?0:1;
 }else if(command==L"--branding"){
  if(argc<3)exitCode=2;else {
   HICON largeIcon=nullptr,smallIcon=nullptr;UINT extracted=ExtractIconExW(argv[2],-101,&largeIcon,&smallIcon,1);
   bool icons=extracted>0&&extracted!=UINT_MAX&&largeIcon&&smallIcon;std::wcout<<L"Embedded HB branding icons: "<<(icons?L"yes":L"no")<<L"\n";
   if(largeIcon)DestroyIcon(largeIcon);if(smallIcon)DestroyIcon(smallIcon);
   ITfInputProcessorProfileMgr* profiles=nullptr;HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(&profiles));
   bool compatible=false;if(SUCCEEDED(hr)){TF_INPUTPROCESSORPROFILE profile{};hr=profiles->GetProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0411,kService,kProfile,nullptr,&profile);compatible=SUCCEEDED(hr)&&(profile.dwCaps&TF_IPP_CAPS_SYSTRAYSUPPORT)&&(profile.dwCaps&TF_IPP_CAPS_IMMERSIVESUPPORT);profiles->Release();}
   std::wcout<<L"Taskbar branding capability: "<<(compatible?L"yes":L"no")<<L"\n";exitCode=icons&&compatible?0:1;
  }
 }else if(command==L"--refresh-list"){
  DWORD_PTR result=0;
  SendMessageTimeoutW(HWND_BROADCAST,WM_SETTINGCHANGE,0,reinterpret_cast<LPARAM>(L"intl"),SMTO_ABORTIFHUNG,1000,&result);
  SendMessageTimeoutW(HWND_BROADCAST,WM_SETTINGCHANGE,0,reinterpret_cast<LPARAM>(L"Keyboard Layout"),SMTO_ABORTIFHUNG,1000,&result);
  std::wcout<<L"Keyboard list change broadcast sent\n";
 }else if(command==L"--isolation"){
  ITfThreadMgr* manager=nullptr;TfClientId client=TF_CLIENTID_NULL;ITfInputProcessorProfileMgr* profiles=nullptr;ITfKeyEventSink* keys=nullptr;
  HRESULT hr=CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_ITfThreadMgr,reinterpret_cast<void**>(&manager));
  if(SUCCEEDED(hr))hr=manager->Activate(&client);
  if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(&profiles));
  if(SUCCEEDED(hr))hr=profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0411,kService,kProfile,nullptr,TF_IPPMF_FORPROCESS|TF_IPPMF_ENABLEPROFILE);
  if(SUCCEEDED(hr))hr=CoCreateInstance(kService,nullptr,CLSCTX_INPROC_SERVER,IID_ITfKeyEventSink,reinterpret_cast<void**>(&keys));
  BOOL eaten=FALSE;if(SUCCEEDED(hr)){keys->OnSetFocus(TRUE);hr=keys->OnTestKeyDown(nullptr,'A',0,&eaten);if(SUCCEEDED(hr)&&!eaten)hr=E_FAIL;}
  // Exercise stale callbacks after a real process-local switch to installed Meltype.
  const CLSID other={0x417d801b,0xa9bd,0x4c26,{0xbd,0x16,0x35,0x6a,0x82,0x5a,0x69,0x98}};
  const GUID otherProfile={0x21f643f4,0x72d5,0x4946,{0xbf,0x70,0x0a,0x12,0x6a,0xb5,0x2d,0x05}};
  if(SUCCEEDED(hr))hr=profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0411,other,otherProfile,nullptr,TF_IPPMF_FORPROCESS|TF_IPPMF_ENABLEPROFILE);
  if(SUCCEEDED(hr)){eaten=TRUE;hr=keys->OnTestKeyDown(nullptr,'A',0,&eaten);if(SUCCEEDED(hr)&&eaten)hr=E_FAIL;}
  if(SUCCEEDED(hr)){eaten=TRUE;hr=keys->OnKeyDown(nullptr,'A',0,&eaten);if(SUCCEEDED(hr)&&eaten)hr=E_FAIL;}
  std::wcout<<L"HanBonGo rejects keys while Meltype is active: "<<(SUCCEEDED(hr)?L"yes":L"no")<<L" (0x"<<std::hex<<hr<<L")\n";exitCode=SUCCEEDED(hr)?0:1;
  if(keys)keys->Release();if(profiles)profiles->Release();if(manager){if(client!=TF_CLIENTID_NULL)manager->Deactivate();manager->Release();}
 }else if(command==L"--langbar"){
  ITfThreadMgr* manager=nullptr;TfClientId client=TF_CLIENTID_NULL;
  HRESULT hr=CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_ITfThreadMgr,reinterpret_cast<void**>(&manager));
  if(SUCCEEDED(hr))hr=manager->Activate(&client);
  ITfInputProcessorProfileMgr* activeProfiles=nullptr;
  if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(&activeProfiles));
  if(SUCCEEDED(hr))hr=activeProfiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0411,kService,kProfile,nullptr,TF_IPPMF_FORPROCESS|TF_IPPMF_ENABLEPROFILE);
  if(activeProfiles)activeProfiles->Release();
  ITfLangBarItemMgr* items=nullptr;ITfLangBarItem* item=nullptr;ITfLangBarItemButton* button=nullptr;
  if(SUCCEEDED(hr))hr=manager->QueryInterface(IID_ITfLangBarItemMgr,reinterpret_cast<void**>(&items));
  if(SUCCEEDED(hr)){hr=items->GetItem(GUID_LBI_INPUTMODE,&item);if(SUCCEEDED(hr)&&!item)hr=E_FAIL;}
  TF_LANGBARITEMINFO info{};if(SUCCEEDED(hr))hr=item->GetInfo(&info);
  if(SUCCEEDED(hr)&&info.clsidService!=kService)hr=E_FAIL;
  if(SUCCEEDED(hr))hr=item->QueryInterface(IID_ITfLangBarItemButton,reinterpret_cast<void**>(&button));
  HICON icon=nullptr;if(SUCCEEDED(hr)&&button)hr=button->GetIcon(&icon);
  std::wcout<<L"Live HanBonGo input-mode item: "<<(SUCCEEDED(hr)&&icon?L"yes":L"no")<<L" (0x"<<std::hex<<hr<<L")\n";
  exitCode=SUCCEEDED(hr)&&icon?0:1;if(icon)DestroyIcon(icon);if(button)button->Release();if(item)item->Release();if(items)items->Release();
  if(manager){if(client!=TF_CLIENTID_NULL)manager->Deactivate();manager->Release();}
 }else if(command==L"--user-install"||command==L"--user-uninstall"||command==L"--user-status"){
  exitCode=UserInstall(command!=L"--user-uninstall",command==L"--user-status");
 }else if(command==L"--default-profile"){
  wchar_t* end=nullptr;unsigned long language=argc==3?wcstoul(argv[2],&end,16):0;
  if(argc!=3||!language||language>0xffff||end==argv[2]||*end)exitCode=2;
  else {
   ITfInputProcessorProfiles* profiles=nullptr;CLSID service{};GUID profile{};
   HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&profiles));
   if(SUCCEEDED(hr)){hr=profiles->GetDefaultLanguageProfile(static_cast<LANGID>(language),GUID_TFCAT_TIP_KEYBOARD,&service,&profile);profiles->Release();}
   if(hr==S_OK){wchar_t serviceId[40],profileId[40];StringFromGUID2(service,serviceId,40);StringFromGUID2(profile,profileId,40);std::wcout<<serviceId<<L"\n"<<profileId<<L"\n";}
   else std::wcerr<<L"Default keyboard profile: 0x"<<std::hex<<hr<<L"\n";
   exitCode=hr==S_OK?0:1;
  }
 }else if(command==L"--status"){
  ITfInputProcessorProfileMgr* profiles=nullptr;HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(&profiles));
  if(SUCCEEDED(hr)){TF_INPUTPROCESSORPROFILE active{};hr=profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&active);if(SUCCEEDED(hr)){wchar_t id[40];StringFromGUID2(active.clsid,id,40);std::wcout<<L"Active keyboard: "<<id<<L"\n";if(active.clsid!=kService)hr=E_FAIL;}profiles->Release();}
  std::wcout<<L"Current build active: "<<(hr==S_OK?L"yes":L"no")<<L"\n";exitCode=hr==S_OK?0:1;
 }else if(command==L"--activate"||command==L"--deactivate"){
  ITfInputProcessorProfileMgr* profiles=nullptr;
  HRESULT hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfileMgr,reinterpret_cast<void**>(&profiles));
  if(SUCCEEDED(hr)){
   if(command==L"--activate"){
    ITfInputProcessorProfiles* languages=nullptr;
    hr=profiles->QueryInterface(IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&languages));
    if(SUCCEEDED(hr)){
     hr=languages->EnableLanguageProfile(kService,0x0411,kProfile,TRUE);
     // Selecting the keyboard must preserve the user's language default.
     if(SUCCEEDED(hr))hr=languages->ChangeCurrentLanguage(0x0411);
     languages->Release();
    }
    if(SUCCEEDED(hr)){
     // The HKL request can select the language's default IME. Send it first;
     // the explicit profile activation below must be the final switch.
     HWND foreground=GetForegroundWindow();HKL japanese=LoadKeyboardLayoutW(L"00000411",KLF_NOTELLSHELL);
     if(foreground&&japanese){DWORD_PTR messageResult=0;
      SendMessageTimeoutW(foreground,WM_INPUTLANGCHANGEREQUEST,0,reinterpret_cast<LPARAM>(japanese),SMTO_ABORTIFHUNG,1000,&messageResult);
     }
    }
    if(SUCCEEDED(hr))hr=profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0411,kService,kProfile,nullptr,TF_IPPMF_FORSESSION|TF_IPPMF_ENABLEPROFILE);
   }else hr=profiles->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,0x0411,kService,kProfile,nullptr,TF_IPPMF_FORSESSION);
   if(command==L"--activate"&&hr==S_OK){TF_INPUTPROCESSORPROFILE active{};hr=profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&active);if(SUCCEEDED(hr)&&active.clsid!=kService)hr=E_FAIL;}
   profiles->Release();
  }
  std::wcout<<command<<L": 0x"<<std::hex<<hr<<L"\n";exitCode=hr==S_OK?0:1;
 }else if(command==L"--integration"||command==L"--integration-session"){
  exitCode=argc<3?2:Integration(argv[2],command==L"--integration-session");
 }else if(command==L"--register"||command==L"--unregister"){
  if(argc<3)exitCode=2;else {HMODULE dll=LoadLibraryW(argv[2]);if(!dll){std::wcerr<<L"LoadLibrary: "<<GetLastError()<<L"\n";exitCode=1;}else{
   auto function=reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(dll,command==L"--register"?"DllRegisterServer":"DllUnregisterServer"));HRESULT hr=function?function():E_NOINTERFACE;
   std::wcout<<command<<L": 0x"<<std::hex<<hr<<L"\n";exitCode=FAILED(hr)?1:0;FreeLibrary(dll);}}
 }else if(command==L"--probe"||command==L"--convert"){
  {hj::Engine engine;HRESULT hr=engine.Open();std::wcout<<L"Microsoft IME Open: 0x"<<std::hex<<hr<<L"\n";
   if(FAILED(hr))exitCode=1;else {
    std::wstring reading=command==L"--probe"?L"にほんご":argc>2?hj::Reading(argv[2]):L"";
    if(reading.empty())exitCode=2;else {auto candidates=engine.Convert(reading);std::wcout<<L"Reading: "<<reading<<L"\n";bool found=false;
     for(auto& s:candidates){std::wcout<<s<<L"\n";if(s==L"日本語")found=true;}
     if(command==L"--probe"&&!found){std::wcerr<<L"Japanese dictionary conversion did not return 日本語\n";exitCode=1;}
    }
   }}
 }else exitCode=2;
 CoUninitialize();return exitCode;
}
