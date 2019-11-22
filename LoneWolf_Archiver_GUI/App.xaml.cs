using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;

namespace LoneWolf_Archiver_GUI
{
    /// <summary>
    /// App.xaml 的交互逻辑
    /// </summary>
    public partial class App : Application
    {
	    private void UnhandledException(object sender,
		    System.Windows.Threading.DispatcherUnhandledExceptionEventArgs e)
	    {
		    MessageBox.Show(
			    string.Format("An unhandled exception occurred. Error message: {0}", e.Exception.Message),
			    "Error",
			    MessageBoxButton.OK,
			    MessageBoxImage.Error
		    );
	    }
	}
}
