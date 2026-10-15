#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <msime.h>
namespace hj {
std::wstring Compose(const std::wstring& jamo);
std::wstring Reading(const std::wstring& hangul);
wchar_t KeyJamo(UINT key, bool shift);
std::wstring Katakana(std::wstring text);
std::vector<std::wstring> KanaCandidates(const std::wstring& reading);
class Engine {
 IFELanguage* language_ = nullptr;
 HRESULT status_ = E_FAIL;
public:
 ~Engine();
 HRESULT Open();
 HRESULT Status() const { return status_; }
 std::vector<std::wstring> Convert(const std::wstring& reading);
};
}
