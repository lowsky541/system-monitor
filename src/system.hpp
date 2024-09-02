#ifndef __IG_SYSTEM_HPP__
#define __IG_SYSTEM_HPP__

#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>

#if defined(_WIN32) || defined(_WIN64)
// for windows
#else
#include <cpuid.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#endif

std::string CPUInfo();
std::string OperatingSystem();
std::string Hostname();
std::string LoggedUser();

namespace System
{
    namespace proc
    {
        struct cpu_stat_t
        {
            uint64_t user;
            uint64_t nice;
            uint64_t System;
            uint64_t idle;
            uint64_t iowait;
            uint64_t irq;
            uint64_t softirq;
            uint64_t steal;
            uint64_t guest;
            uint64_t guest_nice;
            uint64_t total;
        };

        struct proc_pstat_t
        {
            pid_t pid;
            char comm[256];
            char state;
            uint64_t utime;
            uint64_t cutime;
            uint64_t stime;
            uint64_t cstime;
            uint64_t vsize;
            uint64_t rss;
        };

        struct proc_pstatm_t
        {
            uint32_t size;
            uint32_t resident;
            uint32_t shared;
            uint32_t trs;
            uint32_t lrs;
            uint32_t drs;
            uint32_t dt;
        };

        struct process_t
        {
            pid_t pid;
            std::string name;
            char state;

            struct {
                float mem;
                float cpu;
            } usage;

            struct {
                struct proc_pstat_t pstat;
                struct proc_pstatm_t pstatm;
            } proc;
        };

        int   read_pstatm(const char *filepath, struct proc_pstatm_t &info);
        int   read_pstat(const char *filepath, struct proc_pstat_t &info);
        int   read_stat(struct cpu_stat_t &info);
        float cpu_diff(const cpu_stat_t &info1, const cpu_stat_t &info2, bool percent);
        int   cpu_times(uint64_t &cpu_times_total);
    };

    namespace Power
    {
        std::string Status();
        uint64_t ChargeNow();
        uint64_t ChargeFull();
    }

    namespace Thermal
    {
        float CPUTemperature(const std::filesystem::path &sensorPath);
        float CPUTemperatureMax(const std::filesystem::path &sensorPath, float &max);
        std::filesystem::path FindCPUSensor();
    }
}

#endif
