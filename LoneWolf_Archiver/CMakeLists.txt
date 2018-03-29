# CMake 最低版本号要求
cmake_minimum_required (VERSION 2.8)

# 项目信息
project (LoneWolf_Archiver)

# 指定生成目标
add_executable(LoneWolf_Archiver 
								cipherstream.cpp 
								HWRM_BigFile.cpp 
								HWRM_BigFile_Internal.cpp 
								LoneWolfArchiver.cpp 
								md5.cpp 
								memmapfilestream.cpp 
								stdafx.cpp
								)

SET(GCC_COVERAGE_COMPILE_FLAGS "-std=c++14 -o2")	
								
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )
