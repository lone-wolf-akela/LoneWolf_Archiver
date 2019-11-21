#pragma once

using namespace System;
using namespace System::Collections::Generic;

#include "../LoneWolf_Archiver_Lib/export.h"

namespace LoneWolfArchiverDotnet
{
	public enum class MsgType{INFO, WARN, ERR};
	public delegate void ProgressCallback(MsgType type, String^ msg, int current, int max, String^ filename);
	
	public ref class Archiver
	{
	public:
		Archiver()
		{
			_interface = new libexport::Interface;
		}
		~Archiver()
		{
			this->!Archiver();
		}
		!Archiver()
		{
			delete _interface;
		}

		void Open(String^ file);
		void ExtractAll(String^ output_path, ProgressCallback^ callback);
		void ExtractFile(String^ output_path, String^ file_path, ProgressCallback^ callback);
		void ExtractFolder(String^ output_path, String^ folder_path, ProgressCallback^ callback);
		void ExtractToc(String^ output_path, String^ toc, ProgressCallback^ callback);
		String^ GetFiletree();
		void Generate(
			String^ root,
			bool all_in_one,
			String^ archivepath,
			int thread_num,
			bool encryption,
			int compress_level,
			bool keep_sign,
			IEnumerable<String^>^ ignore_list,
			uint_fast32_t seed
		);

	private:
		libexport::Interface* _interface;
	};
}
