#pragma once
#include "stdafx.h"
#include "HWRM_BigFile_Internal.h"

struct BuildfileCommand
{
	std::string command;
	std::unordered_map<std::string, std::string> params;
};
struct BuildFileSetting
{
	enum{ Override , SkipFile} command;
	std::string wildcard;
	int minsize;
	int maxsize;
	CompressMethod ct;
};
struct BuildTOCSetting
{
	std::string name;
	std::string alias;
	std::string relativeroot;
	CompressMethod defcompression;
	std::vector<BuildFileSetting> buildFileSettings;
	std::vector<std::wstring> files;
};
struct BuildArchiveSetting
{
	std::string name;
	std::vector<BuildTOCSetting> buildTOCSetting;
};

class BigFile
{
public:
	BigFile() = default;
	BigFile(boost::filesystem::path file, BigFileState state = Read)
	{
		open(file, state);
	}

	void open(boost::filesystem::path file, BigFileState state = Read);
	void close(void);
	void setCoreNum(unsigned coreNum);
	void setCompressLevel(int level);
	void skipToolSignature(bool skip);
	void writeEncryption(bool enc);

	void extract(boost::filesystem::path directory);
	void listFiles(void);
	void testArchive(void);
	void create(boost::filesystem::path root, boost::filesystem::path build);
	void generate(boost::filesystem::path file, boost::filesystem::path root, bool allInOne);
	std::string getArchiveSignature(void) const;

	~BigFile(void)
	{
		close();
	}
private:
	BigFile_Internal _internal;
	BuildArchiveSetting _buildArchiveSetting;
	std::vector<std::string> _archiveIgnoreSet;

	void _parseBuildfile(boost::filesystem::path buildfile);
};
