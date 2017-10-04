// LoneWolfArchiver.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include "HWRM_BigFile.h"

//#define DEBUG_EX
//#define DEBUG_BD
//#define DEBUG_GN
//#define DEBUG_GNONE

namespace po = boost::program_options;

int main(int argc, char *argv[])
{
#if defined(DEBUG_BD)
	argc = 7;
	argv = new char*[argc];
	argv[0] = "";
	argv[1] = "-a";
	argv[2] = "F:/DataStore/迅雷下载/家园2重制版/fx.big";
	argv[3] = "-c";
	argv[4] = "D:/buildfile.txt";
	argv[5] = "-r";
	argv[6] = "F:/tmp/新建文件夹/FXG/";
#endif
#if defined(DEBUG_GN)
	argc = 6;
	argv = new char*[argc];
	argv[0] = "";
	argv[1] = "-a";
	argv[2] = "F:/DataStore/迅雷下载/家园2重制版/fx.big";
	argv[3] = "-g";
	argv[4] = "-r";
	argv[5] = "F:/tmp/新建文件夹/FXG/";
#endif
#if defined(DEBUG_GNONE)
	argc = 7;
	argv = new char*[argc];
	argv[0] = "";
	argv[1] = "-a";
	argv[2] = "F:/DataStore/迅雷下载/家园2重制版/fx.big";
	argv[3] = "-g";
	argv[4] = "-r";
	argv[5] = "F:/tmp/新建文件夹/FXG/";
	argv[6] = "--allinone";
#endif
#if defined(DEBUG_EX)
	argc = 5;
	argv = new char*[argc];
	argv[0] = "";
	argv[1] = "-a";
	argv[2] = "F:/DataStore/迅雷下载/家园2重制版/fx.big";
	argv[3] = "-e";
	argv[4] = "f:/tmp/";
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
		("generate,g", "- Create an archive <archivefile>. Buildfile will be generated automatically so you dont't need specify a <buildfile>.")
		(
			"root,r",
			po::value<std::string>()->value_name("<rootpath>"),
			"- Use the <rootpath> as the source folder to locate files listed in the <buildfile>. (required when -c or -g used)"
		)
		(
			"thread",
			po::value<unsigned>()->value_name("<threadnumber>")->
				default_value(std::thread::hardware_concurrency()),
			"- Use <threadnumber> threads to compress or uncompress. Default value is system logic core number."
		)
		(
			"level",
			po::value<int>()->value_name("<compresslevel>")->default_value(6),
			"- compress level. Should be an integer between 0 and 9: 1 gives best speed, 9 gives best compression, 0 gives no compression at all. Default value is 6. (P.S. Relic's Archieve.exe uses level 9.)"
		)
		("allinone", "- When using --generate with --allinone, all locales and the data will be built into the same big file (as separated TOCs); otherwise, when using --generate without --allinone, all locales will be built into their own big files.")
		("sign", "- Calculate tool signature when creating archives. When use --create or --generate without --sign, the tool will skip tool signature calculation to speed up the process.")
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
			vm.count("generate") ||
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
	if((vm.count("generate") || vm.count("create")) && !vm.count("root"))
	{
		std::cerr << "-r <rootpath>, required argument when building the archive." << std::endl;
		std::cout << std::endl;
		std::cout << desc << std::endl;
		std::cout << "e.g.:" << std::endl;
		std::cout << "  LoneWolfArchiver -c filestoadd.txt -a newarchive.sga" << std::endl;
		return 1;
	}

	/************************/
	bool encryption = false;
	boost::filesystem::ifstream shojo("少女在河畔歌唱.txt");
	if (shojo.is_open())
	{
		std::string magic;
		getline(shojo, magic);
		if (magic == "为这世界降下封印！")
		{
			encryption = true;
		}
	}
	/************************/

	bool showTime = false;
	auto start = std::chrono::system_clock::now();
	if(vm.count("extract"))
	{
		showTime = true;

		BigFile file(vm["archive"].as<std::string>());
		file.setCoreNum(vm["thread"].as<unsigned>());
		file.extract(vm["extract"].as<std::string>());
	}
	else if(vm.count("create"))
	{
		showTime = true;
		BigFile file(vm["archive"].as<std::string>(), write);

		file.setCompressLevel(vm["level"].as<int>());
		file.setCoreNum(vm["thread"].as<unsigned>());
		file.writeEncryption(encryption);
		file.skipToolSignature(!vm.count("sign"));

		file.create(vm["root"].as<std::string>(), vm["create"].as<std::string>());
	}
	else if(vm.count("generate"))
	{
		showTime = true;
		BigFile file;

		file.setCompressLevel(vm["level"].as<int>());
		file.setCoreNum(vm["thread"].as<unsigned>());
		file.writeEncryption(encryption);
		file.skipToolSignature(!vm.count("sign"));

		file.generate(
			vm["archive"].as<std::string>(),
			vm["root"].as<std::string>(),
			vm.count("allinone")
		);
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
		file.setCoreNum(vm["thread"].as<unsigned>());
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

