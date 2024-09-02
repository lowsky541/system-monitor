#include "storage.hpp"

bool System::Memory::GetStatistics(StorageStats& physical_stats, StorageStats& virtual_stats)
{
    uint64_t MemTotal     = 0, MemFree  = 0, MemAvailable = 0;
    uint64_t SwapTotal    = 0, SwapFree = 0;
    uint64_t Buffers      = 0, Cached   = 0;
    uint64_t SReclaimable = 0;

    std::string line;
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.good()) return false;

    while (std::getline(meminfo, line))
    {
        std::istringstream iss(line);
        std::string name;

        if (line.rfind("MemTotal:", 0) != std::string::npos)
            iss >> name >> MemTotal;
        else if (line.rfind("MemFree:", 0) != std::string::npos)
            iss >> name >> MemFree;
        else if (line.rfind("MemAvailable:", 0) != std::string::npos)
            iss >> name >> MemAvailable;
        else if (line.rfind("Buffers:", 0) != std::string::npos)
            iss >> name >> Buffers;
        else if (line.rfind("Cached:", 0) != std::string::npos)
            iss >> name >> Cached;
        else if (line.rfind("SReclaimable:", 0) != std::string::npos)
            iss >> name >> SReclaimable;
        else if (line.rfind("SwapTotal:", 0) != std::string::npos)
            iss >> name >> SwapTotal;
        else if (line.rfind("SwapFree:", 0) != std::string::npos)
            iss >> name >> SwapFree;
    }

    physical_stats.total    = MemTotal * 1024;
    physical_stats.free     = MemFree * 1024;
    #if defined(USE_MEMAVAILABLE_CALC)
        physical_stats.used     = (MemTotal - MemAvailable) * 1024;
    #else
        const uint64_t buffers = Buffers;
        const uint64_t cache = Cached + SReclaimable;
        physical_stats.used     = (MemTotal - (MemFree + buffers + cache)) * 1024;
    #endif
    physical_stats.fraction = (double)((double)physical_stats.used / (double)physical_stats.total);

    virtual_stats.total    = SwapTotal * 1024;
    virtual_stats.free     = SwapFree * 1024;
    virtual_stats.used     = virtual_stats.total - virtual_stats.free;
    virtual_stats.fraction = (double)((double)virtual_stats.used / (double)virtual_stats.total);

    return true;
}

bool System::Disk::GetStatisticsFromMnt(const char *mountpoint, StorageStats &stats)
{
    struct statvfs buf;
    if (statvfs(mountpoint, &buf) != 0)
        return false;

    stats.total = buf.f_blocks * buf.f_frsize;
    stats.free = buf.f_bavail * buf.f_bsize;
    stats.used = stats.total - (buf.f_bfree * buf.f_frsize);
    stats.fraction = (double)stats.used / (double)stats.total;

    return true;
}

std::map<std::string, System::StorageStats> System::Disk::GetAllStatistics()
{
    std::map<std::string, System::StorageStats> storages;

    struct mntent *ent;
    std::vector<std::string> devices;

    for (std::filesystem::directory_entry entry : std::filesystem::directory_iterator("/dev/disk/by-path/"))
        devices.push_back(std::filesystem::read_symlink(entry).filename());

    std::sort(devices.begin(), devices.end());

    FILE* mounts = setmntent("/proc/mounts", "r");

    while (NULL != (ent = getmntent(mounts)))
    {
        for (const std::string& device : devices)
        {
            if (ent->mnt_fsname == std::string("/dev/" + device)) {
                struct System::StorageStats stats = {};
                if (System::Disk::GetStatisticsFromMnt(ent->mnt_dir, stats)) {
                    if (storages.count(device) == 0) {
                        storages[device] = stats;
                    }
                }else {
                    fprintf(stderr, "Failed to get disk statistics for device: %s.", device.c_str());
                }
            }
        }
    }

    endmntent(mounts);

    return storages;
}