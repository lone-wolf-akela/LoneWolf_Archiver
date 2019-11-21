#include <string>
#include <vector>

#include <msclr/marshal_cppstd.h>

#include "LoneWolf_Archiver_Dotnet.h"

namespace
{
	using LoneWolfArchiverDotnet::ProgressCallback;
	using msclr::interop::marshal_as;

	auto to_cpp_callback(ProgressCallback^ callback)
	{
		gcroot<ProgressCallback^> c;
		return [c](int current, int max, std::string filename)
		{
			c->Invoke(current, max, marshal_as<String^>(filename));
		};
	}
}

namespace LoneWolfArchiverDotnet
{
	using msclr::interop::marshal_as;

	void Archiver::Open(String^ file)
	{
		_interface->Open(marshal_as<std::wstring>(file));
	}

	void Archiver::ExtractAll(String^ output_path, ProgressCallback^ callback)
	{
		_interface->ExtractAll(
			marshal_as<std::wstring>(output_path),
			to_cpp_callback(callback)
		);
	}

	void Archiver::ExtractFile(String^ output_path, String^ file_path, ProgressCallback^ callback)
	{
		_interface->ExtractFile(
			marshal_as<std::wstring>(output_path),
			marshal_as<std::wstring>(file_path),
			to_cpp_callback(callback)
		);
	}

	void Archiver::ExtractFolder(String^ output_path, String^ folder_path, ProgressCallback^ callback)
	{
		_interface->ExtractFolder(
			marshal_as<std::wstring>(output_path),
			marshal_as<std::wstring>(folder_path),
			to_cpp_callback(callback)
		);
	}

	void Archiver::ExtractToc(String^ output_path, String^ toc, ProgressCallback^ callback)
	{
		_interface->ExtractToc(
			marshal_as<std::wstring>(output_path),
			marshal_as<std::wstring>(toc),
			to_cpp_callback(callback)
		);
	}

	String^ Archiver::GetFiletree()
	{
		std::string str = _interface->GetFiletree();
		array<Byte>^ data = gcnew array<Byte>(int(str.length()));
		Runtime::InteropServices::Marshal::Copy(
			IntPtr(str.data()), data, 0, int(str.length()));
		return Text::Encoding::UTF8->GetString(data);
	}

	void Archiver::Generate(
		String^ root,
		bool all_in_one,
		String^ archivepath,
		int thread_num,
		bool encryption,
		int compress_level,
		bool keep_sign,
		IEnumerable<String^>^ ignore_list,
		uint_fast32_t seed
	)
	{
		std::vector<std::wstring> v_ignorelist;
		for each (auto l in ignore_list)
		{
			v_ignorelist.emplace_back(marshal_as<std::wstring>(l));
		}

		_interface->Generate(
			marshal_as<std::wstring>(root),
			all_in_one,
			marshal_as<std::wstring>(archivepath),
			thread_num,
			encryption,
			compress_level,
			keep_sign,
			v_ignorelist,
			seed
		);
	}
}
