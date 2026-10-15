# 한본어 입력기 · HanBonGo

한글 두벌식으로 일본어 발음을 입력하는 Windows 입력기입니다. Windows 키보드 목록에 등록해 **Win + Space**로 전환하고, 평소 쓰는 편집창에서 사용할 수 있습니다.

```text
니홍고 → にほんご → Space → 日本語 → Enter
```

입력하는 동안 후보 창을 보여 주며, Microsoft 일본어 IME의 변환 엔진으로 한자 후보를 찾습니다. 후보 창에는 Pretendard JP를 내장했습니다.

[최신 버전 다운로드](https://github.com/MapleYoil/HanBonGo/releases/latest) · [릴리스 기록](https://github.com/MapleYoil/HanBonGo/releases) · [문제 제보](https://github.com/MapleYoil/HanBonGo/issues)

## 알려진 문제 · v0.1.5

v0.1.5는 [시험 버전](https://github.com/MapleYoil/HanBonGo/releases/tag/v0.1.5)으로 제공합니다. 안정 버전 자동 업데이트 대상에는 포함하지 않습니다.

크롬·디스코드 등에서 한본어를 선택해도 Microsoft 일본어 입력기로 전환되거나 한국어로 입력되는 문제가 보고되어 있습니다. 입력 중 후보가 나타나지 않고 Space를 누른 뒤에만 표시되는 경우도 있습니다. 등록과 전환 처리를 보완했지만, 해당 앱에서의 해결은 아직 확인하지 못했습니다.

별도 TSF 호스트의 프로세스 간 전환 검사에서 64비트는 통과했지만 32비트는 키 입력 처리기 활성화에 실패했습니다. 기본 빌드 검사의 통과가 실제 앱 호환성을 뜻하지는 않습니다.

## 설치

Windows 10/11 x64에서 사용합니다. 한자 변환에는 **Microsoft 일본어 IME**가 필요합니다. 일본어가 설치되어 있지 않으면 Windows 설정에서 일본어를 추가하고 Microsoft 입력기를 설치하세요. Visual Studio나 CMake는 필요하지 않습니다.

1. [최신 릴리스](https://github.com/MapleYoil/HanBonGo/releases/latest)에서 **HanBonGo-windows.zip**을 다운로드합니다.
2. ZIP 파일의 압축을 풉니다.
3. 압축을 푼 폴더의 **설치.cmd**를 더블클릭합니다.
4. 관리자 권한 확인 창에서 **예**를 누릅니다.
5. 설치가 끝나면 **Win + Space → 일본어 · 한본어 입력기**를 선택합니다.

입력기 아이콘은 **HB**입니다. 설치와 업데이트는 기존 기본 입력기, 언어 순서, 다른 키보드를 유지합니다. Windows 시작 시 한본어로 자동 전환하지 않습니다.

## 입력 방법

한본어 입력기를 선택하고 한글 두벌식으로 발음을 입력합니다. 본문에는 가나가 조합되고, 입력 중에도 후보 창이 나타납니다. 처음에는 가나 후보를 보여 주고 잠시 후 Microsoft 사전 후보로 갱신합니다.

입력 중에는 아무 후보도 강조하지 않고 본문은 입력한 가나를 유지합니다. **↓**를 처음 누르면 첫 후보를 강조하고, 방향키로 원하는 후보까지 이동할 수 있습니다. 방향키로 이동하는 동안 본문은 바뀌지 않으며, **Enter**로 강조한 후보를 확정합니다. 후보를 고르지 않고 Enter를 누르면 가나 그대로 확정합니다. **Space**는 강조한 후보 또는 첫 후보를 본문에 적용하며, 다시 누르면 다음 후보로 변환합니다.

| 키 | 동작 |
| --- | --- |
| Space | 강조한 후보 또는 첫 후보로 변환 / 다음 후보 |
| ↑ ↓ / ← → | 후보 강조 표시 이동 (본문은 유지) |
| PageUp / PageDown | 후보 9개씩 이동 |
| 1~9 | 현재 페이지 후보 선택·확정 |
| Enter | 강조한 후보 확정 / 강조가 없으면 현재 조합 확정 |
| Escape | 후보를 닫고 가나로 돌아가기, 한 번 더 누르면 조합 취소 |
| Backspace | 마지막 한글 자모 삭제 |
| F6 / F7 | 히라가나 / 가타카나로 변환 |
| - | 장음 기호 ー 입력 |
| Win + Space | 다른 Windows 입력기로 전환 |

미리보기 중에는 방향키로 후보를 둘러본 뒤에도 같은 조합에 이어 입력할 수 있습니다. Space로 후보를 본문에 적용한 뒤 글자를 입력하면 적용한 후보를 확정하고 새 조합을 시작합니다. 문장부호를 입력하거나 편집창을 옮기면 현재 조합을 확정하며, 방향키로 둘러보기만 한 후보는 적용하지 않습니다. `코-히-`는 `こーひー` 한 조합으로 유지됩니다.

### 발음을 입력한 그대로

가나 읽기는 문자별 규칙으로 만듭니다. 단어 프리셋이나 문구별 예외는 없으며, 장음과 조사를 자동으로 고치지 않습니다.

| 한글 입력 | 가나 읽기 |
| --- | --- |
| 니홍고 | にほんご |
| 아리가토 | ありがと |
| 아리가토우 | ありがとう |
| 곤니치와 | ごんにちわ |
| 콘니치하 | こんにちは |
| 갓코우 | がっこう |
| 쿄우 | きょう |
| 코-히- | こーひー |

가나 읽기는 입력 규칙을 따르고, 한자 후보에는 Microsoft 변환 엔진의 해석이 반영됩니다. 가나 그대로 확정하려면 변환 전에 Enter를 누르세요. 이미 변환했다면 F6를 누른 뒤 Enter로 확정합니다.

`오`의 기본 읽기는 `お`이며, 같은 발음의 `を`도 후보에서 선택할 수 있습니다. `소노스베테오`를 입력하면 `そのすべてお`, `そのすべてを`, `ソノスベテオ`, `ソノスベテヲ` 순서로 표시됩니다. **2**를 누르면 `そのすべてを`를 확정하고, 후보를 선택하지 않고 Enter를 누르면 원래 읽기인 `そのすべてお`를 확정합니다. 특정 단어를 고치는 사전 없이, 읽기의 `お`를 `を`로 바꾼 선택지를 제공합니다.

<details>
<summary>자음·모음·받침 규칙</summary>

초성 ㄱ/ㄷ/ㅂ/ㅈ은 각각 が/だ/ば/ざ 행, ㅋ·ㄲ/ㅌ·ㄸ/ㅍ·ㅃ/ㅊ은 각각 か/た/ぱ/ちゃ 행을 사용합니다. ㄴ·ㄹ·ㅁ·ㅅ·ㅇ·ㅎ은 な·ら·ま·さ·あ·は 행입니다. ㅆ은 さ 행, ㅉ은 ざ 행입니다. 단어의 위치와 관계없이 같은 규칙을 적용합니다.

ㅏ·ㅣ·ㅜ·ㅔ·ㅗ는 a·i·u·e·o입니다. 한국어 근사 발음에 따라 ㅐ→e, ㅓ→o, ㅡ→u로 처리합니다. ㅑ·ㅕ·ㅛ·ㅠ는 작은 ゃ·ょ·ょ·ゅ를 붙인 요음이며, 초성 ㅇ에서는 や·よ·よ·ゆ입니다. ㅘ·ㅝ·ㅟ는 わ·うぉ·うぃ 계열입니다. ㅒ·ㅖ·ㅙ·ㅚ·ㅞ는 e, ㅢ는 i로 처리합니다.

받침 ㄴ·ㅁ·ㅇ은 ん, ㄹ은 る, 나머지 받침은 っ입니다. 겹받침도 같은 규칙을 적용합니다. 아직 조합 중인 자모는 그대로 표시하며, 다음 모음을 입력하면 다시 조합합니다. 장음은 `오우`, `에이`처럼 직접 입력하거나 `-`로 `ー`를 입력합니다.

</details>

## 업데이트

설치하면 시작 메뉴의 **한본어 입력기** 폴더에 업데이터와 제거 바로가기를 만듭니다. 업데이터는 위쪽 화살표 모양의 트레이 아이콘으로 표시됩니다.

**트레이 아이콘을 우클릭**하면 다음 설정을 바꿀 수 있습니다.

| 설정 | 기본값 / 선택 항목 |
| --- | --- |
| Windows 시작 시 자동 실행 | 켜짐 |
| 업데이트 확인 주기 | 기본 6시간 / 30분·1시간·3시간·6시간·12시간·하루 |
| 새 업데이트 알림 | 켜짐 |
| 업데이트 자동 설치 | 꺼짐 |
| 크레딧 | 개발자와 GitHub 바로가기 |

업데이터는 실행 직후와 설정한 주기마다 [GitHub Releases](https://github.com/MapleYoil/HanBonGo/releases)를 확인합니다. 새 버전 알림을 누르면 설치를 시작하며, 설치 중 관리자 권한 확인 창이 표시될 수 있습니다. Windows 시작 시 실행을 끈 선택은 다음 업데이트에서도 유지합니다.

메뉴는 트레이 우클릭으로만 열립니다. 수동 실행이나 더블클릭으로 별도 창이 뜨지 않습니다. 메뉴의 **종료**는 업데이터만 종료하며 입력기는 계속 사용할 수 있습니다.

## 제거와 문제 제보

시작 메뉴에서 **한본어 입력기 → 한본어 입력기 제거**를 실행하고 관리자 권한 확인 창에서 **예**를 누릅니다. 사용 중인 입력기 DLL은 파일이 남아 있을 수 있습니다.

오류가 나면 안내에 표시된 단계와 오류 기록을 확인하세요. [문제 제보](https://github.com/MapleYoil/HanBonGo/issues) 시 Windows 버전, 사용한 프로그램, 입력한 발음, 기대한 결과를 함께 적어 주세요.

| 파일 | 위치 |
| --- | --- |
| 업데이터 설정 | `%LOCALAPPDATA%\HanBonGo\settings.json` |
| 업데이트 오류 기록 | `%LOCALAPPDATA%\HanBonGo\updater.log` |
| 제거 오류 기록 | `%LOCALAPPDATA%\HanBonGo\uninstall.log` |

## 작동 방식과 지원 범위

Windows TSF 입력기로 동작하며 64비트·32비트 DLL을 함께 설치합니다. 한글 자모 조합과 가나 읽기는 한본어가 처리하고, 한자 후보는 PC에 설치된 Microsoft 일본어 IME의 `IFELanguage` 엔진에 요청합니다. Microsoft 사전 파일을 복사하거나 자체 일본어 단어 사전을 배포하지 않습니다.

후보 창은 한본어에서 표시합니다. Microsoft 일본어 IME의 원래 후보 UI, 문절별 편집, 사용자 학습 기능은 포함하지 않습니다. 현재는 데스크톱 TSF 편집창을 대상으로 하며, Store/UWP 앱·게임·특수 편집기의 호환성은 별도 확인이 필요합니다. 배포 바이너리는 코드 서명되지 않았습니다.

## 소스 빌드

필요한 도구는 Visual Studio 2022 C++ 도구, Windows SDK, CMake입니다. 한자 변환 검사에는 Microsoft 일본어 IME가 필요합니다.

```powershell
cd HanBonGo
.\scripts\build.ps1
```

`build.ps1`은 x64·Win32 빌드와 검사를 실행하고 배포 ZIP을 만든 뒤 자동으로 재설치합니다. 기존 기본 입력기와 언어 순서는 유지합니다. 시스템 등록에 필요한 관리자 권한 확인 창이 표시될 수 있습니다.

<details>
<summary>개발용 명령과 검증 범위</summary>

중간 빌드만 할 때는 `build.ps1 -SkipInstall`을 사용합니다. 이미 빌드한 결과를 다시 설치하려면 일반 PowerShell에서 실행합니다.

```powershell
.\scripts\update.ps1
```

빌드 검사는 두 아키텍처의 한글 자모 조합·가나 변환과 실제 Microsoft IME의 `にほんご` → `日本語` 변환을 확인합니다. 업데이터, 기본 입력기·언어 목록 보존, 제거 절차 검사도 실행합니다. 제거 절차 검사는 계정 설정과 설치를 변경하지 않는 모의 검사입니다. 이 검사만으로 모든 앱의 입력 호환성이나 실제 제거 성공을 보장하지 않습니다.

설치 경로는 `C:\Program Files\HanBonGo\<빌드 해시>`이며, 현재 설치 정보는 `C:\Program Files\HanBonGo\active-install.json`에 기록합니다. 입력기 소스나 제품 버전이 바뀌면 COM ID와 언어 프로필 ID를 새로 생성합니다. 전체 입력기 식별자는 `ime-tool.exe --input-tip`으로 확인합니다. 같은 소스와 버전을 재설치할 때는 동일한 ID를 유지합니다.

사용자 키보드 등록에는 Windows `input.dll`의 `InstallLayoutOrTip`을 사용합니다. 업데이트 후 `EnumEnabledLayoutOrTip`과 `IsEnabledLanguageProfile`로 등록 상태를 확인하고, 기존 한본어 항목을 목록의 같은 위치에서 새 버전으로 교체합니다. 다른 키보드는 유지합니다.

등록 상태와 아이콘을 확인하는 명령입니다.

```powershell
$installed = (Get-Content 'C:\Program Files\HanBonGo\active-install.json' -Raw | ConvertFrom-Json).versionRoot
& "$installed\x64\ime-tool.exe" --user-status
& "$installed\x64\ime-tool.exe" --registration
& "$installed\x64\ime-tool.exe" --input-tip
& "$installed\x64\ime-tool.exe" --branding "$installed\x64\HangulJpIME.dll"
```

`ime-tool.exe --integration <DLL 경로>`는 TSF 문서·키 이벤트·조합 범위를 확인하는 실험용 진단이며 기본 빌드 검사에 포함하지 않습니다. 실행하면 테스트 창을 열고 해당 프로세스의 입력 언어를 전환합니다.

업데이터는 `version.json`보다 높은 안정 버전 태그의 공개 GitHub Releases에서 `HanBonGo-windows.zip`을 찾고, GitHub SHA-256 digest로 다운로드를 검증합니다. 릴리스 태그와 `version.json`의 버전을 맞춰 배포해야 합니다. 트레이 프로그램은 .NET Framework 4.8을 사용하며, 설치 작업은 숨겨진 PowerShell 도구로 수행합니다.

</details>

## 크레딧

- 개발: [MapleYoil](https://github.com/MapleYoil)
- 일본어 변환: PC에 설치된 Microsoft 일본어 IME
- 후보 창 글꼴: [Pretendard JP](https://github.com/orioncactus/pretendard), Kil Hyung-jin. [SIL Open Font License 1.1](src/resources/fonts/LICENSE.txt)

Pretendard JP는 입력기 안에 포함해 해당 프로세스에서만 사용하며 시스템 글꼴을 설치하지 않습니다. 배포 ZIP에는 `licenses/PretendardJP-LICENSE.txt`를 포함합니다.
