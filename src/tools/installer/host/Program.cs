// Legend of Kairi Setup (s168): the installer window. installer.py serves the page and starts this with
//   LegendOfKairiSetup.exe --url http://127.0.0.1:<port>/?t=<token> --data <WebView2 profile dir> [--title T] [--size 1280x760]
// Exit codes: 0 = window closed, 2 = bad arguments, 3 = no WebView2 runtime on this PC (installer.py then falls back).
using System;
using System.Drawing;
using System.Runtime.InteropServices;
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
            if (url == null || !Uri.TryCreate(url, UriKind.Absolute, out var u) || u.Host != "127.0.0.1")
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

    sealed class HostForm : Form
    {
        static readonly Color Night = Color.FromArgb(5, 11, 28);
        readonly WebView2 web = new WebView2 { Dock = DockStyle.Fill, DefaultBackgroundColor = Night };
        readonly Uri start;
        readonly string data;
        readonly int lw, lh;

        [DllImport("dwmapi.dll")]
        static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);

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
            c.NewWindowRequested += (o, a) => a.Handled = true;                        // no pop-up windows
            c.NavigationStarting += (o, a) =>                                          // only our own page
            {
                if (!Uri.TryCreate(a.Uri, UriKind.Absolute, out var n) || n.Host != start.Host || n.Port != start.Port)
                    a.Cancel = true;
            };
            c.ScriptDialogOpening += OnScriptDialog;                                   // alert / confirm with our title
            c.Navigate(start.AbsoluteUri);
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
