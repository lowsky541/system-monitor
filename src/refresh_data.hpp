#ifndef __REFRESH_DATA_HPP__
#define __REFRESH_DATA_HPP__

#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <assert.h>
#include <cpuid.h>
#include <filesystem>
#include <fstream>
#include <ifaddrs.h>
#include <istream>
#include <limits.h>
#include <map>
#include <math.h>
#include <memory>
#include <mntent.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

struct cpu_stat_t {
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

struct storage_t {
  std::string device;
  uint64_t total;
  uint64_t used;
  float percent;
};

struct pstat_t {
  bool success;

  int pid;
  std::string comm;
  char state;
  int ppid;
  int pgrp;
  int session;
  int tty_nr;
  int tpgid;
  unsigned int flags;
  unsigned long minflt;
  unsigned long cminflt;
  unsigned long majflt;
  unsigned long cmajflt;
  unsigned long utime;
  unsigned long stime;
  long cutime;
  long cstime;
  long priority;
  long nice;
  long num_threads;
  long itrealvalue;
  unsigned long long starttime;
  unsigned long vsize;
  long rss;
  unsigned long rsslim;
  unsigned long startcode;
  unsigned long endcode;
  unsigned long startstack;
  unsigned long kstkesp;
  unsigned long kstkeip;
  unsigned long signal;
  unsigned long blocked;
  unsigned long sigignore;
  unsigned long sigcatch;
  unsigned long wchan;
  unsigned long nswap;
  unsigned short cnswap;
  int exit_signal;
  int processor;
  unsigned int rt_priority;
  unsigned int policy;
  unsigned long long delayacct_blkio_ticks;
};

struct pstatm_t {
  bool success;
  unsigned long size;     // total program size
  unsigned long resident; // resident set size
  unsigned long share;    // shared pages
  unsigned long text;     // text (code)
  unsigned long lib;      // library
  unsigned long data;     // data/stack
  unsigned long dt;       // dirty pages (unused in Linux 2.6)
  std::string cmd;        // command name
};

struct process_snap_t {
  pid_t pid;
  std::string name;
  char state;
  pstat_t pstat;
  pstatm_t pstatm;
};

struct process_t {
  pid_t pid;
  std::string name;
  char state;
  float cpu, mem;
  pstat_t pstat1, pstat2;
  pstatm_t pstatm1, pstatm2;
};

struct interface_t {
  std::string name;
  std::string addr;
  bool is_up;
  bool is_ipv6;
  std::array<uint32_t, 16> values;
};

class RefreshData {
public:
  static std::shared_ptr<RefreshData> init();

  void refresh_operating_system();
  void refresh_user();
  void refresh_hostname();
  void refresh_cpu_info();

  // Battery
  bool setup_refresh_battery(const char* procfile_now,
                             const char* procfile_full,
                             const char* procfile_status);
  void refresh_battery_full();
  void refresh_battery();

  // Thermal
  bool setup_refresh_thermal(const char* procfile);
  void refresh_thermal();

  // Fan
  bool setup_refresh_fan(const char* procfile);
  void refresh_fan();

  bool setup_proc();
  void refresh_cpu_stat(bool initial = false);
  void refresh_cpu_graph_stat(bool initial = false);
  void refresh_memory();

  void refresh_storages();
  void refresh_processes(bool initial = false);
  void refresh_interfaces();

  pid_t pid;
  std::string operating_system;
  std::string user;
  std::string hostname;
  std::string cpu_info;

  struct {
    cpu_stat_t last;
    cpu_stat_t current;
  } cpu;

  struct {
    cpu_stat_t last;
    cpu_stat_t current;
    std::array<float, 60> values;
  } cpu_graph;

  struct {
    uint64_t now;
    uint64_t full;
    std::string status;
    std::array<float, 60> values;
  } battery;

  struct {
    uint64_t current;
    std::array<float, 60> values;
  } thermal;

  struct {
    uint64_t current;
    std::array<float, 60> values;
  } fan;

  struct {
    float refresh_accumulator;
    float fps;
    bool animated;
    float yscale;
  } graph;

  struct {
    uint64_t phys_used;
    uint64_t phys_total;
    float phys_percent;

    uint64_t virt_used;
    uint64_t virt_total;
    float virt_percent;
  } memory;

  long pages;
  long processors;
  long page_size;
  unsigned long long total_memory;

  std::vector<storage_t> storages;

  struct {
    std::vector<process_snap_t> snap_past;
    std::vector<process_snap_t> snap_pres;
    std::vector<process_t> processes;
  } processes;

  struct {
    std::vector<interface_t> interfaces;
  } network;

  std::vector<pid_t> processes_selection;
  char processes_filter[64];

private:
  std::ifstream m_if_battery_now;
  std::ifstream m_if_battery_full;
  std::ifstream m_if_battery_status;
  std::ifstream m_if_thermal;
  std::ifstream m_if_fan;

  std::ifstream m_if_proc_stat;
  std::ifstream m_if_proc_meminfo;
};

#endif