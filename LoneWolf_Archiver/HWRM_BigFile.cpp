#include "stdafx.h"

#include "HWRM_BigFile.h"

void BigFile::open(boost::filesystem::path file, BigFileState state)
{
	_internal.open(file, state);
}

void BigFile::close()
{
	_internal.close();
}

void BigFile::setCoreNum(unsigned coreNum)
{
	_internal.setCoreNum(coreNum);
}

void BigFile::setCompressLevel(int level)
{
	_internal.setCompressLevel(level);
}

void BigFile::skipToolSignature(bool skip)
{
	_internal.skipToolSignature(skip);
}

void BigFile::writeEncryption(bool enc)
{
	_internal.writeEncryption(enc);
}

void BigFile::extract(boost::filesystem::path directory)
{
	_internal.extract(directory);
}

void BigFile::listFiles()
{
	_internal.listFiles();
}

void BigFile::testArchive()
{
	_internal.testArchive();
}

static bool match(char const *needle, char const *haystack) 
{
	for (; *needle != '\0'; ++needle) {
		switch (*needle) {
		case '?':
			if (*haystack == '\0')
				return false;
			++haystack;
			break;
		case '*': {
			if (needle[1] == '\0')
				return true;
			size_t max = strlen(haystack);
			for (size_t i = 0; i < max; i++)
				if (match(needle + 1, haystack + i))
					return true;
			return false;
		}
		default:
			if (*haystack != *needle)
				return false;
			++haystack;
		}
	}
	return *haystack == '\0';
}

void BigFile::create(boost::filesystem::path root, boost::filesystem::path build)
{
	//check ArchiveIgnore.txt
	_archiveIgnoreSet.clear();
	boost::filesystem::ifstream input("ArchiveIgnore.txt");
	if(input.is_open())
	{
		while (input.peek() != EOF)
		{
			std::string tmpStr;
			getline(input, tmpStr);
			if (tmpStr != "")
			{
				_archiveIgnoreSet.push_back(boost::to_lower_copy(tmpStr));
			}
		}
	}
	else
	{
		std::cout << "No ArchiveIgnore.txt Detected." << std::endl;
	}

	std::cout << "Reading Build Config..." << std::endl;

	//though there seems no problem, I guess I'd still make sure the root path ends with '\' or '/'
	std::string rootstr = root.string();
	if (rootstr != "" && rootstr.back() != '\\' && rootstr.back() != '/')
	{
		rootstr += '\\';
	}
	//and we must make sure it's all lower case
	boost::to_lower(rootstr);
	root = rootstr;

	_parseBuildfile(build);
	BuildArchiveTask task;
	task.name = _buildArchiveSetting.name;
	for (BuildTOCSetting &tocSet : _buildArchiveSetting.buildTOCSetting)
	{
		BuildTOCTask tocTask;
		tocTask.name = tocSet.name;
		tocTask.alias = tocSet.alias;
		tocTask.rootFolderTask.name = "";

		boost::filesystem::path tocRootPath(root);

		//though there seems no problem, I guess I'd still make sure the path ends with '\' or '/'
		if (
			tocSet.relativeroot != "" &&
			tocSet.relativeroot.back() != '\\'&&
			tocSet.relativeroot.back() != '/'
			)
		{
			tocSet.relativeroot += '\\';
		}

		tocRootPath /= tocSet.relativeroot;
		tocRootPath = system_complete(tocRootPath);

		for (std::wstring &file : tocSet.files)
		{
			BuildFileTask fileTask;
			fileTask.realpath = boost::filesystem::path(file);
			fileTask.realpath = system_complete(fileTask.realpath);
			fileTask.name = boost::to_lower_copy(fileTask.realpath.filename().string());

			fileTask.filesize = uint32_t(file_size(fileTask.realpath));
			fileTask.compressMethod = tocSet.defcompression;

			bool skipfile = false;
			//check _archiveIgnoreSet
			for(std::string set : _archiveIgnoreSet)
			{
				if (boost::ends_with(set, "\\") || boost::ends_with(set, "/"))
				{
					set = set.substr(0, set.length() - 1);
					for (
						auto iter = fileTask.realpath.begin(); 
						iter != fileTask.realpath.end(); 
						++iter
						)
					{
						if (boost::to_lower_copy(iter->string()) == set)
						{
							skipfile = true;
							break;
						}
					}
					if(skipfile)
					{
						break;
					}
				}
				else
				{
					if (match(set.c_str(), fileTask.name.c_str()))
					{
						skipfile = true;
						break;
					}
				}
			}
			if (skipfile)
			{
				continue;
			}
			//check buildFileSettings			
			for (BuildFileSetting &fileSet : tocSet.buildFileSettings)
			{
				if (
					(fileSet.minsize == -1 || fileTask.filesize >= uint32_t(fileSet.minsize)) &&
					(fileSet.maxsize == -1 || fileTask.filesize <= uint32_t(fileSet.maxsize)) &&
					match(fileSet.wildcard.c_str(), fileTask.name.c_str())
					)
				{
					switch (fileSet.command)
					{
					case BuildFileSetting::Override:
						fileTask.compressMethod = fileSet.ct;
						break;
					case BuildFileSetting::SkipFile:
						skipfile = true;
						break;
					default:
						throw OutOfRangeError();
					}
					break;
				}
			}
			if (skipfile)
			{
				continue;
			}
			//if the file size is 0 and is not skipped, then force it uncompressed
			if (fileTask.filesize == 0)
			{
				fileTask.compressMethod = Uncompressed;
			}

			boost::filesystem::path relativePath = relative(fileTask.realpath, tocRootPath);
			relativePath = relativePath.remove_filename();
			BuildFolderTask *currentFolder = &tocTask.rootFolderTask;

			boost::filesystem::path currentPath("");
			for (auto iter = relativePath.begin(); iter != relativePath.end(); ++iter)
			{
				currentPath /= *iter;

				bool found = false;
				for (BuildFolderTask &subFolderTask : currentFolder->subFolderTasks)
				{
					if (
						boost::to_lower_copy(subFolderTask.name) ==
						boost::to_lower_copy(iter->string())
						)
					{
						currentFolder = &subFolderTask;
						found = true;
						break;
					}
				}
				if (!found)
				{
					BuildFolderTask newFolder;
					newFolder.name = iter->string();
					newFolder.fullpath = currentPath.string();
					currentFolder->subFolderTasks.push_back(newFolder);
					currentFolder = &currentFolder->subFolderTasks.back();
				}
			}
			currentFolder->subFileTasks.push_back(fileTask);
		}
		task.buildTOCTasks.push_back(tocTask);
	}

	_internal.build(task);
}

// 对文件夹深度优先遍历获取所有文件名放入容器中  
static std::vector<boost::filesystem::path> getAllFileNames(const boost::filesystem::path &rootPath)
{
	std::vector<boost::filesystem::path> vecOut;
	for (
		boost::filesystem::directory_iterator iter(rootPath);
		iter != boost::filesystem::directory_iterator();
		++iter
		)
	{
		if (is_directory(*iter))
		{
			auto tmp = getAllFileNames(*iter);
			vecOut.insert(vecOut.end(), tmp.begin(), tmp.end());
		}
		else
		{
			vecOut.emplace_back(*iter);
		}
	}
	return vecOut;
}

//this string will be writen in every build configs
static char fileSettingStr[] =
R"CONFIG(FileSettingsStart defcompression="1"
Override wildcard="*.*" minsize="-1" maxsize="100" ct="0"
Override wildcard="*.mp3" minsize="-1" maxsize="-1" ct="0"
Override wildcard="*.wav" minsize="-1" maxsize="-1" ct="0"
Override wildcard="*.jpg" minsize="-1" maxsize="-1" ct="0"
Override wildcard="*.lua" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.fda" minsize="-1" maxsize="-1" ct="0"
Override wildcard="*.txt" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.ship" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.resource" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.pebble" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.level" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.wepn" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.subs" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.miss" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.events" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.madstate" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.script" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.ti" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.st" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.vp" minsize="-1" maxsize="-1" ct="2"
Override wildcard="*.wf" minsize="-1" maxsize="-1" ct="2"
SkipFile wildcard="Keeper.txt" minsize="-1" maxsize="-1"
SkipFile wildcard="*.big" minsize="-1" maxsize="-1"
SkipFile wildcard="*_.*" minsize="-1" maxsize="-1"
FileSettingsEnd
)CONFIG";

/**
 * \brief			generate build configs from a folder
 * \param file		the out put .big file 
 * \param root the	root path of unpackaged mod files
 * \param allInOne	should we pack all locale files in the same .big file with other files, 
 *					or pack them in their own .big?
 */
