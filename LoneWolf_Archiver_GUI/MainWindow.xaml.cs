using ByteSizeLib;
using Microsoft.Win32;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Globalization;
using System.IO.Pipes;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Windows.Media.Animation;
using Newtonsoft.Json;

namespace LoneWolf_Archiver_GUI
{
    public class JsonClient : IDisposable
    {
        private bool _disposed = false;
        private readonly TcpClient _tcpClient = new TcpClient();
        private readonly NetworkStream _netStream;
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        private void Dispose(bool disposing)
        {
            if (_disposed) return;
            if (disposing)
            {
                SendMsg("bye");
                RecvAndCheck("bye");
                _tcpClient.Dispose();
            }
            _disposed = true;
        }

        ~JsonClient()
        {
            Dispose(false);
        }
        public JsonClient(int port)
        {
            _tcpClient.Connect(IPAddress.Loopback, port);
            _netStream = _tcpClient.GetStream();
            SendMsg("hello");
            RecvAndCheck("hello");
        }

        public JObject RecvAndCheck(string expectedMsg)
        {
            var r = RecvMsg();
            if ((string)r["msg"] != expectedMsg)
            {
                MessageBox.Show((string)r["msg"]);
                return null;
            }
            else
            {
                return r;
            }
        }

        private void SendMsg(JObject msg)
        {
            byte[] bytes = Encoding.UTF8.GetBytes(msg.ToString(Formatting.None));
            Int32 len = bytes.Length;

            using (var writer = new BinaryWriter(_netStream, Encoding.UTF8, true))
            {
                writer.Write(IPAddress.HostToNetworkOrder(len));
                writer.Write(bytes);
            }
        }
        public void SendMsg(string msg)
        {
            var root = new JObject { ["msg"] = msg };
            SendMsg(root);
        }
        public void SendMsg(string msg, JObject param)
        {
            var root = new JObject { ["msg"] = msg, ["param"] = param };
            SendMsg(root);
        }

        private JObject RecvMsg()
        {
            using (var reader = new BinaryReader(_netStream, Encoding.UTF8, true))
            {
                var len = IPAddress.NetworkToHostOrder(reader.ReadInt32());
                var buf = reader.ReadBytes(len);
                return JObject.Parse(Encoding.UTF8.GetString(buf));
            }
        }
    }
    /// <summary>
    /// MainWindow.xaml 的交互逻辑
    /// </summary>
    public sealed partial class MainWindow : Window, IDisposable
    {
        const string PortnumFile = "archiver_server_port.tmp";
        private bool _disposed = false;
        private JsonClient _client;
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        private void Dispose(bool disposing)
        {
            if (_disposed) return;
            if (disposing)
            {
                _client.Dispose();
                File.Delete(PortnumFile);
            }
            _disposed = true;
        }

        ~MainWindow()
        {
            Dispose(false);
        }

        private readonly Treeitem _treeroot = new Treeitem { Alias = "", Title = "Root" };
        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            // Configure the process using the StartInfo properties.
            Process process = new Process
            {
                StartInfo = {
                FileName = @"LoneWolf_Archiver.exe",
                Arguments = @"--server",
                WindowStyle = ProcessWindowStyle.Minimized
            }
            };

            File.Delete(PortnumFile);
            process.Start();
            do
            {
                Thread.Sleep(250);
            } while (!File.Exists(PortnumFile));
            var port = int.Parse(File.ReadAllText(PortnumFile));
            _client = new JsonClient(port);

            filetree.Items.Add(_treeroot);
        }
        public class Treeitem
        {
            public class File
            {
                public string Filename { get; set; }

                private ByteSize _sizeUncompressed;
                public string SizeUncompressed
                {
                    get => _sizeUncompressed.ToString();
                    set => _sizeUncompressed = ByteSize.FromBytes(uint.Parse(value));
                }

                private ByteSize _sizeCompressed;
                public string SizeCompressed
                {
                    get => _sizeCompressed.ToString();
                    set => _sizeCompressed = ByteSize.FromBytes(uint.Parse(value));
                }

                private enum CmEnum
                {
                    Uncompressed,
                    DecompressDuringRead,
                    DecompressAllAtOnce
                }
                private CmEnum _compressMethod;

                public string CompressMethod
                {
                    get
                    {
                        switch (_compressMethod)
                        {
                            case CmEnum.Uncompressed:
                                return "Uncompressed";
                            case CmEnum.DecompressDuringRead:
                                return "Decompress During Read";
                            case CmEnum.DecompressAllAtOnce:
                                return "Decompress All At Once";
                            default:
                                return "Unknown Compress Method";
                        }
                    }
                    set
                    {
                        switch (value)
                        {
                            case "store":
                                _compressMethod = CmEnum.Uncompressed;
                                break;
                            case "compress_stream":
                                _compressMethod = CmEnum.DecompressDuringRead;
                                break;
                            case "compress_buffer":
                                _compressMethod = CmEnum.DecompressAllAtOnce;
                                break;
                            default:
                                throw new ArgumentOutOfRangeException();
                        }
                    }
                }

