# CMakeList.txt: LoneWolf_Archiver 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

# proj info
project (LoneWolf_Archiver)

cmake_minimum_required (VERSION 3.14)

# use c++ 20
set (CMAKE_CXX_STANDARD 20)

# 将源代码添加到此项目的可执行文件。
add_executable (LoneWolf_Archiver 
	"buildfile/convert2utf8.cpp"
	"buildfile/genfile.cpp"
	"buildfile/parsefile.cpp"
	"buildfile/scanfiles.cpp"
	"stream/cipherstream.cpp" 
	"stream/memmapfilestream.cpp"
	"HWRM_BigFile.cpp"
	"HWRM_BigFile_Internal.cpp"
	"LoneWolfArchiver.cpp"
	"linuxfix.cpp")

# boost
set (Boost_USE_STATIC_LIBS ON) 
set (Boost_USE_MULTITHREADED ON)  
set (Boost_USE_STATIC_RUNTIME ON) 
find_package (Boost 1.70 COMPONENTS program_options locale iostreams REQUIRED)
include_directories (${Boost_INCLUDE_DIR})
target_link_libraries (LoneWolf_Archiver ${Boost_LIBRARIES})

# pthread
find_package (Threads REQUIRED)  
if (THREADS_HAVE_PTHREAD_ARG)  
	set_property (TARGET LoneWolf_Archiver PROPERTY COMPILE_OPTIONS "-pthread")  
	set_property (TARGET LoneWolf_Archiver PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")  
endif (THREADS_HAVE_PTHREAD_ARG)  
target_link_libraries (LoneWolf_Archiver "${CMAKE_THREAD_LIBS_INIT}")

# zlib
find_package (ZLIB REQUIRED)
include_directories (${ZLIB_INCLUDE_DIRS})
target_link_libraries (LoneWolf_Archiver ${ZLIB_LIBRARIES})

# openssl
find_package (OpenSSL REQUIRED)
include_directories (${OPENSSL_INCLUDE_DIR})
target_link_libraries (LoneWolf_Archiver ${OPENSSL_CRYPTO_LIBRARY})

# jsoncpp
find_package(jsoncpp REQUIRED)
target_link_libraries (LoneWolf_Archiver jsoncpp_lib)

# copy files
add_custom_command (TARGET LoneWolf_Archiver POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_SOURCE_DIR}/archive_config.json
	${CMAKE_CURRENT_BINARY_DIR}/archive_config.json)
add_custom_command (TARGET LoneWolf_Archiver POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_SOURCE_DIR}/Drag_Folder_Here_to_Make_an_Archive.bat
	${CMAKE_CURRENT_BINARY_DIR}/Drag_Folder_Here_to_Make_an_Archive.bat)