void BigFile::generate(boost::filesystem::path file, boost::filesystem::path root, bool allInOne)
{
	std::cout << "Generating Build Configs..." << std::endl << std::endl;

	//get Mod Name from the folder name
	const std::string modName = file.filename().replace_extension().string();

	//vector to store all locales detected
	std::vector<boost::filesystem::path> locales;
	//vector to store all .big files to build. The filesystem::path is the 
	//output .big file of this build task, and the std::string is the build config's filename.
	std::vector<std::tuple<boost::filesystem::path, std::string>> buildTasks;	
	
	//get all locales from "locale" folder
	for (
		boost::filesystem::directory_iterator iter(root/"locale");
		iter != boost::filesystem::directory_iterator();
		++iter
		)
	{
		if (is_directory(*iter))
		{
			locales.push_back(system_complete(iter->path()));
		}
	}
	
	//let's create the first build task which contains all things other than locales
	std::string buildfilename = "buildfile.txt";
	boost::filesystem::ofstream buildfile(buildfilename);
	buildTasks.emplace_back(file, buildfilename);
	if (!buildfile.is_open())
	{
		throw FileIoError("Error happened when creating buildfile config.");
	}	

	//first data buildfile
	buildfile << "Archive name=\"MOD" << modName << "\"" << std::endl;
	buildfile << "TOCStart name=\"TOCMOD" << modName
		<< "\" alias=\"Data\" relativeroot=\"\"" << std::endl;
	buildfile << fileSettingStr;

	//find all files except locales
	std::vector<boost::filesystem::path> allFiles = getAllFileNames(root);
	for (boost::filesystem::path &dataFile : allFiles)
	{
		dataFile = system_complete(dataFile);
		bool isLocale = false;
		//find if it's one of the locales
		for (boost::filesystem::path &locale : locales)
		{
			if(boost::istarts_with(dataFile.string(), locale.string()))
			{
				isLocale = true;
				break;
			}
		}
		if (!isLocale)
		{
			//make sure we use UTF-8 here
			buildfile << boost::locale::conv::from_utf(system_complete(dataFile).wstring(), "UTF-8")
				<< std::endl;
		}
	}
	buildfile << "TOCEnd" << std::endl;

	//now deal with locales
	for(boost::filesystem::path &locale : locales)
	{
		//locale name is the last element in the path
		//its a little strange that boost::filesystem::path has no back() member,
		//so I use locale.rbegin()
		const std::string localeName = locale.rbegin()->string();
		
		//if not all in one, then every locale should be a new build files
		//or it just needs to be a new TOC entry
		if(!allInOne)
		{
			buildfile.close();
			buildfilename = "buildfile_" + localeName + ".txt";
			
			//locale .big files has the same path but different name with the main .big
			file = file.remove_filename() / (localeName + ".big");
			buildTasks.emplace_back(file, buildfilename);

			buildfile.open(buildfilename);
			if (!buildfile.is_open())
			{
				throw FileIoError("Error happened when creating buildfile config.");
			}
			buildfile << "Archive name=\"MOD" << modName << localeName << "\"" << std::endl;
		}
		buildfile << "TOCStart name=\"TOCMOD" << modName << localeName
			<< "\" alias=\"Locale\" relativeroot=\"Locale\\" << localeName << "\"" << std::endl;
		buildfile << fileSettingStr;

		//find all files in this locale
		std::vector<boost::filesystem::path> allLocaleFiles = getAllFileNames(locale);
		for (boost::filesystem::path &localeFile : allLocaleFiles)
		{
			//use UTF-8!
			buildfile << boost::locale::conv::from_utf(system_complete(localeFile).wstring(), "UTF-8")
				<< std::endl;
		}
		buildfile << "TOCEnd" << std::endl;
	}
	buildfile.close();

	//with all build configs prepared, let's actually begin create our .big files
	for (auto &buildTask : buildTasks)
	{
		std::cout << "Creating: " << std::get<0>(buildTask) << std::endl;
		open(std::get<0>(buildTask), Write);
		create(root, std::get<1>(buildTask));
		close();
	}
}

std::string BigFile::getArchiveSignature() const
{
	std::stringstream stream;
	std::string tmpStr;
	const uint8_t *ptr = _internal.getArchiveSignature();

	for (int i = 0; i < 16; ++i)
	{
		stream << "\\x" << std::hex << unsigned(ptr[i]);
	}
	stream >> tmpStr;

	return tmpStr;
}

