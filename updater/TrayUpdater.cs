using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

[assembly: System.Reflection.AssemblyTitle("한본어 입력기 업데이터")]
[assembly: System.Reflection.AssemblyProduct("HanBonGo")]
[assembly: System.Reflection.AssemblyVersion("0.1.0.0")]

namespace HanBonGo {
    sealed class Settings {
        public int IntervalMinutes { get; set; }
        public bool Notifications { get; set; }
        public bool AutoInstall { get; set; }
        public bool AutoStart { get; set; }
        public string LastNotified { get; set; }
        public Settings() { IntervalMinutes = 360; Notifications = true; AutoStart = true; LastNotified = ""; }
        public static readonly int[] Intervals = { 30, 60, 180, 360, 720, 1440 };
        public static Settings Load() {
            try { var value = Json.Read<Settings>(Paths.Settings); if (!Intervals.Contains(value.IntervalMinutes)) value.IntervalMinutes = 360; return value; }
            catch { return new Settings(); }
        }
        public void Save() { Json.Write(Paths.Settings, this); }
    }
    static class Paths {
        public static readonly string Data = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HanBonGo");
        public static readonly string Settings = Path.Combine(Data, "settings.json");
        public static readonly string Root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "HanBonGo");
        public const string Repository = "https://github.com/MapleYoil/HanBonGo";
        public static void Log(string value) { try { Directory.CreateDirectory(Data); File.AppendAllText(Path.Combine(Data, "updater.log"), DateTimeOffset.Now.ToString("o") + " " + value + Environment.NewLine); } catch { } }
    }
    static class Json {
        public static T Read<T>(string path) { return new JavaScriptSerializer().Deserialize<T>(File.ReadAllText(path)); }
        public static void Write(string path, object value) {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            string temporary = path + ".tmp"; File.WriteAllText(temporary, new JavaScriptSerializer().Serialize(value));
            if (File.Exists(path)) File.Replace(temporary, path, null); else File.Move(temporary, path);
        }
    }
    static class Startup {
        const string Name = "HanBonGoUpdater";
        public static bool Enabled { get { using (var key = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run")) return key != null && key.GetValue(Name) != null; } }
        public static void Set(bool enabled) {
            using (var key = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run")) {
                if (enabled) key.SetValue(Name, "\"" + Application.ExecutablePath + "\" --startup"); else key.DeleteValue(Name, false);
            }
        }
    }
    sealed class Manifest { public string product { get; set; } public string version { get; set; } public string repository { get; set; } }
    sealed class Release { public bool draft { get; set; } public bool prerelease { get; set; } public string tag_name { get; set; } public List<Asset> assets { get; set; } }
    sealed class Asset { public string name { get; set; } public string browser_download_url { get; set; } public string digest { get; set; } public long size { get; set; } }
    static class Updates {
        public static Version ParseVersion(string text) {
            if (String.IsNullOrEmpty(text) || !System.Text.RegularExpressions.Regex.IsMatch(text, @"^v?\d+\.\d+\.\d+(\.\d+)?$")) throw new InvalidDataException("안정 버전 번호가 올바르지 않습니다.");
            return new Version(text.TrimStart('v'));
        }
        public static Asset Select(Release release, string current) {
            if (release.draft || release.prerelease || ParseVersion(release.tag_name) <= ParseVersion(current)) return null;
            var assets = (release.assets ?? new List<Asset>()).Where(a => a.name == "HanBonGo-windows.zip").ToArray();
            if (assets.Length != 1) throw new InvalidDataException("업데이트 파일이 없습니다.");
            Asset asset = assets[0]; Uri uri;
            if (!Uri.TryCreate(asset.browser_download_url, UriKind.Absolute, out uri) || uri.Scheme != "https" || uri.Host != "github.com" || !uri.AbsolutePath.StartsWith("/MapleYoil/HanBonGo/releases/download/", StringComparison.Ordinal) || !uri.AbsolutePath.EndsWith("/HanBonGo-windows.zip", StringComparison.Ordinal)) throw new InvalidDataException("업데이트 주소가 올바르지 않습니다.");
            if (asset.digest == null || !System.Text.RegularExpressions.Regex.IsMatch(asset.digest, "^sha256:[a-fA-F0-9]{64}$") || asset.size <= 0 || asset.size > 52428800) throw new InvalidDataException("업데이트 검증 정보가 올바르지 않습니다.");
            return asset;
        }
        public static HttpClient Client() {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            var client = new HttpClient(); client.Timeout = TimeSpan.FromSeconds(30);
            client.DefaultRequestHeaders.UserAgent.ParseAdd("HanBonGo-Updater/0.1.0");
            client.DefaultRequestHeaders.Add("Accept", "application/vnd.github+json");
            client.DefaultRequestHeaders.Add("X-GitHub-Api-Version", "2026-03-10"); return client;
        }
        public static async Task<Release> Latest() {
            using (var client = Client()) using (var response = await client.GetAsync("https://api.github.com/repos/MapleYoil/HanBonGo/releases/latest")) {
                if (response.StatusCode == HttpStatusCode.NotFound) return null;
                response.EnsureSuccessStatusCode(); return new JavaScriptSerializer().Deserialize<Release>(await response.Content.ReadAsStringAsync());
            }
        }
        public static string Extract(string archive, string destination) {
            string root = Path.GetFullPath(destination).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            using (var zip = ZipFile.OpenRead(archive)) {
                long total = 0; var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach (var entry in zip.Entries) {
                    string target = Path.GetFullPath(Path.Combine(root, entry.FullName));
                    if (!target.StartsWith(root, StringComparison.OrdinalIgnoreCase) || entry.FullName.Contains(':') || ((entry.ExternalAttributes >> 16) & 0xF000) == 0xA000 || !names.Add(target)) throw new InvalidDataException("업데이트 파일 경로가 올바르지 않습니다.");
                    total += entry.Length; if (total > 104857600) throw new InvalidDataException("업데이트 파일이 너무 큽니다.");
                }
            }
            ZipFile.ExtractToDirectory(archive, destination); return destination;
        }
        public static async Task Install(Release release, Asset asset) {
            string download = Path.Combine(Paths.Data, "downloads", Guid.NewGuid().ToString("N")); Directory.CreateDirectory(download);
            string archive = Path.Combine(download, "HanBonGo-windows.zip");
            using (var client = Client()) {
                client.Timeout = TimeSpan.FromMinutes(2);
                using (var response = await client.GetAsync(asset.browser_download_url, HttpCompletionOption.ResponseHeadersRead)) {
                    response.EnsureSuccessStatusCode();
                    using (var input = await response.Content.ReadAsStreamAsync()) using (var output = File.Create(archive)) {
                        byte[] buffer = new byte[81920]; long total = 0; int read;
                        while ((read = await input.ReadAsync(buffer, 0, buffer.Length)) != 0) { total += read; if (total > 52428800) throw new InvalidDataException("업데이트 파일이 너무 큽니다."); await output.WriteAsync(buffer, 0, read); }
                    }
                }
            }
            using (var sha = SHA256.Create()) using (var input = File.OpenRead(archive)) {
                string hash = BitConverter.ToString(sha.ComputeHash(input)).Replace("-", "");
                if (!hash.Equals(asset.digest.Substring(7), StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("업데이트 파일 검증에 실패했습니다.");
            }
            string package = Extract(archive, Path.Combine(download, "package"));
            var manifest = Json.Read<Manifest>(Path.Combine(package, "version.json"));
            if (manifest.product != "HanBonGo" || manifest.repository != "MapleYoil/HanBonGo" || ParseVersion(manifest.version) != ParseVersion(release.tag_name)) throw new InvalidDataException("업데이트 정보가 일치하지 않습니다.");
            foreach (string path in new[] { @"scripts\update.ps1", @"scripts\install.ps1", @"build\x64\Release\HangulJpIME.dll", @"build\Win32\Release\HangulJpIME.dll", @"build\x64\Release\ime-tool.exe", @"build\Win32\Release\ime-tool.exe", @"build\updater\HanBonGo.Updater.exe" }) if (!File.Exists(Path.Combine(package, path))) throw new InvalidDataException("업데이트에 필요한 파일이 없습니다.");
            var start = new ProcessStartInfo("powershell.exe", "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + Path.Combine(package, @"scripts\update.ps1") + "\"");
            start.UseShellExecute = false; start.CreateNoWindow = true;
            using (var process = Process.Start(start)) { await Task.Run(() => process.WaitForExit()); if (process.ExitCode != 0) throw new IOException("업데이트 설치를 완료하지 못했습니다."); }
        }
    }
    static class TrayMenu {
        public static ContextMenuStrip Create(Func<Settings> current, Func<string> status, Action<bool> startupChanged, Action<int> intervalChanged, Action<bool> notificationsChanged, Action<bool> automaticChanged, Action exit) {
            var menu = new ContextMenuStrip { Font = new Font("Malgun Gothic", 9) };
            menu.Items.Add(new ToolStripMenuItem("한본어 입력기 업데이터") { Enabled = false }); menu.Items.Add(new ToolStripSeparator());
            var startup = new ToolStripMenuItem("Windows 시작 시 자동 실행") { CheckOnClick = true }; startup.Click += (s, e) => startupChanged(startup.Checked); menu.Items.Add(startup);
            var interval = new ToolStripMenuItem("업데이트 확인 주기");
            string[] labels = { "30분", "1시간", "3시간", "6시간", "12시간", "하루" };
            for (int i = 0; i < Settings.Intervals.Length; i++) {
                int minutes = Settings.Intervals[i]; var item = new ToolStripMenuItem(labels[i]) { Tag = minutes };
                item.Click += (s, e) => intervalChanged(minutes); interval.DropDownItems.Add(item);
            }
            menu.Items.Add(interval);
            var notify = new ToolStripMenuItem("새 업데이트 알림") { CheckOnClick = true }; notify.Click += (s, e) => notificationsChanged(notify.Checked); menu.Items.Add(notify);
            var automatic = new ToolStripMenuItem("업데이트 자동 설치") { CheckOnClick = true }; automatic.Click += (s, e) => automaticChanged(automatic.Checked); menu.Items.Add(automatic);
            menu.Items.Add(new ToolStripSeparator());
            var info = new ToolStripMenuItem { Enabled = false }; menu.Items.Add(info);
            var credits = new ToolStripMenuItem("크레딧"); credits.DropDownItems.Add(new ToolStripMenuItem("한본어 입력기 · HanBonGo") { Enabled = false });
            credits.DropDownItems.Add(new ToolStripMenuItem("개발 · MapleYoil") { Enabled = false });
            credits.DropDownItems.Add(new ToolStripMenuItem("일본어 변환 · Microsoft 일본어 IME") { Enabled = false });
            credits.DropDownItems.Add(new ToolStripSeparator());
            credits.DropDownItems.Add("github.com/MapleYoil/HanBonGo", null, (s, e) => { try { Process.Start(new ProcessStartInfo(Paths.Repository) { UseShellExecute = true }); } catch (Exception error) { Paths.Log("GitHub link failed: " + error.Message); } }); menu.Items.Add(credits);
            menu.Items.Add(new ToolStripSeparator()); menu.Items.Add("종료", null, (s, e) => exit());
            menu.Opening += (s, e) => {
                Settings settings = current(); startup.Checked = Startup.Enabled; notify.Checked = settings.Notifications; automatic.Checked = settings.AutoInstall;
                interval.Text = "업데이트 확인 주기 · " + labels[Array.IndexOf(Settings.Intervals, settings.IntervalMinutes)];
                foreach (ToolStripMenuItem item in interval.DropDownItems) item.Checked = (int)item.Tag == settings.IntervalMinutes;
                info.Text = status();
            };
            return menu;
        }
    }
    sealed class TrayContext : ApplicationContext {
        readonly NotifyIcon tray; readonly System.Windows.Forms.Timer timer; readonly Control dispatcher;
        readonly EventWaitHandle showSettings; readonly RegisteredWaitHandle showWait;
        readonly EventWaitHandle shutdown; readonly RegisteredWaitHandle shutdownWait;
        Settings settings; Release pending; Asset pendingAsset; bool busy;
        string status = "업데이트 확인을 준비하고 있습니다.";
        public TrayContext() {
            settings = Settings.Load(); dispatcher = new Control(); dispatcher.CreateControl();
            showSettings = new EventWaitHandle(false, EventResetMode.AutoReset, @"Local\HanBonGoShowSettings");
            showWait = ThreadPool.RegisterWaitForSingleObject(showSettings, (state, timeout) => { try { dispatcher.BeginInvoke((Action)OpenSettings); } catch { } }, null, Timeout.Infinite, false);
            shutdown = new EventWaitHandle(false, EventResetMode.AutoReset, @"Local\HanBonGoExit");
            shutdownWait = ThreadPool.RegisterWaitForSingleObject(shutdown, (state, timeout) => { try { dispatcher.BeginInvoke((Action)ExitThread); } catch { } }, null, Timeout.Infinite, false);
            var menu = TrayMenu.Create(() => settings, () => status,
                enabled => { bool previous = Startup.Enabled; if (!Change(next => { Startup.Set(enabled); next.AutoStart = enabled; })) try { Startup.Set(previous); } catch { } },
                minutes => Change(next => next.IntervalMinutes = minutes),
                enabled => { if (Change(next => { next.Notifications = enabled; if (enabled) next.LastNotified = ""; }) && enabled) dispatcher.BeginInvoke((Action)(async () => await Check())); },
                enabled => { if (Change(next => next.AutoInstall = enabled) && enabled && pending != null) dispatcher.BeginInvoke((Action)(async () => await Install())); },
                ExitThread);
            tray = new NotifyIcon { Icon = MakeIcon(), Text = "한본어 입력기 업데이터", ContextMenuStrip = menu, Visible = true };
            tray.DoubleClick += (s, e) => OpenSettings(); tray.BalloonTipClicked += async (s, e) => { if (pending != null && !busy) await Install(); };
            timer = new System.Windows.Forms.Timer(); timer.Interval = settings.IntervalMinutes * 60000; timer.Tick += async (s, e) => await Check(); timer.Start();
            var first = new System.Windows.Forms.Timer { Interval = 3000 }; first.Tick += async (s, e) => { first.Stop(); first.Dispose(); await Check(); }; first.Start();
            Paths.Log("Tray updater started.");
        }
        [DllImport("user32.dll")] static extern bool DestroyIcon(IntPtr hIcon);
        static Icon MakeIcon() {
            using (var bitmap = new Bitmap(32, 32)) using (var g = Graphics.FromImage(bitmap)) using (var font = new Font("Malgun Gothic", 16, FontStyle.Bold, GraphicsUnit.Pixel)) {
                g.Clear(Color.Transparent); g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
                using (var brush = new SolidBrush(Color.FromArgb(35, 62, 100))) g.FillEllipse(brush, 1, 1, 30, 30);
                g.DrawString("↑", font, Brushes.White, new RectangleF(0, 2, 32, 28), new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center });
                IntPtr h = bitmap.GetHicon(); var icon = (Icon)Icon.FromHandle(h).Clone(); DestroyIcon(h); return icon;
            }
        }
        public void OpenSettings() {
            tray.ContextMenuStrip.Show(Cursor.Position);
        }
        bool Change(Action<Settings> edit) {
            try {
                var next = new Settings { IntervalMinutes = settings.IntervalMinutes, Notifications = settings.Notifications, AutoInstall = settings.AutoInstall, AutoStart = settings.AutoStart, LastNotified = settings.LastNotified };
                edit(next); next.Save(); settings = next; timer.Stop(); timer.Interval = settings.IntervalMinutes * 60000; timer.Start(); return true;
            } catch (Exception error) { Paths.Log("Setting change failed: " + error.Message); tray.ShowBalloonTip(10000, "한본어 입력기", "설정을 저장하지 못했습니다.", ToolTipIcon.Error); return false; }
        }
        void Balloon(string title, string message, ToolTipIcon icon) { if (settings.Notifications) tray.ShowBalloonTip(10000, title, message, icon); }
        async Task Check() {
            if (busy) return; busy = true;
            try {
                var manifest = Json.Read<Manifest>(Path.Combine(Paths.Root, "version.json")); var release = await Updates.Latest();
                if (release == null) { status = "아직 공개된 업데이트가 없습니다."; Paths.Log("No published release (HTTP 404)."); return; }
                Asset asset = Updates.Select(release, manifest.version);
                if (asset == null) { pending = null; pendingAsset = null; status = "최신 버전입니다. 마지막 확인: " + DateTime.Now.ToString("HH:mm"); return; }
                pending = release; pendingAsset = asset; status = release.tag_name + " 업데이트가 있습니다.";
                if (settings.Notifications && settings.LastNotified != release.tag_name) { Balloon("한본어 입력기 업데이트", release.tag_name + " 버전이 있습니다. 알림을 누르면 설치합니다.", ToolTipIcon.Info); settings.LastNotified = release.tag_name; settings.Save(); }
            } catch (Exception e) { status = "업데이트를 확인하지 못했습니다. 다음 주기에 다시 확인합니다."; Paths.Log("Check failed: " + e.Message); }
            finally { busy = false; }
            if (settings.AutoInstall && pending != null) await Install();
        }
        async Task Install() {
            if (busy || pending == null) return; busy = true;
            try {
                status = "업데이트를 설치하고 있습니다."; await Updates.Install(pending, pendingAsset); status = "업데이트를 설치했습니다."; pending = null; pendingAsset = null; Balloon("한본어 입력기", status, ToolTipIcon.Info);
                var state = Json.Read<Dictionary<string, object>>(Path.Combine(Paths.Root, "active-install.json"));
                string next = Path.Combine((string)state["versionRoot"], @"updater\HanBonGo.Updater.exe");
                if (!Path.GetFullPath(next).Equals(Application.ExecutablePath, StringComparison.OrdinalIgnoreCase)) { Process.Start(new ProcessStartInfo(next, "--replace") { UseShellExecute = true }); ExitThread(); }
            }
            catch (Exception e) { status = "업데이트를 설치하지 못했습니다."; Paths.Log("Install failed: " + e.Message); Balloon("한본어 입력기", status + " 관리자 권한 확인을 완료했는지 확인해주세요.", ToolTipIcon.Warning); }
            finally { busy = false; }
        }
        protected override void ExitThreadCore() { timer.Stop(); timer.Dispose(); showWait.Unregister(null); shutdownWait.Unregister(null); showSettings.Dispose(); shutdown.Dispose(); tray.Visible = false; tray.Icon.Dispose(); tray.ContextMenuStrip.Dispose(); tray.Dispose(); dispatcher.Dispose(); base.ExitThreadCore(); }
    }
    static class Program {
        [STAThread] static int Main(string[] args) {
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
            if (args.Contains("--exit")) { try { using (var request = EventWaitHandle.OpenExisting(@"Local\HanBonGoExit")) request.Set(); } catch { } return 0; }
            if (args.Length >= 2 && args[0] == "--preview") {
                Directory.CreateDirectory(args[1]);
                using (var menu = TrayMenu.Create(() => new Settings(), () => "아직 공개된 업데이트가 없습니다.", enabled => { }, minutes => { }, enabled => { }, enabled => { }, () => { })) {
                    menu.Show(new Point(20, 20)); Application.DoEvents(); Render(menu, Path.Combine(args[1], "menu.png"));
                    var cadence = (ToolStripMenuItem)menu.Items[3]; cadence.ShowDropDown(); Application.DoEvents(); Render(cadence.DropDown, Path.Combine(args[1], "interval.png")); cadence.HideDropDown();
                    var credits = (ToolStripMenuItem)menu.Items[8]; credits.ShowDropDown(); Application.DoEvents(); Render(credits.DropDown, Path.Combine(args[1], "credits-menu.png")); menu.Close();
                } return 0;
            }
            if (args.Length >= 2 && args[0] == "--self-test") return SelfTest(args[1]);
            bool owned; using (var mutex = new Mutex(true, @"Local\HanBonGoTrayUpdater", out owned)) {
                if (!owned && args.Contains("--replace")) { try { owned = mutex.WaitOne(10000); } catch (AbandonedMutexException) { owned = true; } }
                if (!owned) { if (!args.Contains("--startup")) try { using (var request = EventWaitHandle.OpenExisting(@"Local\HanBonGoShowSettings")) request.Set(); } catch { } return 0; }
                try { using (var app = new TrayContext()) Application.Run(app); } catch (Exception e) { Paths.Log("Tray failure: " + e.ToString()); return 1; } finally { mutex.ReleaseMutex(); }
            } return 0;
        }
        static void Render(Control control, string path) { using (var bitmap = new Bitmap(control.Width, control.Height)) { control.DrawToBitmap(bitmap, new Rectangle(0, 0, control.Width, control.Height)); bitmap.Save(path); } }
        static int SelfTest(string report) {
            int checks = 0;
            try {
                var release = new Release { tag_name = "v0.2.0", assets = new List<Asset> { new Asset { name = "HanBonGo-windows.zip", browser_download_url = "https://github.com/MapleYoil/HanBonGo/releases/download/v0.2.0/HanBonGo-windows.zip", digest = "sha256:" + new string('a', 64), size = 1000 } } };
                if (Updates.Select(release, "0.1.0") == null) throw new Exception("new version"); checks++;
                if (Updates.Select(release, "0.2.0") != null) throw new Exception("same version"); checks++;
                release.prerelease = true; if (Updates.Select(release, "0.1.0") != null) throw new Exception("prerelease"); checks++; release.prerelease = false;
                foreach (string url in new[] { "http://github.com/MapleYoil/HanBonGo/releases/download/v0.2.0/HanBonGo-windows.zip", "https://evil.example/HanBonGo-windows.zip", "https://github.com/Other/Repo/releases/download/v0.2.0/HanBonGo-windows.zip" }) { release.assets[0].browser_download_url = url; bool rejected = false; try { Updates.Select(release, "0.1.0"); } catch (InvalidDataException) { rejected = true; } if (!rejected) throw new Exception("unsafe URL"); checks++; }
                string temporary = Path.Combine(Path.GetTempPath(), "HanBonGo-test-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(temporary);
                try { string archive = Path.Combine(temporary, "bad.zip"); using (var zip = ZipFile.Open(archive, ZipArchiveMode.Create)) zip.CreateEntry("../escape.txt"); bool rejected = false; try { Updates.Extract(archive, Path.Combine(temporary, "extract")); } catch (InvalidDataException) { rejected = true; } if (!rejected) throw new Exception("unsafe archive"); checks++; File.Delete(archive); } finally { Directory.Delete(temporary); }
                Json.Write(report, new { passed = true, checks = checks }); return 0;
            } catch (Exception e) { Json.Write(report, new { passed = false, checks = checks, error = e.ToString() }); return 1; }
        }
    }
}
