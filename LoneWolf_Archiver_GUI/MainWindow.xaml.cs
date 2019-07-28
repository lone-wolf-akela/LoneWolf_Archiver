using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.IO;
using System.IO.Pipes;
using System.Diagnostics;
using System.Windows.Interop;
using Microsoft.Win32;
using ByteSizeLib;
using Newtonsoft.Json.Linq;

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

        private void sendMsg(JObject msg)
        {
            byte[] bytes = Encoding.Default.GetBytes(msg.ToString());
            pipe.Write(bytes, 0, bytes.Length);
            bytes[0] = 0;
            pipe.Write(bytes, 0, 1);
        }
        private JObject msgStr2Json(string str)
        {
            var root = new JObject();
            root["msg"] = str;
            return root;
        }
        private void sendMsg(string msg)
        {
            sendMsg(msgStr2Json(msg));
        }

        private JObject recvMsg()
        {
            string msg_str = "";
            var c = new byte[1];
            while (true)
            {
                pipe.Read(c, 0, 1);
                if (c[0] == 0)
                    break;
                msg_str += Convert.ToChar(c[0]);
            }
            var root = JObject.Parse(msg_str);
            return root;
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
            var s = (string)recvMsg()["msg"];
            if (s != "hello")
                MessageBox.Show("Failed to connect pipe.");

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
                                return "无压缩";
                            case CompressMethod.Decompress_During_Read:
                                return "使用时解压";
                            case CompressMethod.Decompress_All_At_Once:
                                return "解压后缓存于内存";
                            default:
                                throw new ArgumentOutOfRangeException();
                        }
                    }
                    set
                    {
                        switch (value)
                        {
                            case "0":
                                _compressMethod = CompressMethod.Uncompressed;
                                break;
                            case "16":
                                _compressMethod = CompressMethod.Decompress_During_Read;
                                break;
                            case "32":
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
                    if (_Alias == "")
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
            var s = (string)recvMsg()["msg"];
            if (s != "bye")
                MessageBox.Show(s);
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
            /*if (dialog.ShowDialog() == true)
            {
                filegrid.ItemsSource = null;

                sendMsg("open-big");
                sendMsg(dialog.FileName);

                sendMsg("get-foldertree");
                treeroot.Items.Clear();

                var list = new List<treeitem>();
                treeitem ptr = treeroot;
                string s = recvMsg();
                while (s != "\u0004")
                {
                    switch (s)
                    {
                        case "\u0001":
                            s = recvMsg();
                            while (s != "\u0001")
                            {
                                var file = new treeitem.file
                                {
                                    filename = s,
                                    sizeUncompressed = recvMsg(),
                                    sizeCompressed = recvMsg(),
                                    compressMethod = recvMsg(),
                                    modificationDate = recvMsg()
                                };
                                ptr.Files.Add(file);

                                s = recvMsg();
                            }
                            break;
                        case "\u0002":
                            list.Add(ptr);
                            break;
                        case "\u0003":
                            ptr = list.Last();
                            list.RemoveAt(list.Count - 1);
                            break;
                        default:
                            ptr = new treeitem
                            {
                                Title = s,
                                Alias = recvMsg()
                            };
                            list.Last().Items.Add(ptr);
                            break;
                    }
                    s = recvMsg();
                }
            }*/
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
