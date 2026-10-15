#include "live_candidates.h"
#include "core.h"
#include <cstdint>
#include <utility>

namespace hj {
namespace {
struct CriticalLock {
 CRITICAL_SECTION* lock;
 explicit CriticalLock(CRITICAL_SECTION& value):lock(&value){EnterCriticalSection(lock);}
 ~CriticalLock(){LeaveCriticalSection(lock);}
};
struct Apartment {
 HRESULT status=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 ~Apartment(){if(SUCCEEDED(status))CoUninitialize();}
};
}
struct LiveCandidates::State {
 CRITICAL_SECTION lock;
 HANDLE changed=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 HWND target=nullptr;
 UINT message=0;
 bool stop=false,pending=false,ready=false;
 uint64_t generation=0;
 std::wstring reading;
 std::vector<std::wstring> results;
 State(){InitializeCriticalSection(&lock);}
 ~State(){if(changed)CloseHandle(changed);DeleteCriticalSection(&lock);}
};
struct LiveCandidates::Launch {
 std::shared_ptr<State> state;
 HMODULE module=nullptr;
 LONG* objectCount=nullptr;
};
LiveCandidates::~LiveCandidates(){Stop();}
bool LiveCandidates::Start(HMODULE module,LONG* objectCount,HWND target,UINT message){
 if(state_)return true;
 auto state=std::make_shared<State>();
 if(!state->changed)return false;
 state->target=target;state->message=message;
 HMODULE pinned=nullptr;
 // The worker outlives service deactivation without making DLL unloading race
 // its engine destructor or COM teardown. No thread wait occurs on the UI thread.
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
   reinterpret_cast<LPCWSTR>(module),&pinned))return false;
 auto launch=new Launch{state,pinned,objectCount};
 InterlockedIncrement(objectCount);
 HANDLE thread=CreateThread(nullptr,0,ThreadEntry,launch,0,nullptr);
 if(!thread){InterlockedDecrement(objectCount);delete launch;FreeLibrary(pinned);return false;}
 CloseHandle(thread);state_=std::move(state);return true;
}
void LiveCandidates::Submit(const std::wstring& reading){
 auto state=state_;if(!state)return;
 CriticalLock guard(state->lock);
 ++state->generation;state->reading=reading;state->pending=true;state->ready=false;state->results.clear();
 SetEvent(state->changed);
}
void LiveCandidates::Cancel(){
 auto state=state_;if(!state)return;
 CriticalLock guard(state->lock);
 ++state->generation;state->pending=false;state->ready=false;state->results.clear();
 SetEvent(state->changed);
}
bool LiveCandidates::Take(std::vector<std::wstring>& results){
 auto state=state_;if(!state)return false;
 CriticalLock guard(state->lock);bool ready=state->ready&&!state->stop;
 if(ready){results=std::move(state->results);state->ready=false;}
 return ready;
}
void LiveCandidates::Stop(){
 auto state=std::move(state_);if(!state)return;
 CriticalLock guard(state->lock);state->stop=true;state->target=nullptr;++state->generation;
 SetEvent(state->changed);
}
void LiveCandidates::Run(const std::shared_ptr<State>& state){
 Apartment apartment;
 {
  Engine engine;
  for(;;){
   WaitForSingleObject(state->changed,INFINITE);
   // Restart the quiet period on every input. Typing never waits for IFELanguage.
   while(WaitForSingleObject(state->changed,50)==WAIT_OBJECT_0){
    CriticalLock guard(state->lock);if(state->stop)break;
   }
   bool stop,pending;uint64_t generation;std::wstring reading;
   {CriticalLock guard(state->lock);stop=state->stop;pending=state->pending;
    generation=state->generation;reading=state->reading;state->pending=false;}
   if(stop)break;if(!pending)continue;
   auto results=SUCCEEDED(apartment.status)?engine.Convert(reading):std::vector<std::wstring>{reading,Katakana(reading)};
   CriticalLock guard(state->lock);
   if(!state->stop&&state->generation==generation){
    state->results=std::move(results);state->ready=true;
    if(state->target)PostMessageW(state->target,state->message,0,0);
   }
  }
 }
}
DWORD WINAPI LiveCandidates::ThreadEntry(void* value){
 auto launch=static_cast<Launch*>(value);
 HMODULE pinned=launch->module;LONG* objectCount=launch->objectCount;
 // An unavailable dictionary or allocation failure must never take down the
 // application hosting this text service. Immediate kana candidates stay usable.
 try{Run(launch->state);}catch(...){}
 delete launch;
 InterlockedDecrement(objectCount);
 FreeLibraryAndExitThread(pinned,0);
}
}
