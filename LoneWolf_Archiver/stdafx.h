// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: 在此处引用程序需要的其他头文件
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>
#include <filesystem>

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/program_options.hpp>

#include <zlib.h>

#include "exceptions.h"
#include "md5.h"
#include "ThreadPool/ThreadPool.h"