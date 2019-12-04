using ByteSizeLib;
using Microsoft.Win32;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO.Pipes;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Media.Animation;
using Newtonsoft.Json;

using LoneWolfArchiverDotnet;

namespace LoneWolf_Archiver_GUI
{

	/// <summary>
	/// MainWindow.xaml 的交互逻辑
	/// </summary>
	public sealed partial class MainWindow : Window, IDisposable
	{
		const string PortnumFile = "archiver_server_port.tmp";
		private bool _disposed = false;
		private readonly Archiver _archiver = new Archiver();
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
				_archiver.Dispose();
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

				_archiver.Open(dialog.FileName);
				var jsonFiletree = JToken.Parse(_archiver.GetFiletree());
				_treeroot.Items.Clear();
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

			if (dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
			{
				DateTime begTime = DateTime.Now;

				var progress = new ExtractProgress();
				progress.Show();
				//progress.Topmost = true;

				var worker = new BackgroundWorker();
				worker.DoWork += delegate
				{
					void Callback(MsgType type, string msg, int current, int max, string filename)
					{
						if (filename != null)
						{
							progress.Dispatcher?.BeginInvoke(new Action(() =>
								{
									progress.UpdateProgress(current, max, filename);
								}));
						}

						if (msg != null)
						{
							progress.Dispatcher?.BeginInvoke(new Action(() => { progress.UpdateCurrentWork(msg); }));
						}
					}
					_archiver.ExtractAll(dialog.SelectedPath, Callback);
				};
				worker.RunWorkerCompleted += delegate
				{
					progress.Topmost = false;
					DateTime endTime = DateTime.Now;
					MessageBox.Show(string.Format("解包完成！{0}用时：{1}秒。", Environment.NewLine,
						(endTime - begTime).TotalSeconds));
					progress.Close();
				};
				worker.RunWorkerAsync();
			}
		}
	}
}
