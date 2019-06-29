# proj info
project (LoneWolf_Archiver)

cmake_minimum_required (VERSION 3.14)

# use c++ 20
set (CMAKE_CXX_STANDARD 20)
if (MSVC)
    add_compile_options(/WX /utf-8)
else()
    add_compile_options(-Werror)
endif()

# source files
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
# need this for jsoncpp
find_package(jsoncpp REQUIRED)
target_link_libraries (LoneWolf_Archiver jsoncpp)

# installation
install (TARGETS LoneWolf_Archiver
	RUNTIME DESTINATION .)
install (FILES archive_config.json
	DESTINATION .)