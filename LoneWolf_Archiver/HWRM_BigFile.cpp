#include "stdafx.h"

#include "HWRM_BigFile.h"

void BigFile::open(boost::filesystem::path file)
{
	_internal.open(file, read);
}

void BigFile::close()
{
	_internal.close();
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

static void EscapeRegex(std::string &regex)
{
	boost::replace_all(regex, "\\", "\\\\");
	boost::replace_all(regex, "^", "\\^");
	boost::replace_all(regex, ".", "\\.");
	boost::replace_all(regex, "$", "\\$");
	boost::replace_all(regex, "|", "\\|");
	boost::replace_all(regex, "(", "\\(");
	boost::replace_all(regex, ")", "\\)");
	boost::replace_all(regex, "[", "\\[");
	boost::replace_all(regex, "]", "\\]");
	boost::replace_all(regex, "*", "\\*");
	boost::replace_all(regex, "+", "\\+");
	boost::replace_all(regex, "?", "\\?");
	boost::replace_all(regex, "/", "\\/");
}

static bool MatchTextWithWildcards(const std::string &text, std::string wildcardPattern, bool caseSensitive)
{
	// Escape all regex special chars
	EscapeRegex(wildcardPattern);

	// Convert chars '*?' back to their regex equivalents
	boost::replace_all(wildcardPattern, "\\?", ".");
	boost::replace_all(wildcardPattern, "\\*", ".*");

	boost::regex pattern(wildcardPattern, caseSensitive ? boost::regex::normal : boost::regex::icase);

	return regex_match(text, pattern);
}

void BigFile::create(
	boost::filesystem::path root,
	boost::filesystem::path build
)
{
	_parseBuildfile(build);
	BuildArchiveTask task;
	task.name = _buildArchiveSetting.name;
	for(BuildTOCSetting &tocSet : _buildArchiveSetting.buildTOCSetting)
	{
		BuildTOCTask tocTask;
		tocTask.name = tocSet.name;
		tocTask.alias = tocSet.alias;
		tocTask.rootFolderTask.name = "";

		boost::filesystem::path tocRootPath(root);
		tocRootPath /= tocSet.relativeroot;
		tocRootPath = system_complete(tocRootPath);

		for(std::string &file : tocSet.files)
		{
			BuildFileTask fileTask;
			fileTask.realpath = file;
			fileTask.realpath = system_complete(fileTask.realpath);	
			fileTask.name = fileTask.realpath.filename().generic_string();

			fileTask.filesize = uint32_t(file_size(fileTask.realpath));
			fileTask.compressMethod = tocSet.defcompression;

			bool skipfile = false;
			for(BuildFileSetting &fileSet: tocSet.buildFileSettings)
			{
				if (
					MatchTextWithWildcards(fileTask.name, fileSet.wildcard, false) &&
					(fileSet.minsize == -1 || fileTask.filesize >= uint32_t(fileSet.minsize)) &&
					(fileSet.maxsize == -1 || fileTask.filesize <= uint32_t(fileSet.maxsize))
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
				}
			}
			if(skipfile)
			{
				continue;
			}

			boost::filesystem::path relativePath = relative(fileTask.realpath, tocRootPath);
			relativePath = relativePath.remove_filename();
			BuildFolderTask *currentFolder = &tocTask.rootFolderTask;
			for (auto iter = relativePath.begin(); iter != relativePath.end(); ++iter)
			{
				bool found = false;
				for(BuildFolderTask &subFolderTask : currentFolder->subFolderTasks)
				{					
					if (
						boost::algorithm::to_lower_copy(subFolderTask.name) ==
						boost::algorithm::to_lower_copy(iter->generic_string())
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
					newFolder.name = iter->generic_string();
					auto pathEnd = iter;
					++pathEnd;

					boost::filesystem::path tmpPath("");
					for(auto iter2= relativePath.begin();iter2!= pathEnd;++iter2)
					{
						tmpPath /= *iter;
					}
					newFolder.fullpath = tmpPath.generic_string();
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

void BigFile::generate(
	boost::filesystem::path root,
	boost::filesystem::path build
)
{
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
	boost::algorithm::trim(tmpStr);
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
		if(command.command=="Archive")
		{
			_buildArchiveSetting.name = command.params["name"];
		}
		else if(command.command == "TOCStart")
		{
			BuildTOCSetting thisTOC;
			thisTOC.name = command.params["name"];
			thisTOC.alias = command.params["alias"];
			thisTOC.relativeroot = command.params["relativeroot"];

			command = getCommand(input);
			if (command.command == "FileSettingsStart")
			{
				thisTOC.defcompression =
					command.params["defcompression"] == "0" ? Uncompressed :
					command.params["defcompression"] == "1" ? Decompress_During_Read :
					command.params["defcompression"] == "2" ? Decompress_All_At_Once :
					throw FormatError("Buildfile format error");

				command = getCommand(input);
				while(command.command!="FileSettingsEnd")
				{
					BuildFileSetting thisFileSet;
					thisFileSet.wildcard = command.params["wildcard"];
					thisFileSet.minsize = typeConvert<std::string, int>(command.params["minsize"]);
					thisFileSet.maxsize = typeConvert<std::string, int>(command.params["minsize"]);
					if (command.command == "Override")
					{
						thisFileSet.command = BuildFileSetting::Override;
						thisFileSet.ct =
							command.params["ct"] == "0" ? Uncompressed :
							command.params["ct"] == "1" ? Decompress_During_Read :
							command.params["ct"] == "2" ? Decompress_All_At_Once :
							throw FormatError("Buildfile format error");
					}
					else if (command.command == "SkipFile")
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
			while (file != "TOCEnd")
			{
				thisTOC.files.push_back(file);
				getline(input, file);
			}
			_buildArchiveSetting.buildTOCSetting.push_back(thisTOC);
		}
	}
	input.close();
}
