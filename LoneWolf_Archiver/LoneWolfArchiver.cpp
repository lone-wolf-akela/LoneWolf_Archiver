// LoneWolfArchiver.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include "HWRM_BigFile.h"

namespace po = boost::program_options;

int main(int argc, char *argv[])
{
#if defined(_DEBUG)
	argc = 4;
	argv = new char*[argc];
	argv[0] = "";
	argv[1] = "-a";
	//argv[2] = "D:/Program Files (x86)/Steam/steamapps/common/Homeworld/HomeworldRM/Data/English.big";
	argv[3] = "-t";
	argv[2] = "F:/DataStore/迅雷下载/家园2重制版/Homeworld2.big";
	//argv[3] = "-e";
	//argv[4] = "F:/DataStore/迅雷下载/家园2重制版/";
#endif

	po::options_description desc("LoneWolfArchiver.exe Usage");
	desc.add_options()		
		(
			"archive,a", 
			po::value<std::string>()->value_name("<archivefile>")->required(), 
			"- Specify the name of the archive to operate on."
		)
		(
			"create,c", 
			po::value<std::string>()->value_name("<buildfile>"),
			"- Create an archive <archivefile> using the <buildfile> input file."
		)
		("generate,g", "- Create an archive <archivefile>. Buildfile will be generated automatically.")
		(
			"root,r",
			po::value<std::string>()->value_name("<rootpath>"),
			"- Use the <rootpath> as the source folder to locate files listed in the <buildfile>. (required when -c or -g used)"
		)
		("list,l", "- List the contents of the archive <archivefile>.")
		("test,t", "- Test the archive File, Check the CRC of each file.")
		(
			"extract,e",
			po::value<std::string>()->value_name("<extract location>"),
			"- Extract the archive contents to the folder <extract location>."
		)
		("hash", "- List the hash on the archive.")
		("verbose,v", "- (This option is deprecated)")
		("help,h", "- show this help message.")
		;
	
	po::variables_map vm;
	store(parse_command_line(argc, argv, desc), vm);
	if (vm.count("help")) 
	{
		std::cout << desc << std::endl;
		std::cout << "e.g.:" << std::endl;
		std::cout << "  LoneWolfArchiver -c filestoadd.txt -a newarchive.sga" << std::endl;
		return 0;
	}
	try
	{
		notify(vm);
	}
	catch(po::required_option)
	{
		std::cerr << "-a <archivefile>, argument missing, required argument." << std::endl;
		std::cout << std::endl;
		std::cout << desc << std::endl;
		std::cout << "e.g.:" << std::endl;
		std::cout << "  LoneWolfArchiver -c filestoadd.txt -a newarchive.sga" << std::endl;
		return 1;
	}
	if (!(
			vm.count("create") || 
			vm.count("list") || 
			vm.count("test") || 
			vm.count("hash") || 
			vm.count("extract") 
			)
		)
	{
		std::cerr << "Missing argument." << std::endl;
		std::cout << std::endl;
		std::cout << desc << std::endl;
		std::cout << "e.g.:" << std::endl;
		std::cout << "  LoneWolfArchiver -c filestoadd.txt -a newarchive.sga" << std::endl;
		return 1;
	}
	if(vm.count("create") && !vm.count("root"))
	{
		std::cerr << "-r <rootpath>, required argument when building the archive." << std::endl;
		std::cout << std::endl;
		std::cout << desc << std::endl;
		std::cout << "e.g.:" << std::endl;
		std::cout << "  LoneWolfArchiver -c filestoadd.txt -a newarchive.sga" << std::endl;
		return 1;
	}

	bool showTime = false;
	auto start = std::chrono::system_clock::now();
	if(vm.count("extract"))
	{
		showTime = true;

		BigFile file(vm["archive"].as<std::string>());
		file.extract(vm["extract"].as<std::string>());
	}
	else if(vm.count("create"))
	{
		showTime = true;
	}
	else if(vm.count("generate"))
	{
		showTime = true;
	}
	else if(vm.count("list"))
	{
		BigFile file(vm["archive"].as<std::string>());
		file.listFiles();
	}
	else if(vm.count("test"))
	{
		showTime = true;
		BigFile file(vm["archive"].as<std::string>());
		file.testArchive();
	}
	else if(vm.count("hash"))
	{
		BigFile file(vm["archive"].as<std::string>());
		std::cout << file.getArchiveSignature();
	}
	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double, std::ratio<1>> diff = end - start;
	if (showTime)
	{
		std::cout << "Operation took " << std::fixed << std::setprecision(2)
			<< diff.count() << " seconds." << std::endl;
	}
    return 0;
}

