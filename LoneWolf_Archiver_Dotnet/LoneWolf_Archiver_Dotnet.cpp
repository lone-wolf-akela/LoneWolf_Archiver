﻿#include <string>
#include <vector>

#include <msclr/marshal_cppstd.h>

#include "LoneWolf_Archiver_Dotnet.h"

namespace
{
	using LoneWolfArchiverDotnet::ProgressCallback;
	using msclr::interop::marshal_as;

	auto to_cpp_callback(ProgressCallback^ callback)
	{
		const gcroot<ProgressCallback^> c = callback;
		return [c](
			libexport::MsgType type,
			std::optional<std::string> msg,
			int current, int max,
			std::optional<std::string> filename)
		{
			LoneWolfArchiverDotnet::MsgType managed_type;
			switch (type)
			{
			case libexport::INFO:
				managed_type = LoneWolfArchiverDotnet::MsgType::INFO;
				break;
			case libexport::WARN:
				managed_type = LoneWolfArchiverDotnet::MsgType::WARN;
				break;
			default: /*case libexport::ERR*/
				managed_type = LoneWolfArchiverDotnet::MsgType::ERR;
				break;
			}
			const auto managed_msg = msg.has_value() ? marshal_as<String^>(*msg) : nullptr;
			const auto managed_filename = filename.has_value() ? marshal_as<String^>(*filename) : nullptr;
			c->Invoke(managed_type, managed_msg, current, max, managed_filename);
		};
	}
}

#define FORWARD_EXCEPTION(code) \
	try{ code } \
	catch(const std::exception& e) \
	{ throw gcnew Exception(marshal_as<String^>(e.what())); }

namespace LoneWolfArchiverDotnet
{
	using msclr::interop::marshal_as;

	void Archiver::Open(String^ file)
	{
		FORWARD_EXCEPTION(_interface->Open(marshal_as<std::wstring>(file));)
	}

	void Archiver::ExtractAll(String^ output_path, ProgressCallback^ callback)
	{
		FORWARD_EXCEPTION(_interface->ExtractAll(
			marshal_as<std::wstring>(output_path),
			to_cpp_callback(callback));
		)
	}

	void Archiver::ExtractFile(String^ output_path, String^ file_path, ProgressCallback^ callback)
	{
		FORWARD_EXCEPTION(_interface->ExtractFile(
			marshal_as<std::wstring>(output_path),
			marshal_as<std::wstring>(file_path),
			to_cpp_callback(callback));
		)
	}

	void Archiver::ExtractFolder(String^ output_path, String^ folder_path, ProgressCallback^ callback)
	{
		FORWARD_EXCEPTION(_interface->ExtractFolder(
			marshal_as<std::wstring>(output_path),
			marshal_as<std::wstring>(folder_path),
			to_cpp_callback(callback));
		)
	}

	void Archiver::ExtractToc(String^ output_path, String^ toc, ProgressCallback^ callback)
	{
		FORWARD_EXCEPTION(_interface->ExtractToc(
			marshal_as<std::wstring>(output_path),
			marshal_as<std::wstring>(toc),
			to_cpp_callback(callback));
		)
	}

	String^ Archiver::GetFiletree()
	{
		FORWARD_EXCEPTION(
			std::string str = _interface->GetFiletree();
		array<Byte> ^ data = gcnew array<Byte>(int(str.length()));
		Runtime::InteropServices::Marshal::Copy(
			IntPtr(str.data()), data, 0, int(str.length()));
		return Text::Encoding::UTF8->GetString(data);
		)
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