static BuildfileCommand getCommand(std::istream &input)
{
	BuildfileCommand result;

	std::string tmpStr;
	while (input.peek() == '\r' || input.peek() == '\n')
	{
		getline(input, tmpStr);
		if (input.peek() == EOF)
		{
			throw FormatError("Buildfile format error");
		}
	}
	getline(input, tmpStr);
	//all commands should be lower case
	boost::to_lower(tmpStr);
	boost::trim(tmpStr);
	bool escape = false;
	std::vector<std::string> tmpStrVec;
	tmpStrVec.push_back("");
	for (char &c : tmpStr)
	{
		switch (c)
		{
		case '\"':
			escape = !escape;
			tmpStrVec.back() += c;
			break;
		case ' ':
		case '\t':
			if (escape)
			{
				tmpStrVec.back() += c;
			}
			else
			{
				tmpStrVec.push_back("");
			}
			break;
		default:
			tmpStrVec.back() += c;
		}
	}

	result.command = tmpStrVec[0];
	for (size_t i = 1; i < tmpStrVec.size(); ++i)
	{
		auto found = tmpStrVec[i].find_first_of("=");
		if (found == std::string::npos)
		{
			throw FormatError("Buildfile format error");
		}
		std::string tmpParamName = tmpStrVec[i].substr(0, found);
		result.params[tmpParamName] = tmpStrVec[i].substr(found + 1);
		trim_if(result.params[tmpParamName], boost::is_any_of("\""));
	}

	return result;
}

template <typename A,typename B>
static B typeConvert(A a)
{
	B b;
	std::stringstream stream;
	stream << a;
	stream >> b;
	return b;
}

void BigFile::_parseBuildfile(boost::filesystem::path buildfile)
{
	_buildArchiveSetting.buildTOCSetting.clear();
	
	boost::filesystem::ifstream input(buildfile);
	if (!input.is_open())
	{
		throw FileIoError("Error happened when openning buildfile.");
	}
	//test if the file has BOM
	char BOM[4];
	input.read(BOM, 3);
	BOM[3] = 0;
	if(strcmp(BOM,"\xef\xbb\xbf"))
	{
		//no BOM, go back
		input.seekg(0);
	}

	while (input.peek() != EOF)
	{
		bool exitflag = false;
		while (input.peek() == '\r' || input.peek() == '\n')
		{
			std::string tmpStr;
			getline(input, tmpStr);
			if (input.peek() == EOF)
			{
				exitflag = true;
				break;
			}
		}
		if(exitflag)
		{
			break;
		}

		BuildfileCommand command = getCommand(input);
		if(command.command=="archive")
		{
			_buildArchiveSetting.name = command.params["name"];
		}
		else if(command.command == "tocstart")
		{
			BuildTOCSetting thisTOC;
			thisTOC.name = command.params["name"];
			thisTOC.alias = command.params["alias"];
			thisTOC.relativeroot = command.params["relativeroot"];

			command = getCommand(input);
			if (command.command == "filesettingsstart")
			{
				thisTOC.defcompression =
					command.params["defcompression"] == "0" ? Uncompressed :
					command.params["defcompression"] == "1" ? Decompress_During_Read :
					command.params["defcompression"] == "2" ? Decompress_All_At_Once :
					throw FormatError("Buildfile format error");

				command = getCommand(input);
				while(command.command!="filesettingsend")
				{
					BuildFileSetting thisFileSet;
					thisFileSet.wildcard = boost::to_lower_copy(command.params["wildcard"]);
					thisFileSet.minsize = typeConvert<std::string, int>(command.params["minsize"]);
					thisFileSet.maxsize = typeConvert<std::string, int>(command.params["maxsize"]);
					if (command.command == "override")
					{
						thisFileSet.command = BuildFileSetting::Override;
						thisFileSet.ct =
							command.params["ct"] == "0" ? Uncompressed :
							command.params["ct"] == "1" ? Decompress_During_Read :
							command.params["ct"] == "2" ? Decompress_All_At_Once :
							throw FormatError("Buildfile format error");
					}
					else if (command.command == "skipfile")
					{
						thisFileSet.command = BuildFileSetting::SkipFile;
					}
					else
					{
						throw FormatError("Buildfile format error");
					}

					thisTOC.buildFileSettings.push_back(thisFileSet);

					command = getCommand(input);
				}
			}
			std::string file;
			getline(input, file);
			//make sure the path is lower case
			boost::to_lower(file);
			while (file != "tocend")
			{								
				thisTOC.files.push_back(boost::locale::conv::to_utf<wchar_t>(file, "UTF-8"));
				getline(input, file);
				boost::to_lower(file);
			}
			sort(thisTOC.files.begin(), thisTOC.files.end());
			_buildArchiveSetting.buildTOCSetting.push_back(thisTOC);
		}
	}
	input.close();
}
