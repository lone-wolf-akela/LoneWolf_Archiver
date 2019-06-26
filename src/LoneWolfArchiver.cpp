/**\file LoneWolfArchiver.cpp
 * \brief	Program entry point.
 */

#include "stdafx.h"

#include "HWRM_BigFile.h"

namespace po = boost::program_options;

struct
{
	unsigned threadNum = std::thread::hardware_concurrency();
	int compressLevel = 6;
	bool keepSign = false;	
	bool encryption = false;
	std::vector<std::string> ignoreList;
}options;

int main(const int argc, char *argv[])
{
	try 
	{
		//parse json
		{
			std::ifstream configfile("archive_config.json");
			if (!configfile)
			{
				std::cout << "Failed to read 'archive_config.json'. Using default settings..." << std::endl;
			}
			else
			{
				try
				{
					Json::Value config;
					configfile >> config;
					if (0 != config.get("thread_number", 0).asUInt())
					{
						options.threadNum = config.get("thread_number", 0).asUInt();
					}
					options.compressLevel = config.get("compress_level", 6).asInt();
					options.keepSign = config.get("keep_sign", false).asBool();
					options.encryption = config.get("encryption", false).asBool();
					for (auto &v : config.get("ignore_list", {}))
					{
						options.ignoreList.emplace_back(v.asString());
					}
				}
				catch (const Json::Exception& e)
				{
					std::cerr << "'archive_config.json' format invalid. Some settings may use default values:" << std::endl;
					std::cerr << e.what() << std::endl;
				}
			}
		}
		//parse command line
		po::options_description desc("LoneWolfArchiver Usage");
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
				default_value(options.threadNum),
				"- Use <threadnumber> threads to compress or uncompress. Default value is system logic core number."
			)
			(
				"level",
				po::value<int>()->value_name("<compresslevel>")->
				default_value(options.compressLevel),
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
		catch (const po::required_option&)
		{
			std::cerr << "-a <archivefile>, argument missing, required argument." << std::endl;
			std::cout << std::endl;
			std::cout << desc << std::endl;
			std::cout << "e.g.:" << std::endl;
			std::cout << "  LoneWolfArchiver -c filestoadd.txt -a newarchive.sga" << std::endl;
			return 1;
		}

		options.threadNum = vm["thread"].as<unsigned>();
		options.compressLevel = vm["level"].as<int>();
		options.keepSign = options.keepSign || vm.count("sign");

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

		if ((vm.count("generate") || vm.count("create")) && !vm.count("root"))
		{
			std::cerr << "-r <rootpath>, required argument when building the archive." << std::endl;
			std::cout << std::endl;
			std::cout << desc << std::endl;
			std::cout << "e.g.:" << std::endl;
			std::cout << "  LoneWolfArchiver -c filestoadd.txt -a newarchive.sga" << std::endl;
			return 1;
		}

		/************************/

		//remember when do we begin
		bool showTime = false;
		const auto start = std::chrono::system_clock::now();

		if (vm.count("extract"))
		{
			showTime = true;

			BigFile file(vm["archive"].as<std::string>());
			file.setCoreNum(options.threadNum);
			file.extract(vm["extract"].as<std::string>());
		}
		else if (vm.count("create"))
		{
			showTime = true;
			BigFile file(vm["archive"].as<std::string>(), Write);

			file.setCompressLevel(options.compressLevel);
			file.setCoreNum(options.threadNum);
			file.writeEncryption(options.encryption);
			file.skipToolSignature(!options.keepSign);
			file.setIgnoreList(options.ignoreList);

			file.create(vm["root"].as<std::string>(), vm["create"].as<std::string>());
		}
		else if (vm.count("generate"))
		{
			showTime = true;
			BigFile file;

			file.setCompressLevel(options.compressLevel);
			file.setCoreNum(options.threadNum);
			file.writeEncryption(options.encryption);
			file.skipToolSignature(!options.keepSign);
			file.setIgnoreList(options.ignoreList);

			file.generate(
				vm["archive"].as<std::string>(),
				vm["root"].as<std::string>(),
				vm.count("allinone")
			);
		}
		else if (vm.count("list"))
		{
			BigFile file(vm["archive"].as<std::string>());
			file.listFiles();
		}
		else if (vm.count("test"))
		{
			showTime = true;
			BigFile file(vm["archive"].as<std::string>());
			file.setCoreNum(options.threadNum);
			file.testArchive();
		}
		else if (vm.count("hash"))
		{
			BigFile file(vm["archive"].as<std::string>());
			std::cout << file.getArchiveSignature();
		}

		//print the time we use if needed
		const auto end = std::chrono::system_clock::now();
		std::chrono::duration<double, std::ratio<1>> diff = end - start;
		if (showTime)
		{
			std::cout << "Operation took " << std::fixed << std::setprecision(2)
				<< diff.count() << " seconds." << std::endl;
		}
		return 0;
	}
	catch(const std::exception &e)
	{
		std::cerr << "Unhandled exception: " << typeid(e).name() << std::endl;
		std::cerr << "\t" << e.what() << std::endl;
		std::cerr << "Aborted" << std::endl;
		abort();
	}
}

