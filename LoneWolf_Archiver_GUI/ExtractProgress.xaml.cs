using System;
using System.Collections.Generic;
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
using System.Windows.Shapes;

namespace LoneWolf_Archiver_GUI
{
    /// <summary>
    /// ExtractProgress.xaml 的交互逻辑
    /// </summary>
    public partial class ExtractProgress : Window
    {
        public ExtractProgress()
        {
            InitializeComponent();
        }

        public void init(uint totalNum)
        {
            readBar.Maximum = decompressBar.Maximum = writeBar.Maximum = totalNum;
        }

        public void Update(uint readNum, uint deCompressNum, uint writeNum)
        {
            readBar.Value = readNum;
            decompressBar.Value = deCompressNum;
            writeBar.Value = writeNum;
        }

    }
}
