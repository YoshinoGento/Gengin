using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

// Local exhibition entry point; never reads the development project's Settings.
internal static class ExhibitionLauncher
{
    private static readonly string Root = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
    private static readonly string[] Scenarios = { "01_water", "02_pests", "03_rain", "04_expansion", "05_judging", "06_recovery" };

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            if (args.Length != 0)
            {
                if (args.Length != 2 || args[0] != "--prepare" || Array.IndexOf(Scenarios, args[1]) < 0)
                    throw new ArgumentException("Invalid exhibition scenario");
                Run(args[1], true);
                return 0;
            }
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            using (Form form = CreateMenu()) Application.Run(form);
            return 0;
        }
        catch (Exception error)
        {
            if (args.Length == 0) MessageBox.Show(error.Message, "起動できませんでした");
            else Console.Error.WriteLine(error.Message);
            return 1;
        }
    }

    private static void SafePath(string path)
    {
        string full = Path.GetFullPath(path);
        if (!full.StartsWith(Root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            throw new IOException("Path outside exhibition folder");
        for (string current = full; !String.IsNullOrEmpty(current); current = Path.GetDirectoryName(current))
            if ((File.Exists(current) || Directory.Exists(current)) &&
                (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
                throw new IOException("Linked paths are not supported");
    }

    private static void ValidateTree(string path)
    {
        SafePath(path);
        if (!Directory.Exists(path)) return;
        foreach (string entry in Directory.GetFileSystemEntries(path))
        {
            SafePath(entry);
            if (Directory.Exists(entry)) ValidateTree(entry);
        }
    }

    private static void CopyTree(string source, string destination)
    {
        SafePath(source); SafePath(destination);
        Directory.CreateDirectory(destination);
        foreach (string file in Directory.GetFiles(source))
        {
            SafePath(file);
            File.Copy(file, Path.Combine(destination, Path.GetFileName(file)), false);
        }
        foreach (string directory in Directory.GetDirectories(source))
            CopyTree(directory, Path.Combine(destination, Path.GetFileName(directory)));
    }

    private static void Run(string scenario, bool prepareOnly)
    {
        string exe = Path.Combine(Root, "SuiroNogyo.exe");
        string settings = Path.Combine(Root, "Settings");
        string lockPath = Path.Combine(Root, ".exhibition.lock");
        SafePath(exe); SafePath(lockPath);
        if (!File.Exists(exe)) throw new FileNotFoundException("SuiroNogyo.exe がありません。");
        using (FileStream mutex = new FileStream(lockPath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None))
        {
            foreach (Process other in Process.GetProcessesByName("SuiroNogyo"))
            {
                using (other)
                {
                    if (String.Equals(other.MainModule.FileName, exe, StringComparison.OrdinalIgnoreCase))
                        throw new IOException("先に、この展示版のゲームを終了してください。");
                }
            }
            if (scenario != null)
            {
                string source = Path.Combine(Root, "Scenarios", scenario, "farm");
                string backups = Path.Combine(Root, "PreviousRuns");
                ValidateTree(source); ValidateTree(settings); SafePath(backups);
                if (!File.Exists(Path.Combine(source, "farm_documents.json"))) throw new IOException("展示データが不足しています。");
                string staged = Path.Combine(Root, "settings_prepared_" + Guid.NewGuid().ToString("N"));
                CopyTree(source, Path.Combine(staged, "farm"));
                string audio = Path.Combine(settings, "runtime", "title_audio.json");
                if (File.Exists(audio))
                {
                    Directory.CreateDirectory(Path.Combine(staged, "runtime"));
                    File.Copy(audio, Path.Combine(staged, "runtime", "title_audio.json"));
                }
                string previous = null;
                if (Directory.Exists(settings))
                {
                    Directory.CreateDirectory(backups);
                    previous = Path.Combine(backups, DateTime.Now.ToString("yyyyMMdd_HHmmss") + "_" + Guid.NewGuid().ToString("N"));
                    SafePath(previous);
                    Directory.Move(settings, previous);
                }
                try { Directory.Move(staged, settings); }
                catch
                {
                    if (previous != null && !Directory.Exists(settings)) Directory.Move(previous, settings);
                    throw;
                }
            }
            if (!prepareOnly)
            {
                var start = new ProcessStartInfo(exe) { WorkingDirectory = Root, UseShellExecute = false };
                using (Process game = Process.Start(start))
                {
                    if (game == null) throw new IOException("ゲームを起動できませんでした。");
                    game.WaitForExit();
                    if (game.ExitCode != 0) throw new IOException("ゲームが終了コード " + game.ExitCode + " で終了しました。");
                }
            }
        }
    }

    private static void Begin(Form form, string scenario)
    {
        form.Hide();
        try { Run(scenario, false); }
        catch (Exception error) { MessageBox.Show(error.Message, "起動できませんでした"); }
        finally { form.Show(); form.Activate(); }
    }

    private static Form CreateMenu()
    {
        var form = new Form {
            Text = "水路農業 - ゲームシステム展示", ClientSize = new Size(620, 540),
            StartPosition = FormStartPosition.CenterScreen, FormBorderStyle = FormBorderStyle.FixedDialog,
            MaximizeBox = false, BackColor = Color.White, Font = new Font("Meiryo UI", 11), AutoScaleMode = AutoScaleMode.Dpi
        };
        form.Controls.Add(new Label { Text = "水路農業", Bounds = new Rectangle(28, 20, 560, 48), Font = new Font("Meiryo UI", 23, FontStyle.Bold) });
        form.Controls.Add(new Label { Text = "ゲームシステム展示 / 準備済みの農場", Bounds = new Rectangle(30, 76, 550, 28) });
        string[] titles = { "01  水路と栽培", "02  害虫への対処", "03  雨と取水", "04  土地拡張", "05  品評会への出品", "06  栽培の再開支援" };
        string[] details = { "1日目 / 水分と成長", "6日目 / 発見と駆除", "8日目 / 天候と水分管理", "21日目 / 25マスから30マスへ", "10日目 / 出品候補を準備済み", "1日目 / 資金・種なしからの再開" };
        for (int i = 0; i < Scenarios.Length; ++i)
        {
            string scenario = Scenarios[i];
            var button = new Button {
                Text = titles[i] + "\r\n" + details[i], Bounds = new Rectangle(30 + i % 2 * 290, 120 + i / 2 * 88, 270, 76)
            };
            button.Click += delegate { Begin(form, scenario); };
            form.Controls.Add(button);
        }
        var resume = new Button { Text = "前回の続き", Bounds = new Rectangle(30, 397, 560, 48) };
        resume.Click += delegate { Begin(form, null); };
        form.Controls.Add(resume);
        form.Controls.Add(new Label {
            Text = "各項目は準備済みの状態から開始します。\r\n以前の展示セーブは PreviousRuns に退避します。",
            Bounds = new Rectangle(30, 463, 560, 58)
        });
        return form;
    }
}
