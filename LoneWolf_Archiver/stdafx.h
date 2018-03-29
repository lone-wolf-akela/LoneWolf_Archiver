// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include <cstdio>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>
#include <tuple>
#include <random>

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/locale.hpp>

#include <zlib.h>

#include "exceptions.h"
#include "md5.h"
#include "ThreadPool/ThreadPool.h"