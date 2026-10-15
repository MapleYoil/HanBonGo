# Project workflow

The user requires every input-method update to be automatically reinstalled.
After code changes run `scripts/build.ps1`: it builds and tests x64/Win32, then
invokes `scripts/update.ps1` to install the new keyboard profile. Preserve the
user's default input method, language order, and other keyboards. Do not select
HanBonGo automatically during installation, updates, or Windows startup.
Use `-SkipInstall` only for intermediate diagnostics, not final delivery.
Do not tell the user an update is applied based only on changed registry paths.
Keep both COM and language-profile identities release-specific to avoid old
DLL/class-factory and TSF language-profile caches. Read the full input tip from
`ime-tool --input-tip`; never reconstruct it with the legacy fixed profile GUID.
Never terminate the user's editing applications or force logout to update.

Product name: 한본어 입력기 (HanBonGo). Upstream is
https://github.com/MapleYoil/HanBonGo. Repository creation and initial release are authorized. Do not push or
publish a release until the user explicitly requests it. Prepare release
packages locally. Native HanBonGo.Updater.exe runs only in the tray, checks
public GitHub Releases at startup and the configured interval, and exposes
settings/credits directly within its right-click menu; never open separate
settings or credits windows. No PowerShell updater startup entry.
