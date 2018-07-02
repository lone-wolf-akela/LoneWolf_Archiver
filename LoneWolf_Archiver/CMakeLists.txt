# 项目信息
project (LoneWolf_Archiver)

#CMake min version
cmake_minimum_required (VERSION 2.8)

# 指定生成目标
add_executable(LoneWolf_Archiver 
								cipherstream.cpp 
								HWRM_BigFile.cpp 
								HWRM_BigFile_Internal.cpp 
								LoneWolfArchiver.cpp 
								memmapfilestream.cpp 
								stdafx.cpp
								)

#boost
set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME ON) 

find_package(Boost 1.67 COMPONENTS filesystem program_options locale iostreams REQUIRED)
if (Boost_FOUND)
	include_directories( ${Boost_INCLUDE_DIR} )
	target_link_libraries(LoneWolf_Archiver ${Boost_LIBRARIES})
endif(Boost_FOUND)

#pthread
find_package(Threads REQUIRED)  
if(THREADS_HAVE_PTHREAD_ARG)  
  set_property(TARGET LoneWolf_Archiver PROPERTY COMPILE_OPTIONS "-pthread")  
  set_property(TARGET LoneWolf_Archiver PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")  
endif(THREADS_HAVE_PTHREAD_ARG)  

target_link_libraries(LoneWolf_Archiver "${CMAKE_THREAD_LIBS_INIT}")

#zlib
find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    target_link_libraries( LoneWolf_Archiver ${ZLIB_LIBRARIES} )
endif( ZLIB_FOUND )

set(GCC_COVERAGE_COMPILE_FLAGS "-std=c++17 -O2")
				
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )


