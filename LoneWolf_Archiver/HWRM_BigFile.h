/**\file HWRM_BigFile.h
 * \brief	core class HWRM_BigFile.
 *			this class contains mostly operation interface.
 *			inner workers are in HWRM_BigFile_Internal
 */

#pragma once
#include "stdafx.h"
#include "HWRM_BigFile_Internal.h"

/**
 * \brief Represents a line in a build config file.
 */
struct BuildfileCommand
{
	///An action command, like "FileSettingsStart" and "Override"
	std::string command;

	///Command param pairs like 'wildcard="*.mp3"' and 'defcompression="1"' 
	std::unordered_map<std::string, std::string> params;
};


/**
 * \brief	Represents each line in a build config file between
 *			"FileSettingsStart" and "FileSettingsEnd"
 */
struct BuildFileSetting
{
	///command type: use special setting for these files ("Override") or skip these files and not pack them into the archive ("SkipFile")
	enum{ Override , SkipFile} command = Override;

	///filename rule
	std::string wildcard;

	///min file size
	int minsize = 0;
	
	///max file size
	int maxsize = 0;

	///compression Method
	CompressMethod ct = Uncompressed;
};


/**
 * \brief	Represents each line in a build config file between
 *			"TOCStart" and "TOCEnd"
 */
struct BuildTOCSetting
{
	///name of the TOC
	std::string name;

	///alias of the TOC, aka "data" or "locale"
	std::string alias;

	///root path of this TOC relative to the project root path
	std::string relativeroot;

	///default compression Method
	CompressMethod defcompression = Uncompressed;

	///a list of all BuildFileSetting in this TOC
	std::vector<BuildFileSetting> buildFileSettings;

	///a list of all files in this TOC
	/**
	 * Attention, we must use wstring here, to prevent problems with non-Ascii filenames
	 */
	std::vector<std::wstring> files;
};


/**
 * \brief Represents a whole build config file
 */
struct BuildArchiveSetting
{
	///archive name
	std::string name;

	///a list of all BuildTOCSetting in this archive
	std::vector<BuildTOCSetting> buildTOCSetting;
};


/**
 * \brief	.Big file class.
 *			Provides methods to create, read and do other things to a .big file 
 */
class BigFile
{
public:
	BigFile() = default;
	BigFile(boost::filesystem::path const &file, BigFileState state = Read)
	{
		open(file, state);
	}

	void open(boost::filesystem::path const &file, BigFileState state = Read);
	void close(void);
	void setCoreNum(unsigned coreNum);
	void setCompressLevel(int level);
	void skipToolSignature(bool skip);
	void writeEncryption(bool enc);
	void setIgnoreList(std::vector<std::string> list);

	void extract(boost::filesystem::path const &directory);
	void listFiles(void);
	void testArchive(void);
	void create(
		boost::filesystem::path root, 
		boost::filesystem::path const &build
	);
	void generate(
		boost::filesystem::path file,
		boost::filesystem::path const &root, 
		bool allInOne
	);
	std::string getArchiveSignature(void) const;

	~BigFile(void)
	{
		close();
	}
private:
	///a BigFile_Internal, which provides internal functions for .big file operations
	BigFile_Internal _internal;

	///stores BuildArchiveSetting of this class. not used when read from an already existed .big file
	BuildArchiveSetting _buildArchiveSetting;
	
	///stores files to ignore when create a .big files. This settings are read from ArchiveIgnore.txt
	std::vector<std::string> _archiveIgnoreSet;

	void _parseBuildfile(boost::filesystem::path buildfile);
};
