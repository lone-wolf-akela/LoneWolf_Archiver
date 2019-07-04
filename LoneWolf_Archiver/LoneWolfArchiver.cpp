/**\file LoneWolfArchiver.cpp
 * \brief	Program entry point.
 */

#include <iostream>
#include <fstream>
#include <thread>

#include <boost/program_options.hpp>
#include <json/json.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "archive/archive.h"

namespace po = boost::program_options;
struct
{
	unsigned threadNum = std::thread::hardware_concurrency();
	int compressLevel = 6;
	bool keepSign = false;	
	bool encryption = false;
	std::vector<std::u8string> ignoreList;
}options;

#if !defined(_DEBUG)
#define CATCH_EXCEPTION
#endif
int main(const int argc, char *argv[])
{
#if defined(CATCH_EXCEPTION)
	try
#endif
	{
		spdlog::set_pattern("[%T] [%n] [%^%l%$] %v");
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
						options.ignoreList.emplace_back(
							reinterpret_cast<const char8_t*>(v.asCString()));
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
		const std::string cmd_example = "e.g.:\n  LoneWolfArchiver -c filestoadd.txt -a newarchive.sga -r c:\\rootpath\\";
		po::options_description desc("LoneWolfArchiver Usage");
		desc.add_options()
			(
				"archive,a",
				po::value<std::string>()->value_name("<archivefile>"),
				"- Specify the name of the archive to operate on."
			)
			(
				"create,c",
				po::value<std::string>()->value_name("<buildfile>"),
				"- Create an archive <archivefile> using the <buildfile> input file."
			)
			("generate,g", "- Create an archive <archivefile>. The tool will be scan the files in <rootpath> automatically so you dont't need to specify a <buildfile>.")
			(
				"root,r",
				po::value<std::string>()->value_name("<rootpath>"),
				"- Use the <rootpath> as the source folder. (required when -c, -g or -s used)"
			)
			(
				"scan,s",
				po::value<std::string>()->value_name("<outpout_buildfile>"),
				"- Scan the files in <rootpath> and generate <outpout_buildfile> for latter use."
			)
			(
				"thread,p",
				po::value<unsigned>()->value_name("<threadnumber>")->
				default_value(options.threadNum),
				"- Use <threadnumber> threads to compress or uncompress. Default value is system logic core number."
			)
			(
				"level,v",
				po::value<int>()->value_name("<compresslevel>")->
				default_value(options.compressLevel),
				"- compress level. Should be an integer between 0 and 9: 1 gives best speed, 9 gives best compression, 0 gives no compression at all. Default value is 6. (P.S. Relic's Archieve.exe uses level 9.)"
			)
			("allinone,o", "- When using -g/-s with --allinone, all locales and the data will be built into the same big file (as separated TOCs); otherwise, when using -g/-s without --allinone, all locales will be built into their own big files.")
			("sign,i", "- Calculate tool signature when creating archives. When use --create or --generate without --sign, the tool will skip tool signature calculation to speed up the process.")
			("list,l", "- List the contents of the archive <archivefile>.")
			("test,t", "- Test the archive File, Check the CRC of each file.")
			(
				"extract,e",
				po::value<std::string>()->value_name("<extract location>"),
				"- Extract the archive contents to the folder <extract location>."
			)
			("hash,h", "- List the hash on the archive.")
			("verbose,v", "- (This option is deprecated)")
			("help,h", "- show this help message.")
			;

		po::variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		if (vm.count("help"))
		{
			std::cout << desc << std::endl;
			std::cout << cmd_example << std::endl;
			return 0;
		}
		try
		{
			notify(vm);
		}
		catch (const po::required_option&)
		{
			std::cerr << "Missing argument." << std::endl;
			std::cout << std::endl;
			std::cout << desc << std::endl;
			std::cout << cmd_example << std::endl;
			return 1;
		}

		options.threadNum = vm["thread"].as<unsigned>();
		options.compressLevel = vm["level"].as<int>();
		options.keepSign = options.keepSign || vm.count("sign");

		if (!(
			vm.count("create") ||
			vm.count("generate") ||
			vm.count("scan") ||
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
			std::cout << cmd_example << std::endl;
			return 1;
		}

		if ((vm.count("generate") || vm.count("create") || vm.count("scan")) && !vm.count("root"))
		{
			std::cerr << "-r <rootpath>, required argument when building the archive." << std::endl;
			std::cout << std::endl;
			std::cout << desc << std::endl;
			std::cout << cmd_example << std::endl;
			return 1;
		}
		if ((vm.count("generate") || vm.count("create") || vm.count("list") ||
			vm.count("test")  || vm.count("hash")  || vm.count("generate")) && !vm.count("archive"))
		{
			std::cerr << "-a <archivefile>, required argument missing." << std::endl;
			std::cout << std::endl;
			std::cout << desc << std::endl;
			std::cout << cmd_example << std::endl;
			return 1;
		}		
		/************************/

		//remember when do we begin
		bool showTime = false;
		const auto start = std::chrono::system_clock::now();

		if (vm.count("extract"))
		{
			showTime = true;

			archive::Archive file(std::filesystem::path(
				vm["archive"].as<std::string>()).filename().string());
			file.open(vm["archive"].as<std::string>(), archive::Archive::Mode::Read);
			ThreadPool pool(options.threadNum);
			file.extract(pool, vm["extract"].as<std::string>()).get();
		}
		else if (vm.count("create"))
		{
			showTime = true;
			archive::Archive file(std::filesystem::path(
				vm["archive"].as<std::string>()).filename().string());
			file.open(vm["archive"].as<std::string>(),
				options.encryption ?
				archive::Archive::Mode::Write_Encrypted :
				archive::Archive::Mode::Write_NonEncrypted);
			auto task = buildfile::parseFile(vm["create"].as<std::string>());
			ThreadPool pool(options.threadNum);
			file.create(pool,
				task,
				vm["root"].as<std::string>(),
				options.compressLevel,
				!options.keepSign,
				options.ignoreList).get();
		}
		else if (vm.count("generate"))
		{
			showTime = true;
			auto tasks = buildfile::scanFiles(vm["root"].as<std::string>(), vm.count("allinone"));
			ThreadPool pool(options.threadNum);
			std::vector<archive::Archive> files;
			std::vector<std::future<void>> futurelist;
			{
				files.emplace_back(tasks[0].filename);
				files.back().open(vm["archive"].as<std::string>(),
					options.encryption ?
					archive::Archive::Mode::Write_Encrypted :
					archive::Archive::Mode::Write_NonEncrypted);
				futurelist.push_back(files.back().create(pool,
					tasks[0],
					vm["root"].as<std::string>(),
					options.compressLevel,
					!options.keepSign,
					options.ignoreList));
			}
			for (int i = 1; i < tasks.size(); i++)
			{		
				files.emplace_back(tasks[i].filename);
				files.back().open(std::filesystem::path(
					vm["archive"].as<std::string>()).parent_path() /
					(tasks[i].filename + ".big"),
					options.encryption ?
					archive::Archive::Mode::Write_Encrypted :
					archive::Archive::Mode::Write_NonEncrypted);
				futurelist.push_back(files.back().create(pool,
					tasks[i],
					vm["root"].as<std::string>(),
					options.compressLevel,
					!options.keepSign,
					options.ignoreList));
			}
			for (auto& f : futurelist)
			{
				f.get();
			}	
		}
		else if (vm.count("scan"))
		{
			auto tasks = buildfile::scanFiles(vm["root"].as<std::string>(), vm.count("allinone"));
			{
				buildfile::genFile(vm["scan"].as<std::string>(), tasks[0]);
			}
			for (int i = 1; i < tasks.size(); i++)
			{
				auto p = std::filesystem::path(
					vm["scan"].as<std::string>()).parent_path() /
					(tasks[i].filename + ".txt");
				buildfile::genFile(p, tasks[i]);
			}
		}
		else if (vm.count("list"))
		{
			archive::Archive file(std::filesystem::path(
				vm["archive"].as<std::string>()).filename().string());
			file.open(vm["archive"].as<std::string>(),	archive::Archive::Mode::Read);
			file.listFiles();
		}
		else if (vm.count("test"))
		{
			showTime = true;
			std::cout << "Archive Self Testing" << std::endl;
			std::cout << "TEST: RUNNING - Archive Self Integrity Check" << std::endl;
			archive::Archive file(std::filesystem::path(
				vm["archive"].as<std::string>()).filename().string());
			file.open(vm["archive"].as<std::string>(), archive::Archive::Mode::Read);
			ThreadPool pool(options.threadNum);
			bool passed = file.testArchive(pool).get();
			std::cout << "TEST: " << (passed ? "PASSED" : "FAILED")
				<< "  - Archive Self Integrity Check" << std::endl;
		}
		else if (vm.count("hash"))
		{
			archive::Archive file(std::filesystem::path(
				vm["archive"].as<std::string>()).filename().string());
			file.open(vm["archive"].as<std::string>(), archive::Archive::Mode::Read);
			std::cout << reinterpret_cast<const char*>(file.getArchiveSignature().c_str()) << std::endl;
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
#if defined(CATCH_EXCEPTION)
	catch(const std::exception &e)
	{
		auto logger = spdlog::stderr_color_mt("main");
		logger->error("Unhandled exception: {0}", typeid(e).name());
		logger->error("Message: {0}", e.what());
		logger->error("Aborted");
		abort();
	}
#endif
}

