// Legend of Kairi Setup (s168): the installer window. installer.py serves the page and starts this with
//   LegendOfKairiSetup.exe --url http://127.0.0.1:<port>/?t=<token> --data <WebView2 profile dir> [--title T] [--size 1280x760]
// Exit codes: 0 = window closed, 2 = bad arguments, 3 = no WebView2 runtime on this PC (installer.py then falls back).
// (s185) The taskbar button shows the install's progress: the page posts "p:<0-1000>:<run|fail|done|none>" (only when it changes),
// nothing else is accepted, and only from the top-level page of http://127.0.0.1:<port>. Nothing but that origin can load, in
// frames neither, and no request to anywhere else leaves the web view; no pop-ups, downloads, permissions, host objects or DevTools.
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

namespace KairiSetup
{
    static class Program
    {
        [STAThread]
        static int Main(string[] args)
        {
            string url = null, data = null, title = "Legend of Kairi";
            int w = 1280, h = 760;
            for (int i = 0; i + 1 < args.Length; i++)
            {
                switch (args[i])
                {
                    case "--url": url = args[++i]; break;
                    case "--data": data = args[++i]; break;
                    case "--title": title = args[++i]; break;
                    case "--size":
                        var p = args[++i].Split('x');
                        if (p.Length == 2 && int.TryParse(p[0], out var pw) && int.TryParse(p[1], out var ph)) { w = pw; h = ph; }
                        break;
                }
            }
            if (url == null || !Uri.TryCreate(url, UriKind.Absolute, out var u) || u.Scheme != Uri.UriSchemeHttp || u.Host != "127.0.0.1")
                return 2;
            try { CoreWebView2Environment.GetAvailableBrowserVersionString(); }
            catch (Exception) { return 3; }
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new HostForm(u, data, title, w, h));
            return 0;
        }
    }

    // (s185) the taskbar button's progress bar (Windows 7+), declared by hand: no extra package
    [ComImport, Guid("ea1afb91-9e28-4b86-90e9-9e9f8a5eefaf"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface ITaskbarList3
    {
        [PreserveSig] int HrInit();
        [PreserveSig] int AddTab(IntPtr hwnd);
        [PreserveSig] int DeleteTab(IntPtr hwnd);
        [PreserveSig] int ActivateTab(IntPtr hwnd);
        [PreserveSig] int SetActiveAlt(IntPtr hwnd);
        [PreserveSig] int MarkFullscreenWindow(IntPtr hwnd, [MarshalAs(UnmanagedType.Bool)] bool fullscreen);
        [PreserveSig] int SetProgressValue(IntPtr hwnd, ulong completed, ulong total);
        [PreserveSig] int SetProgressState(IntPtr hwnd, int flags);
    }

    [ComImport, Guid("56FDF344-FD6D-11d0-958A-006097C9A090"), ClassInterface(ClassInterfaceType.None)]
    class TaskbarListCls { }

    sealed class HostForm : Form
    {
        static readonly Color Night = Color.FromArgb(5, 11, 28);
        // "p:<0-1000>:<state>" and nothing else (no spaces, no leading zeros beyond one digit, at most 4 digits)
        static readonly Regex ProgressMsg = new Regex(@"^p:(0|[1-9][0-9]{0,3}):(run|fail|done|none)$", RegexOptions.CultureInvariant);
        const int TBPF_NOPROGRESS = 0, TBPF_NORMAL = 2, TBPF_ERROR = 4;
        readonly WebView2 web = new WebView2 { Dock = DockStyle.Fill, DefaultBackgroundColor = Night };
        readonly Uri start;
        readonly string data;
        readonly int lw, lh;
        readonly int msgTaskbarCreated = RegisterWindowMessage("TaskbarButtonCreated");
        ITaskbarList3 taskbar;
        int tbValue = -1;           // last progress shown (0-1000), -1 = none yet
        string tbState = "none";

        [DllImport("dwmapi.dll")]
        static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        static extern int RegisterWindowMessage(string name);
        [DllImport("user32.dll")]
        static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool FlashWindowEx(ref FLASHWINFO info);

        [StructLayout(LayoutKind.Sequential)]
        struct FLASHWINFO { public uint cbSize; public IntPtr hwnd; public uint dwFlags; public uint uCount; public uint dwTimeout; }
        const uint FLASHW_TRAY = 2, FLASHW_TIMERNOFG = 12;

        public HostForm(Uri url, string dataDir, string title, int w, int h)
        {
            start = url; data = dataDir; lw = w; lh = h;
            Text = title;
            BackColor = Night;
            StartPosition = FormStartPosition.Manual;
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch (Exception) { }
            Controls.Add(web);
            Load += OnLoadAsync;
            FormClosing += OnClosing;
            Resize += (o, e) =>
            {
                // minimized: hide the web view (its animation stops) and let WebView2 give memory back
                var c = web.CoreWebView2;
                if (c == null) return;
                bool min = WindowState == FormWindowState.Minimized;
                web.Visible = !min;
                c.MemoryUsageTargetLevel = min ? CoreWebView2MemoryUsageTargetLevel.Low : CoreWebView2MemoryUsageTargetLevel.Normal;
            };
        }

        protected override void OnHandleCreated(EventArgs e)
        {
            base.OnHandleCreated(e);
            int on = 1;   // dark title bar (Windows 10 20H1+ / 11), like the page
            DwmSetWindowAttribute(Handle, 20, ref on, sizeof(int));
        }

        protected override void WndProc(ref Message m)
        {
            base.WndProc(ref m);
            if (msgTaskbarCreated != 0 && m.Msg == msgTaskbarCreated)
            {
                // the taskbar button exists now (or again, after Explorer restarted): (re)connect and show the last state
                taskbar = null;
                ApplyTaskbar();
            }
        }

        // only http://127.0.0.1:<our port>/... ; everything else (other hosts / ports / schemes, about:, data:, file:) is refused
        bool Ours(string uri) =>
            Uri.TryCreate(uri, UriKind.Absolute, out var n) && n.Scheme == Uri.UriSchemeHttp && n.Host == start.Host && n.Port == start.Port
            && string.IsNullOrEmpty(n.UserInfo);

        async void OnLoadAsync(object sender, EventArgs e)
        {
            // logical size -> this monitor's pixels, centred on the working area
            var size = new Size(LogicalToDeviceUnits(lw), LogicalToDeviceUnits(lh));
            var wa = Screen.FromPoint(Cursor.Position).WorkingArea;
            size = new Size(Math.Min(size.Width, wa.Width), Math.Min(size.Height, wa.Height));
            ClientSize = size;
            Location = new Point(wa.Left + (wa.Width - Width) / 2, wa.Top + (wa.Height - Height) / 2);
            try
            {
                var env = await CoreWebView2Environment.CreateAsync(null, data);
                await web.EnsureCoreWebView2Async(env);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, "The window could not start its web view:\n" + ex.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
                Close();
                return;
            }
            var c = web.CoreWebView2;
            var s = c.Settings;
            s.AreDevToolsEnabled = false;
            s.AreDefaultContextMenusEnabled = false;
            s.AreBrowserAcceleratorKeysEnabled = false;
            s.IsStatusBarEnabled = false;
            s.IsZoomControlEnabled = false;
            s.IsGeneralAutofillEnabled = false;
            s.IsPasswordAutosaveEnabled = false;
            s.AreDefaultScriptDialogsEnabled = false;
            s.AreHostObjectsAllowed = false;                                           // (s185) no host objects for the page
            s.IsWebMessageEnabled = true;                                              // only for the progress messages below
            c.NewWindowRequested += (o, a) => a.Handled = true;                        // no pop-up windows
            c.NavigationStarting += (o, a) => { if (!Ours(a.Uri)) a.Cancel = true; };  // only our own page
            c.FrameNavigationStarting += (o, a) => { if (!Ours(a.Uri)) a.Cancel = true; };   // (s185) in frames too
            // (s185) a cancelled navigation can still have sent its request (seen in the s185 test: the other server got the GET), so
            // every request that is not for our origin is answered here with a 403 and never reaches the network
            var wenv = c.Environment;
            c.AddWebResourceRequestedFilter("*", CoreWebView2WebResourceContext.All);
            c.WebResourceRequested += (o, a) =>
            {
                if (!Ours(a.Request.Uri))
                    a.Response = wenv.CreateWebResourceResponse(null, 403, "Forbidden", "Content-Type: text/plain");
            };
            c.DownloadStarting += (o, a) => { a.Cancel = true; a.Handled = true; };    // (s185) nothing is downloaded
            c.PermissionRequested += (o, a) => a.State = CoreWebView2PermissionState.Deny;   // (s185) no camera / mic / location / ...
            c.WebMessageReceived += OnWebMessage;
            c.ScriptDialogOpening += OnScriptDialog;                                   // alert / confirm with our title
            c.Navigate(start.AbsoluteUri);
        }

        void OnWebMessage(object sender, CoreWebView2WebMessageReceivedEventArgs a)
        {
            // CoreWebView2.WebMessageReceived only carries the TOP-LEVEL document's messages (frames have their own event, not used);
            // the sender must still be our origin and the text must be exactly one progress message
            if (!Ours(a.Source)) return;
            string m;
            try { m = a.TryGetWebMessageAsString(); } catch (Exception) { return; }      // a non-string message is ignored
            if (m == null || m.Length > 16) return;
            var x = ProgressMsg.Match(m);
            if (!x.Success || !int.TryParse(x.Groups[1].Value, out var v) || v < 0 || v > 1000) return;
            var st = x.Groups[2].Value;
            bool nowDone = st == "done" && tbState != "done";
            tbValue = v; tbState = st;
            ApplyTaskbar();
            if (nowDone && GetForegroundWindow() != Handle)
            {
                // finished while the player looks elsewhere: flash the taskbar button until the window comes to the front
                var fi = new FLASHWINFO { cbSize = (uint)Marshal.SizeOf<FLASHWINFO>(), hwnd = Handle, dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG };
                FlashWindowEx(ref fi);
            }
        }

        void ApplyTaskbar()
        {
            if (tbValue < 0 || !IsHandleCreated) return;
            try
            {
                if (taskbar == null)
                {
                    var t = (ITaskbarList3)new TaskbarListCls();
                    if (t.HrInit() != 0) return;
                    taskbar = t;
                }
                switch (tbState)
                {
                    case "run":
                        taskbar.SetProgressState(Handle, TBPF_NORMAL);
                        taskbar.SetProgressValue(Handle, (ulong)tbValue, 1000);
                        break;
                    case "fail":     // red, at the point where it stopped
                        taskbar.SetProgressState(Handle, TBPF_ERROR);
                        taskbar.SetProgressValue(Handle, (ulong)Math.Max(tbValue, 1), 1000);
                        break;
                    default:         // setup, cancelled, done: no bar
                        taskbar.SetProgressState(Handle, TBPF_NOPROGRESS);
                        break;
                }
            }
            catch (Exception) { taskbar = null; }    // no taskbar (e.g. a shell without one): the window works the same
        }

        void OnScriptDialog(object sender, CoreWebView2ScriptDialogOpeningEventArgs a)
        {
            var d = a.GetDeferral();
            try
            {
                if (a.Kind == CoreWebView2ScriptDialogKind.Confirm)
                {
                    if (MessageBox.Show(this, a.Message, Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                        a.Accept();
                }
                else if (a.Kind == CoreWebView2ScriptDialogKind.Alert)
                {
                    MessageBox.Show(this, a.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
            finally { d.Complete(); }
        }

        void OnClosing(object sender, FormClosingEventArgs e)
        {
            // the page puts "installing" in its title while a build runs: closing stops it (it resumes next time)
            var t = web.CoreWebView2?.DocumentTitle ?? "";
            if (e.CloseReason == CloseReason.UserClosing && t.Contains("installing") &&
                MessageBox.Show(this, "Stop the install? Next time it carries on where it stopped.", Text,
                                MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
                e.Cancel = true;
        }
    }
}
