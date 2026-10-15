#pragma once
#include <windows.h>
#include <memory>
#include <string>
#include <vector>

namespace hj {
// Each service owns a queue. The dictionary and its COM apartment stay entirely
// on the worker; only completed candidate strings cross to the TSF UI thread.
class LiveCandidates {
 struct State;
 struct Launch;
 std::shared_ptr<State> state_;
 static DWORD WINAPI ThreadEntry(void*);
 static void Run(const std::shared_ptr<State>&);
public:
 LiveCandidates() = default;
 ~LiveCandidates();
 LiveCandidates(const LiveCandidates&) = delete;
 LiveCandidates& operator=(const LiveCandidates&) = delete;
 bool Start(HMODULE module,LONG* objectCount,HWND target,UINT message);
 void Submit(const std::wstring& reading);
 void Cancel();
 bool Take(std::vector<std::wstring>& results);
 void Stop();
};
}
