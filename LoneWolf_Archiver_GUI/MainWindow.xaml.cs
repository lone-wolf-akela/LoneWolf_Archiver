using ByteSizeLib;
using Microsoft.Win32;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO.Pipes;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.IO;

namespace LoneWolf_Archiver_GUI
{
    /// <summary>
    /// MainWindow.xaml 的交互逻辑
    /// </summary>
    public partial class MainWindow : Window
    {
        private NamedPipeClientStream pipe;
        private treeitem treeroot = new treeitem { Alias = "", Title = "Root" };
        public MainWindow()
        {
            InitializeComponent();
        }
        private JObject recvAndCheck(string expectedMsg)
        {
            var r = recvMsg();
            if((string)r["msg"]!=expectedMsg)
            {
                MessageBox.Show((string)r["msg"]);
                return null;
            }
            else
            {
                return r;
            }
        }
        private void sendMsg(JObject msg)
        {
            byte[] bytes = Encoding.Default.GetBytes(msg.ToString());
            pipe.Write(bytes, 0, bytes.Length);
            bytes[0] = 0;
            pipe.Write(bytes, 0, 1);
        }
        private void sendMsg(string msg)
        {
            var root = new JObject();
            root["msg"] = msg;
            sendMsg(root);
        }
        private void sendMsg(string msg, JObject param)
        {
            var root = new JObject();
            root["msg"] = msg;
            root["param"] = param;
            sendMsg(root);
        }
        private const int BUFFER_SIZE = 1024;
        private byte[] pipebuffer = new byte[BUFFER_SIZE + 1];
        private int bytes_in_buffer = 0;
        private JObject recvMsg()
        {
            using (var strm = new MemoryStream())
            {
                while (true)
                {
                    int zero_pos = 
                        Array.FindIndex(pipebuffer, 0, bytes_in_buffer, b => (b == '\0'));
                    if (-1 != zero_pos)
                    {
                        //found 0 in buffer
                        strm.Write(pipebuffer, 0, zero_pos);
                        bytes_in_buffer -= zero_pos + 1;
                        Buffer.BlockCopy(pipebuffer, zero_pos + 1, 
                            pipebuffer, 0, BUFFER_SIZE - zero_pos - 1);
                        break;
                    }
                    else
                    {
                        strm.Write(pipebuffer, 0, bytes_in_buffer);
                    }
                    bytes_in_buffer = pipe.Read(pipebuffer, 0, BUFFER_SIZE);
                    if (bytes_in_buffer == 0)
                    {
                        throw new Exception("Pipe Failed.");
                    }
                }
                strm.Position = 0;
                using (var sr = new StreamReader(strm))
                {
                    var a = sr.ReadToEnd();
                    var root = JObject.Parse(a);
                    return root;
                }  
            }     
        }
        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            Process process = new Process();
            // Configure the process using the StartInfo properties.
            process.StartInfo.FileName = "LoneWolf_Archiver.exe";
            string pipename = "LoneWolf_Archiver_Pipe_" + DateTime.Now.ToFileTimeUtc();
            process.StartInfo.Arguments = "--pipe " + pipename;
            process.StartInfo.WindowStyle = ProcessWindowStyle.Minimized;
            process.Start();

            pipe = new NamedPipeClientStream(pipename);

            pipe.Connect();
            sendMsg("hello");
            recvAndCheck("hello");

            filetree.Items.Add(treeroot);
        }
        public class treeitem
        {
            public class file
            {
                public string filename { get; set; }

                private ByteSize _sizeUncompressed;
                public string sizeUncompressed
                {
                    get { return _sizeUncompressed.ToString(); }
                    set { _sizeUncompressed = ByteSize.FromBytes(UInt32.Parse(value)); }
                }

                private ByteSize _sizeCompressed;
                public string sizeCompressed
                {
                    get { return _sizeCompressed.ToString(); }
                    set { _sizeCompressed = ByteSize.FromBytes(UInt32.Parse(value)); }
                }

                private enum CompressMethod
                {
                    Uncompressed,
                    Decompress_During_Read,
                    Decompress_All_At_Once
                }
                private CompressMethod _compressMethod;

                public string compressMethod
                {
                    get
                    {
                        switch (_compressMethod)
                        {
                            case CompressMethod.Uncompressed:
                                return "Uncompressed";
                            case CompressMethod.Decompress_During_Read:
                                return "Decompress During Read";
                            case CompressMethod.Decompress_All_At_Once:
                                return "Decompress All At Once";
                            default:
                                throw new ArgumentOutOfRangeException();
                        }
                    }
                    set
                    {
                        switch (value)
                        {
                            case "store":
                                _compressMethod = CompressMethod.Uncompressed;
                                break;
                            case "compress_stream":
                                _compressMethod = CompressMethod.Decompress_During_Read;
                                break;
                            case "compress_buffer":
                                _compressMethod = CompressMethod.Decompress_All_At_Once;
                                break;
                            default:
                                throw new ArgumentOutOfRangeException();
                        }
                    }
                }

                private DateTime _modificationDate;
                public string modificationDate
                {
                    //get { return _modificationDate.ToString(); }
                    get { return _modificationDate.ToString(); }
                    set
                    {
                        _modificationDate = new DateTime(1970, 1, 1);
                        _modificationDate = _modificationDate.AddSeconds(UInt32.Parse(value));
                    }
                }
            }

            /***************************************/
            public treeitem()
            {
                this.Items = new ObservableCollection<treeitem>();
                this.Files = new ObservableCollection<file>();
            }

            public string Title { get; set; }

            private string _Alias;
            public string Alias
            {
                get
                {
                    if (_Alias is null || _Alias == "")
                        return "";
                    else
                        return " (" + _Alias + ")";
                }
                set { _Alias = value; }
            }
            public ObservableCollection<treeitem> Items { get; set; }
            public ObservableCollection<file> Files { get; set; }
        }

        private void filetree_SelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
        {
            filegrid.ItemsSource = ((treeitem)filetree.SelectedItem).Files;
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
            sendMsg("bye");
            recvAndCheck("bye");
        }

        private treeitem parseFolderJson(JToken folder)
        {
            var folderitem = new treeitem();
            folderitem.Title = Path.GetFileName(
                ((string)folder["path"]).TrimEnd(new[] { '\\', '/' }));
            foreach(var subfolder in folder["subfolders"])
            {
                folderitem.Items.Add(parseFolderJson(subfolder));
            }
            foreach(var file in folder["files"])
            {
                var f = new treeitem.file();
                f.compressMethod = (string)file["storage"];
                f.filename = (string)file["name"];
                f.modificationDate = (string)file["date"];
                f.sizeCompressed = (string)file["compressedlen"];
                f.sizeUncompressed = (string)file["decompressedlen"];

                folderitem.Files.Add(f);
            }
            return folderitem;
        }
        private treeitem parseTocJson(JToken toc)
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

                var openparam = new JObject();
                openparam["path"] = dialog.FileName;
                sendMsg("open", openparam);
                if (recvAndCheck("ok") is null) return;
                sendMsg("get_filetree");
                treeroot.Items.Clear();

                var list = new List<treeitem>();
                treeitem ptr = treeroot;
                var response = recvAndCheck("filetree");
                if (response is null) return;
                var json_filetree = response["param"];
                var name = (string)json_filetree["name"];
                foreach(var toc in json_filetree["tocs"])
                {
                    treeroot.Items.Add(parseTocJson(toc));
                }
            }
        }

        private void MenuExtract_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new System.Windows.Forms.FolderBrowserDialog
            {
                Description = "请选择解压路径",
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
