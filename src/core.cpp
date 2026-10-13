#include "core.h"
#include <algorithm>
#include <map>
namespace hj {
static const IID languageIID={0x019f7152,0xe6db,0x11d0,{0x83,0xc3,0x00,0xc0,0x4f,0xdd,0xb8,0x2e}};
static const std::wstring initials=L"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ";
static const std::wstring vowels=L"ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ";
static const std::wstring finals=L" ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ";
static int Index(const std::wstring& s,wchar_t c) { auto n=s.find(c);return n==s.npos?-1:static_cast<int>(n); }
static wchar_t JoinV(wchar_t a,wchar_t b) {
 static const std::map<std::wstring,wchar_t> pairs={{L"ㅗㅏ",L'ㅘ'},{L"ㅗㅐ",L'ㅙ'},{L"ㅗㅣ",L'ㅚ'},{L"ㅜㅓ",L'ㅝ'},{L"ㅜㅔ",L'ㅞ'},{L"ㅜㅣ",L'ㅟ'},{L"ㅡㅣ",L'ㅢ'}};
 auto it=pairs.find(std::wstring{a,b});return it==pairs.end()?0:it->second;
}
static wchar_t JoinT(wchar_t a,wchar_t b) {
 static const std::map<std::wstring,wchar_t> pairs={{L"ㄱㅅ",L'ㄳ'},{L"ㄴㅈ",L'ㄵ'},{L"ㄴㅎ",L'ㄶ'},{L"ㄹㄱ",L'ㄺ'},{L"ㄹㅁ",L'ㄻ'},{L"ㄹㅂ",L'ㄼ'},{L"ㄹㅅ",L'ㄽ'},{L"ㄹㅌ",L'ㄾ'},{L"ㄹㅍ",L'ㄿ'},{L"ㄹㅎ",L'ㅀ'},{L"ㅂㅅ",L'ㅄ'}};
 auto it=pairs.find(std::wstring{a,b});return it==pairs.end()?0:it->second;
}
wchar_t KeyJamo(UINT key,bool shift) {
 const std::wstring normal=L"ㅁㅠㅊㅇㄷㄹㅎㅗㅑㅓㅏㅣㅡㅜㅐㅔㅂㄱㄴㅅㅕㅍㅈㅌㅛㅋ";
 const std::wstring upper =L"ㅁㅠㅊㅇㄸㄹㅎㅗㅑㅓㅏㅣㅡㅜㅒㅖㅃㄲㄴㅆㅕㅍㅉㅌㅛㅋ";
 return key>='A'&&key<='Z'?(shift?upper:normal)[key-'A']:0;
}
std::wstring Compose(const std::wstring& keys) {
 std::wstring result;
 for(size_t i=0;i<keys.size();) {
  int l=Index(initials,keys[i]);
  if(l<0||i+1==keys.size()||Index(vowels,keys[i+1])<0) {result+=keys[i++];continue;}
  ++i; wchar_t v=keys[i++];
  if(i<keys.size()) if(auto joined=JoinV(v,keys[i])) {v=joined;++i;}
  int t=0;
  if(i<keys.size()&&Index(finals,keys[i])>0) {
   if(i+1==keys.size()||Index(vowels,keys[i+1])<0) {
    t=Index(finals,keys[i++]);
    if(i<keys.size()) if(auto joined=JoinT(finals[t],keys[i])) {
     if(i+1==keys.size()||Index(vowels,keys[i+1])<0) {t=Index(finals,joined);++i;}
    }
   }
  }
  result+=static_cast<wchar_t>(0xAC00+(l*21+Index(vowels,v))*28+t);
 }
 return result;
}
std::wstring Reading(const std::wstring& text) {
 // Apply only character-level pronunciation rules. Never normalize words,
 // infer vowel length, or replace Japanese particles from a phrase preset.
 const std::wstring rows[]={L"がぎぐげご",L"かきくけこ",L"なにぬねの",L"だぢづでど",L"たちつてと",L"らりるれろ",L"まみむめも",L"ばびぶべぼ",L"ぱぴぷぺぽ",L"さしすせそ",L"さしすせそ",L"あいうえお",L"ざじずぜぞ",L"ざじずぜぞ",L"ちゃちちゅちぇちょ",L"かきくけこ",L"たちつてと",L"ぱぴぷぺぽ",L"はひふへほ"};
 std::wstring out;
 for(wchar_t c:text) {
  if(c==L'-'){out+=L'ー';continue;}
  if(c<0xAC00||c>0xD7A3) {out+=c;continue;}
  int n=c-0xAC00,l=n/588,v=(n/28)%21,t=n%28;
  // a,i,u,e,o; eo->o, eu->u, ae->e are Korean approximations.
  const int vi[]={0,3,0,3,4,3,4,3,4,0,3,3,4,2,4,3,1,2,2,1,1};
  int b=vi[v];std::wstring syllable;
  if(l==14) {const wchar_t* r[]={L"ちゃ",L"ち",L"ちゅ",L"ちぇ",L"ちょ"};syllable=r[b];}
  else syllable=rows[l][b];
  bool pal=v==2||v==6||v==12||v==17;
  if(pal&&l!=11) {
   wchar_t smallKana=v==2?L'ゃ':v==17?L'ゅ':L'ょ';
   syllable=l==14?L"ち":std::wstring(1,rows[l][1]);syllable+=smallKana;
  } else if(pal&&l==11) syllable=v==2?L"や":v==17?L"ゆ":L"よ";
  else if(v==9||v==14||v==16) {
   if(l==11) syllable=v==9?L"わ":v==14?L"うぉ":L"うぃ";
   else {syllable=std::wstring(1,rows[l][2]);syllable+=v==9?L'ぁ':v==14?L'ぉ':L'ぃ';}
  }
  out+=syllable;
  if(t==4||t==16||t==21) out+=L'ん';
  else if(t==8) out+=L'る';
  else if(t) out+=L'っ';
 }
 return out;
}
std::wstring Katakana(std::wstring t) {for(auto& c:t)if(c>=0x3041&&c<=0x3096)c+=0x60;return t;}
Engine::~Engine(){if(language_){language_->Close();language_->Release();}}
HRESULT Engine::Open(){
 if(language_)return S_OK;
 const wchar_t* ids[]={L"MSIME.Japan",L"MSIME.Japan.15",L"MSIME.Japan.14"};
 for(auto id:ids){CLSID clsid;status_=CLSIDFromProgID(id,&clsid);if(FAILED(status_))continue;
  status_=CoCreateInstance(clsid,nullptr,CLSCTX_INPROC_SERVER,languageIID,reinterpret_cast<void**>(&language_));
  if(SUCCEEDED(status_)){status_=language_->Open();if(SUCCEEDED(status_))return status_;language_->Release();language_=nullptr;}
 }
 return status_;
}
std::vector<std::wstring> Engine::Convert(const std::wstring& reading){
 std::vector<std::wstring> results;
 if(SUCCEEDED(Open())&&!reading.empty()&&reading.size()<=256){
  for(int i=0;i<32;++i){MORRSLT* r=nullptr;
   status_=language_->GetJMorphResult(FELANG_REQ_CONV,FELANG_CMODE_PLAURALCLAUSE|FELANG_CMODE_MERGECAND,
     static_cast<int>(reading.size()),i?nullptr:reading.c_str(),nullptr,&r);
   if(r){if(r->pwchOutput&&r->cchOutput){std::wstring s(r->pwchOutput,r->cchOutput);if(std::find(results.begin(),results.end(),s)==results.end())results.push_back(s);}CoTaskMemFree(r);}
   if(status_!=S_OK)break;
  }
 }
 for(auto s:{reading,Katakana(reading)})if(std::find(results.begin(),results.end(),s)==results.end())results.push_back(s);
 return results;
}
}
