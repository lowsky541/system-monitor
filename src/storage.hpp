#ifndef __STORAGE_HPP__
#define __STORAGE_HPP__

// standard includes
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <map>

#if defined(_WIN32) || defined(_WIN64)
#else
    #include <sys/statvfs.h>
    #include <sys/sysinfo.h>
    #include <mntent.h>
    #include <stdint.h>
#endif

namespace fs = std::filesystem;

namespace System
{
    struct StorageStats
    {
        uint64_t used;
        uint64_t free;
        uint64_t total;
        double   fraction;
    };

    namespace Memory
    {
        bool GetStatistics(StorageStats& physical_stats, StorageStats& virtual_stats);
    };

    namespace Disk
    {
        bool GetStatisticsFromMnt(const char* mountpoint, StorageStats& stats);
        std::map<std::string, System::StorageStats> GetAllStatistics();
    };
}


#endif