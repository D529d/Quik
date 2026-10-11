// ============================================================================
// 学生AI助手 —— 自解压安装器（stub · 流式解压版）
// ----------------------------------------------------------------------------
// 负载来源（自动二选一）：
//   1) 附加在自身 exe 末尾：  [stub.exe][payload.zip][Int64 长度][Int64 魔数]
//      —— 适用于负载 < 4GB 的场景，可做真正的"单文件安装包"
//   2) 同目录下的同名数据文件： <安装器名>.dat
//      —— 含大模型时负载达 6GB，超过 Windows 对可执行文件的 4GB 上限
//         （实测：总大小 3.5GB 可运行，5.93GB 会报"不是有效的应用程序"），
//         只能把负载放到外部数据文件里
//
// 解压方式：把负载文件当作流，用受限视图 (PayloadStream) 交给 ZipArchive 逐条解压。
//           全程 1MB 缓冲拷贝：不把负载读进内存（内存占用恒定）、不落地临时 zip。
//
// 用法：
//   学生AI助手_安装包.exe               正常安装：解压到 %LOCALAPPDATA%\学生AI助手，
//                                      建桌面快捷方式并启动
//   学生AI助手_安装包.exe <目标目录>     便携模式：仅解压到指定目录，
//                                      不建快捷方式、不自动启动
// ============================================================================

using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Windows.Forms;

static class Program
{
    // "SAIPAYLD"
    private const long PayloadMagic = 0x5341495041594C44L;
    private const string AppFolderName = "学生AI助手";
    private const string AppExeName = "学生AI助手.exe";
    private const int CopyBufferSize = 1 << 20; // 1MB 缓冲

    // 负载位置描述
    private sealed class PayloadSource
    {
        public string FilePath;
        public long Start;
        public long Length;
    }

    [STAThread]
    private static void Main(string[] args)
    {
        // 给了目标目录就走便携模式（只解压），否则按标准安装流程
        bool portable = args != null && args.Length > 0 && args[0].Trim().Length > 0;
        string target = portable
                            ? Path.GetFullPath(args[0].Trim())
                            : Path.Combine(
                                  Environment.GetFolderPath(
                                      Environment.SpecialFolder.LocalApplicationData),
                                  AppFolderName);

        ProgressForm ui = null;
        try
        {
            string self = Assembly.GetExecutingAssembly().Location;
            PayloadSource payload = LocatePayload(self);

            if (!portable)
            {
                ui = new ProgressForm();
                ui.Show();
                ui.Refresh();
            }

            Extract(payload, target, ui);

            if (portable)
            {
                return; // 便携模式到此结束
            }

            ui.Close();
            ui = null;

            string exe = Path.Combine(target, AppExeName);
            if (!File.Exists(exe))
            {
                throw new Exception("解压后未找到 " + AppExeName);
            }

            CreateDesktopShortcut(exe, target);

            MessageBox.Show(
                "安装完成！\n\n程序目录：\n" + target +
                "\n\n桌面已创建快捷方式「学生AI助手」。\n\n点击「确定」立即启动。",
                "学生AI助手", MessageBoxButtons.OK, MessageBoxIcon.Information);

            ProcessStartInfo psi = new ProcessStartInfo(exe);
            psi.WorkingDirectory = target;
            Process.Start(psi);
        }
        catch (Exception ex)
        {
            if (ui != null)
            {
                ui.Close();
                ui = null;
            }
            MessageBox.Show("安装失败：\n" + ex.Message,
                "学生AI助手", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            if (ui != null)
            {
                ui.Close();
            }
        }
    }

    // 定位负载：先看自身末尾是否附加了负载，否则找同目录的同名 .dat 数据文件
    private static PayloadSource LocatePayload(string selfPath)
    {
        using (FileStream fs = new FileStream(selfPath, FileMode.Open,
                                              FileAccess.Read, FileShare.Read))
        {
            long total = fs.Length;
            if (total >= 16)
            {
                byte[] tail = new byte[16];
                fs.Seek(total - 16, SeekOrigin.Begin);
                ReadExactly(fs, tail, 16);

                long len = BitConverter.ToInt64(tail, 0);
                long magic = BitConverter.ToInt64(tail, 8);
                if (magic == PayloadMagic && len > 0 && len <= total - 16)
                {
                    PayloadSource s = new PayloadSource();
                    s.FilePath = selfPath;
                    s.Start = total - 16 - len;
                    s.Length = len;
                    return s;
                }
            }
        }

        string dat = Path.Combine(
            Path.GetDirectoryName(selfPath),
            Path.GetFileNameWithoutExtension(selfPath) + ".dat");
        if (File.Exists(dat))
        {
            PayloadSource s = new PayloadSource();
            s.FilePath = dat;
            s.Start = 0;
            s.Length = new FileInfo(dat).Length;
            return s;
        }

        throw new Exception("找不到安装数据文件：" + Path.GetFileName(dat) +
                            "\n请把它与本安装器放在同一个目录下。");
    }

    // 流式解压负载到目标目录
    private static void Extract(PayloadSource payload, string target, ProgressForm ui)
    {
        string root = Path.GetFullPath(target)
                          .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string rootPrefix = root + Path.DirectorySeparatorChar;

        // 重新安装：先清空旧程序目录（对话历史在 %APPDATA%、设置在注册表，不受影响）
        if (Directory.Exists(root))
        {
            try { Directory.Delete(root, true); }
            catch { /* 目录被占用时忽略，后续按文件覆盖 */ }
        }
        Directory.CreateDirectory(root);

        using (FileStream fs = new FileStream(payload.FilePath, FileMode.Open,
                                              FileAccess.Read, FileShare.Read))
        using (PayloadStream stream = new PayloadStream(fs, payload.Start, payload.Length))
        using (ZipArchive archive = new ZipArchive(stream, ZipArchiveMode.Read, false))
        {
            long total = 0;
            foreach (ZipArchiveEntry e in archive.Entries)
            {
                total += e.Length;
            }

            byte[] buffer = new byte[CopyBufferSize];
            long done = 0;
            long lastReport = 0;

            foreach (ZipArchiveEntry entry in archive.Entries)
            {
                string dest = Path.GetFullPath(
                    Path.Combine(root, entry.FullName.Replace('/', Path.DirectorySeparatorChar)));

                // 防止 zip 内的 ../ 路径逃逸出目标目录
                if (!dest.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase))
                {
                    throw new Exception("安装包内容路径异常：" + entry.FullName);
                }

                if (entry.Name.Length == 0)
                {
                    Directory.CreateDirectory(dest); // 目录条目
                    continue;
                }

                string dir = Path.GetDirectoryName(dest);
                if (!string.IsNullOrEmpty(dir))
                {
                    Directory.CreateDirectory(dir);
                }

                if (ui != null)
                {
                    ui.SetProgress(done, total, entry.Name);
                }

                using (Stream src = entry.Open())
                using (FileStream dst = new FileStream(dest, FileMode.Create, FileAccess.Write,
                                                       FileShare.None, CopyBufferSize))
                {
                    int n;
                    while ((n = src.Read(buffer, 0, buffer.Length)) > 0)
                    {
                        dst.Write(buffer, 0, n);
                        done += n;
                        // 每 64MB 刷一次进度，避免频繁重绘拖慢解压
                        if (ui != null && done - lastReport >= (64L << 20))
                        {
                            lastReport = done;
                            ui.SetProgress(done, total, entry.Name);
                        }
                    }
                }
            }

            if (ui != null)
            {
                ui.SetProgress(total, total, "完成");
            }
        }
    }

    private static void ReadExactly(Stream s, byte[] buf, int count)
    {
        int off = 0;
        while (off < count)
        {
            int n = s.Read(buf, off, count - off);
            if (n <= 0)
            {
                throw new Exception("读取安装包数据失败");
            }
            off += n;
        }
    }

    // 通过 WScript.Shell 创建桌面快捷方式（后期绑定，无需 COM 引用）
    private static void CreateDesktopShortcut(string exePath, string workDir)
    {
        Type shellType = Type.GetTypeFromProgID("WScript.Shell");
        if (shellType == null)
        {
            return;
        }
        object shell = Activator.CreateInstance(shellType);
        string desktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
        string lnk = Path.Combine(desktop, AppFolderName + ".lnk");

        object sc = shellType.InvokeMember("CreateShortcut",
            BindingFlags.InvokeMethod, null, shell, new object[] { lnk });
        Type scType = sc.GetType();
        scType.InvokeMember("TargetPath", BindingFlags.SetProperty, null, sc,
            new object[] { exePath });
        scType.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, sc,
            new object[] { workDir });
        scType.InvokeMember("IconLocation", BindingFlags.SetProperty, null, sc,
            new object[] { exePath });
        scType.InvokeMember("Save", BindingFlags.InvokeMethod, null, sc, null);
    }
}

// 把"某个文件的一段区间"包装成可定位的只读流，供 ZipArchive 读取。
// 不复制数据、不占用额外内存，Dispose 时不关闭底层文件流。
internal sealed class PayloadStream : Stream
{
    private readonly FileStream _fs;
    private readonly long _start;
    private readonly long _length;
    private long _pos;

    public PayloadStream(FileStream fs, long start, long length)
    {
        _fs = fs;
        _start = start;
        _length = length;
        _pos = 0;
    }

    public override bool CanRead { get { return true; } }
    public override bool CanSeek { get { return true; } }
    public override bool CanWrite { get { return false; } }
    public override long Length { get { return _length; } }

    public override long Position
    {
        get { return _pos; }
        set { _pos = value; }
    }

    public override int Read(byte[] buffer, int offset, int count)
    {
        long remaining = _length - _pos;
        if (remaining <= 0)
        {
            return 0;
        }
        if (count > remaining)
        {
            count = (int)remaining;
        }
        _fs.Seek(_start + _pos, SeekOrigin.Begin);
        int n = _fs.Read(buffer, offset, count);
        _pos += n;
        return n;
    }

    public override long Seek(long offset, SeekOrigin origin)
    {
        long np;
        switch (origin)
        {
            case SeekOrigin.Begin: np = offset; break;
            case SeekOrigin.Current: np = _pos + offset; break;
            case SeekOrigin.End: np = _length + offset; break;
            default: throw new ArgumentException("origin");
        }
        if (np < 0)
        {
            throw new IOException("无效的定位位置");
        }
        _pos = np;
        return _pos;
    }

    public override void Flush() { }
    public override void SetLength(long value) { throw new NotSupportedException(); }
    public override void Write(byte[] buffer, int offset, int count) { throw new NotSupportedException(); }
    protected override void Dispose(bool disposing) { /* 底层流由调用方释放 */ }
}

// 解压进度窗口（数 GB 解压需要数分钟，必须有反馈）
internal sealed class ProgressForm : Form
{
    private readonly ProgressBar _bar;
    private readonly Label _label;

    public ProgressForm()
    {
        Text = "学生AI助手 安装程序";
        FormBorderStyle = FormBorderStyle.FixedDialog;
        StartPosition = FormStartPosition.CenterScreen;
        MaximizeBox = false;
        MinimizeBox = false;
        ClientSize = new Size(440, 120);

        _label = new Label();
        _label.Text = "正在解压安装文件，请稍候…";
        _label.SetBounds(20, 18, 400, 22);
        Controls.Add(_label);

        _bar = new ProgressBar();
        _bar.SetBounds(20, 50, 400, 22);
        _bar.Minimum = 0;
        _bar.Maximum = 1000;
        Controls.Add(_bar);
    }

    public void SetProgress(long done, long total, string current)
    {
        int percent = total > 0 ? (int)(done * 100 / total) : 0;
        if (percent > 100)
        {
            percent = 100;
        }
        _label.Text = string.Format("正在解压：{0}%   {1}", percent, current);

        int value = total > 0 ? (int)(done * 1000 / total) : 0;
        if (value > _bar.Maximum)
        {
            value = _bar.Maximum;
        }
        if (value != _bar.Value)
        {
            _bar.Value = value;
        }
        Application.DoEvents();
    }
}