                private DateTime _modificationDate;
                public string ModificationDate
                {
                    get => _modificationDate.ToString(CultureInfo.CurrentCulture);
                    set
                    {
                        _modificationDate = new DateTime(1970, 1, 1);
                        _modificationDate = _modificationDate.AddSeconds(uint.Parse(value));
                    }
                }
            }

            /***************************************/
            public Treeitem()
            {
                Items = new ObservableCollection<Treeitem>();
                Files = new ObservableCollection<File>();
            }

            public string Title { get; set; }

            private string _alias;
            public string Alias
            {
                get
                {
                    if (_alias is null || _alias == "") return "";
                    else return " (" + _alias + ")";
                }
                set => _alias = value;
            }
            public ObservableCollection<Treeitem> Items { get; }
            public ObservableCollection<File> Files { get; }
        }

        private void filetree_SelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
        {
            filegrid.ItemsSource = ((Treeitem)filetree.SelectedItem).Files;
        }

        private void filegrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            //MessageBox.Show(((file)filegrid.SelectedItem).filename);
        }

        private void MenuItem_Click(object sender, RoutedEventArgs e)
        {

        }

        private void MenuItem_Click_1(object sender, RoutedEventArgs e)
        {

        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            _client.Dispose();
        }

        private Treeitem parseFolderJson(JToken folder)
        {
            var folderitem = new Treeitem
            {
                Title = Path.GetFileName(
                    ((string)folder["path"]).TrimEnd('\\', '/'))
            };
            foreach (var subfolder in folder["subfolders"])
            {
                folderitem.Items.Add(parseFolderJson(subfolder));
            }
            foreach (var file in folder["files"])
            {
                var f = new Treeitem.File
                {
                    CompressMethod = (string)file["storage"],
                    Filename = (string)file["name"],
                    ModificationDate = (string)file["date"],
                    SizeCompressed = (string)file["compressedlen"],
                    SizeUncompressed = (string)file["decompressedlen"]
                };

                folderitem.Files.Add(f);
            }
            return folderitem;
        }
        private Treeitem ParseTocJson(JToken toc)
        {
            var tocitem = parseFolderJson(toc["tocfolder"]);
            tocitem.Title = (string)toc["name"];
            tocitem.Alias = (string)toc["alias"];
            return tocitem;
        }
        private void MenuOpen_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Filter = "Big文件(*.big)|*.big|所有文件(*.*)|*.*",
                FilterIndex = 1,
                Title = "打开Big文件",
                CheckFileExists = true,
                CheckPathExists = true,
                ValidateNames = true,
                DereferenceLinks = true
            };
            if (dialog.ShowDialog() == true)
            {
                filegrid.ItemsSource = null;

                var openparam = new JObject { ["path"] = dialog.FileName };
                _client.SendMsg("open", openparam);
                if (_client.RecvAndCheck("ok") is null) return;
                _client.SendMsg("get_filetree");
                _treeroot.Items.Clear();

                var response = _client.RecvAndCheck("filetree");
                if (response is null) return;
                var jsonFiletree = response["param"];
                var name = (string)jsonFiletree["name"];
                foreach (var toc in jsonFiletree["tocs"])
                {
                    _treeroot.Items.Add(ParseTocJson(toc));
                }
            }
        }

        private void MenuExtract_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new System.Windows.Forms.FolderBrowserDialog
            {
                Description = @"请选择解压路径",
                ShowNewFolderButton = true
            };

            /*if (dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            {
                DateTime begTime = DateTime.Now;
                sendMsg("extract-big");
                sendMsg(dialog.SelectedPath);
                var progress = new ExtractProgress();
                progress.init(uint.Parse(recvMsg()));
                progress.Show();
                progress.Topmost = true;

                var worker = new BackgroundWorker();
                worker.DoWork += delegate (object o, DoWorkEventArgs args)
                {
                    string s = recvMsg();
                    while (s != "finished")
                    {
                        string s1 = s;
                        string s2 = recvMsg();
                        string s3 = recvMsg();
                        progress.Dispatcher.BeginInvoke(
                            new Action(() =>
                            {
                                progress.Update(
                                    uint.Parse(s1),
                                    uint.Parse(s2),
                                    uint.Parse(s3)
                                );
                            })
                        );

                        s = recvMsg();
                    }
                };
                worker.RunWorkerCompleted += delegate (object o, RunWorkerCompletedEventArgs args)
                {
                    progress.Topmost = false;
                    DateTime endTime = DateTime.Now;
                    MessageBox.Show("解包完成！" +
                                    Environment.NewLine +
                                    string.Format("用时：{0}秒", (endTime - begTime).TotalSeconds)
                    );
                    progress.Close();
                };
                worker.RunWorkerAsync();
            }*/
        }
    }
}
