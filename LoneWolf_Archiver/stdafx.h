#pragma once

//Has to define this to use boost with c++17
#define _HAS_AUTO_PTR_ETC 1

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
#include <cstddef>

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/locale.hpp>
#include <boost/hof.hpp>

#include <zlib.h>

#include <openssl/md5.h>

#include <json/json.h>

#include "exceptions.h"
#include "ThreadPool/ThreadPool.h"
#include "extension.h"

#define TOOL_SIG "E01519D6-2DB7-4640-AF54-0A23319C56C3"
#define ARCHIVE_SIG "DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF"