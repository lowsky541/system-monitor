#include "refresh_data.hpp"

std::shared_ptr<RefreshData> RefreshData::init() {
  RefreshData* data = new RefreshData();
  data->graph.animated = true;
  data->graph.fps = 30;
  data->graph.yscale = 100.0;
  data->pid = getpid();

  data->pages = sysconf(_SC_PHYS_PAGES);
  data->processors = sysconf(_SC_NPROCESSORS_ONLN);
  data->page_size = sysconf(_SC_PAGE_SIZE);
  data->total_memory = data->pages * data->page_size;

  return std::shared_ptr<RefreshData>(data);
}

void RefreshData::refresh_operating_system() {
#ifdef _WIN32
  this->operating_system = "Windows (32bit)";
#elif _WIN64
  this->operating_system = "Windows (64bit)";
#elif __APPLE__ || __MACH__
  this->operating_system = "Mac OSX";
#elif __linux__
  this->operating_system = "Linux";
#elif __FreeBSD__
  this->operating_system = "FreeBSD";
#elif __unix || __unix__
  this->operating_system = "Unix";
#else
  this->operating_system = "Other";
#endif
}

void RefreshData::refresh_user() {
  uid_t uid = getuid();
  struct passwd* entry = getpwuid(uid);
  if (entry)
    this->user = std::string(entry->pw_name);
  else
    this->user = std::string("<could not get user>");
}

void RefreshData::refresh_hostname() {
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, HOST_NAME_MAX) == 0)
    this->hostname = std::string(hostname);
  else
    this->hostname = std::string("<could not get hostname>");
}

void RefreshData::refresh_cpu_info() {
  char CPUBrandString[0x40];
  unsigned int CPUInfo[4] = {0, 0, 0, 0};

  __cpuid(0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
  unsigned int nExIds = CPUInfo[0];

  memset(CPUBrandString, 0, sizeof(CPUBrandString));

  for (unsigned int i = 0x80000000; i <= nExIds; ++i) {
    __cpuid(i, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);

    if (i == 0x80000002)
      memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
    else if (i == 0x80000003)
      memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
    else if (i == 0x80000004)
      memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
  }

  this->cpu_info = std::string(CPUBrandString);
}

bool RefreshData::setup_refresh_battery(const char* procfile_now,
                                        const char* procfile_full,
                                        const char* procfile_status) {
  this->m_if_battery_now = std::ifstream(procfile_now);
  this->m_if_battery_full = std::ifstream(procfile_full);
  this->m_if_battery_status = std::ifstream(procfile_status);
  return m_if_battery_now.is_open() && m_if_battery_full.is_open() &&
         m_if_battery_status.is_open();
}

void RefreshData::refresh_battery_full() {
  uint64_t full;
  this->m_if_battery_full >> full;
  this->m_if_battery_full.seekg(std::ios::beg);

  this->battery.full = full;
}

void RefreshData::refresh_battery() {
  this->m_if_battery_now >> this->battery.now;
  this->m_if_battery_now.seekg(std::ios::beg);

  this->m_if_battery_status >> this->battery.status;
  this->m_if_battery_status.seekg(std::ios::beg);

  std::rotate(this->battery.values.begin(), this->battery.values.begin() + 1,
              this->battery.values.end());
  this->battery.values.back() =
      100.00f * ((float)this->battery.now / (float)this->battery.full);
}

bool RefreshData::setup_refresh_thermal(const char* procfile) {
  this->m_if_thermal = std::ifstream(procfile);
  return m_if_thermal.is_open();
}

void RefreshData::refresh_thermal() {
  uint64_t current;
  this->m_if_thermal >> current;
  this->m_if_thermal.seekg(std::ios::beg);

  std::rotate(this->thermal.values.begin(), this->thermal.values.begin() + 1,
              this->thermal.values.end());

  this->thermal.current = current / 1000;
  this->thermal.values.back() = this->thermal.current;
}

bool RefreshData::setup_refresh_fan(const char* procfile) {
  this->m_if_fan = std::ifstream(procfile);
  return m_if_fan.is_open();
}

void RefreshData::refresh_fan() {
  uint64_t current;
  this->m_if_fan >> current;
  this->m_if_fan.seekg(std::ios::beg);

  std::rotate(this->fan.values.begin(), this->fan.values.begin() + 1,
              this->fan.values.end());

  this->fan.current = current / 1000;
  this->fan.values.back() = this->fan.current;
}

bool RefreshData::setup_proc() {
  m_if_proc_stat = std::ifstream("/proc/stat");
  m_if_proc_meminfo = std::ifstream("/proc/meminfo");
  return m_if_proc_stat.is_open() && m_if_proc_meminfo.is_open();
}

float cpu_diff(const cpu_stat_t& info1, const cpu_stat_t& info2, bool percent) {
  uint64_t active1 = info1.user + info1.nice + info1.System + info1.idle +
                     info1.iowait + info1.irq + info1.softirq;
  uint64_t idle1 = info1.idle;

  uint64_t active2 = info2.user + info2.nice + info2.System + info2.idle +
                     info2.iowait + info2.irq + info2.softirq;
  uint64_t idle2 = info2.idle;

  return (float)(active2 - active1 - (idle2 - idle1)) / (active2 - active1) *
         (percent ? 100 : 0);
}

void RefreshData::refresh_cpu_graph_stat(bool initial) {
  cpu_stat_t info;
  std::string _name;

  m_if_proc_stat >> _name >> info.user >> info.nice >> info.System >>
      info.idle >> info.iowait >> info.irq >> info.softirq >> info.steal >>
      info.guest >> info.guest_nice;
  this->m_if_proc_stat.seekg(std::ios::beg);

  info.total = info.user + info.nice + info.System + info.idle + info.iowait +
               info.irq + info.softirq + info.steal + info.guest +
               info.guest_nice;

  const float cpu_usage =
      cpu_diff(this->cpu_graph.last, this->cpu_graph.current, true);
  std::rotate(this->cpu_graph.values.begin(),
              this->cpu_graph.values.begin() + 1, this->cpu_graph.values.end());

  this->cpu_graph.values.back() = cpu_usage;
  if (initial)
    this->cpu_graph.last = info;
  else {
    this->cpu_graph.last = this->cpu_graph.current;
    this->cpu_graph.current = info;
  }
}

void RefreshData::refresh_cpu_stat(bool initial) {
  cpu_stat_t info;
  std::string _name;

  m_if_proc_stat >> _name >> info.user >> info.nice >> info.System >>
      info.idle >> info.iowait >> info.irq >> info.softirq >> info.steal >>
      info.guest >> info.guest_nice;
  this->m_if_proc_stat.seekg(std::ios::beg);

  info.total = info.user + info.nice + info.System + info.idle + info.iowait +
               info.irq + info.softirq + info.steal + info.guest +
               info.guest_nice;

  if (initial) {
    this->cpu.last = info;
  } else {
    this->cpu.last = this->cpu.current;
    this->cpu.current = info;
  }
}

void RefreshData::refresh_memory() {
  uint64_t MemTotal = 0, MemFree = 0, MemAvailable = 0;
  uint64_t SwapTotal = 0, SwapFree = 0;
  uint64_t Buffers = 0, Cached = 0;
  uint64_t SReclaimable = 0;

  std::string line;
  while (std::getline(m_if_proc_meminfo, line)) {
    std::istringstream iss(line);
    std::string _name;

    if (line.rfind("MemTotal:", 0) != std::string::npos)
      iss >> _name >> MemTotal;
    else if (line.rfind("MemFree:", 0) != std::string::npos)
      iss >> _name >> MemFree;
    else if (line.rfind("MemAvailable:", 0) != std::string::npos)
      iss >> _name >> MemAvailable;
    else if (line.rfind("Buffers:", 0) != std::string::npos)
      iss >> _name >> Buffers;
    else if (line.rfind("Cached:", 0) != std::string::npos)
      iss >> _name >> Cached;
    else if (line.rfind("SReclaimable:", 0) != std::string::npos)
      iss >> _name >> SReclaimable;
    else if (line.rfind("SwapTotal:", 0) != std::string::npos)
      iss >> _name >> SwapTotal;
    else if (line.rfind("SwapFree:", 0) != std::string::npos)
      iss >> _name >> SwapFree;
  }

  this->m_if_proc_meminfo.clear();
  this->m_if_proc_meminfo.seekg(std::ios::beg);

  const uint64_t buffers = Buffers;
  const uint64_t cache = Cached + SReclaimable;

  const uint64_t phys_total = MemTotal * 1024;
  const uint64_t phys_used = (MemTotal - MemAvailable) * 1024;
  const float phys_percent = (float)((float)phys_used / (float)phys_total);

  const uint64_t virt_total = SwapTotal * 1024;
  const uint64_t virt_free = SwapFree * 1024;
  const uint64_t virt_used = virt_total - virt_free;
  const float virt_percent = (float)((float)virt_used / (float)virt_total);

  this->memory.phys_total = phys_total;
  this->memory.phys_used = phys_used;
  this->memory.phys_percent = phys_percent;

  this->memory.virt_total = virt_total;
  this->memory.virt_used = virt_used;
  this->memory.virt_percent = virt_percent;
}

void RefreshData::refresh_storages() {
  std::vector<storage_t> storages;

  struct mntent* ent;
  std::vector<std::string> devices;

  for (std::filesystem::directory_entry entry :
       std::filesystem::directory_iterator("/dev/disk/by-path/"))
    devices.push_back(std::filesystem::is_symlink(entry)
                          ? std::filesystem::read_symlink(entry).filename()
                          : entry);

  FILE* mounts = setmntent("/proc/mounts", "r");
  while (NULL != (ent = getmntent(mounts))) {
    for (const std::string& device : devices) {
      if (ent->mnt_fsname == std::string("/dev/" + device)) {

        struct statvfs buf;
        if (statvfs(ent->mnt_dir, &buf) != 0)
          fprintf(stderr, "could not get stats about storage %s",
                  device.c_str());

        storage_t dev_stats;
        dev_stats.device = device;
        dev_stats.total = buf.f_blocks * buf.f_frsize;
        dev_stats.used = dev_stats.total - (buf.f_bfree * buf.f_frsize);
        dev_stats.percent = (float)dev_stats.used / (float)dev_stats.total;

        storages.push_back(dev_stats);
      }
    }
  }

  endmntent(mounts);

  std::sort(storages.begin(), storages.end(),
            [](storage_t a, storage_t b) { return a.device < b.device; });

  this->storages = storages;
}

pstat_t read_pstat(int pid) {
  std::ifstream statFile("/proc/" + std::to_string(pid) + "/stat");
  pstat_t process_stat;

  if (!statFile.is_open())
    return pstat_t{};

  std::string line;
  std::getline(statFile, line);
  std::istringstream iss(line);

  iss >> process_stat.pid; // Read PID first

  // Read comm field, which is enclosed in parentheses
  if (line.find('(') != std::string::npos) {
    size_t start = line.find('(') + 1;
    size_t end = line.rfind(')');
    process_stat.comm = line.substr(start, end - start);
    iss.seekg((end - start) + 3, std::ios::cur);
  }

  // Read other fields as before
  iss >> process_stat.state >> process_stat.ppid >> process_stat.pgrp >>
      process_stat.session >> process_stat.tty_nr >> process_stat.tpgid >>
      process_stat.flags >> process_stat.minflt >> process_stat.cminflt >>
      process_stat.majflt >> process_stat.cmajflt >> process_stat.utime >>
      process_stat.stime >> process_stat.cutime >> process_stat.cstime >>
      process_stat.priority >> process_stat.nice >> process_stat.num_threads >>
      process_stat.itrealvalue >> process_stat.starttime >>
      process_stat.vsize >> process_stat.rss >> process_stat.rsslim >>
      process_stat.startcode >> process_stat.endcode >>
      process_stat.startstack >> process_stat.kstkesp >> process_stat.kstkeip >>
      process_stat.signal >> process_stat.blocked >> process_stat.sigignore >>
      process_stat.sigcatch >> process_stat.wchan >> process_stat.nswap >>
      process_stat.cnswap >> process_stat.exit_signal >>
      process_stat.processor >> process_stat.rt_priority >>
      process_stat.policy >> process_stat.delayacct_blkio_ticks;

  process_stat.success = true;
  return process_stat;
}

pstatm_t read_pstatm(int pid) {
  std::ifstream statmFile("/proc/" + std::to_string(pid) + "/statm");
  pstatm_t process_stat_m;

  if (!statmFile.is_open())
    return pstatm_t{};

  std::string line;
  std::getline(statmFile, line);
  std::istringstream iss(line);

  iss >> process_stat_m.size >> process_stat_m.resident >>
      process_stat_m.share >> process_stat_m.text >> process_stat_m.lib >>
      process_stat_m.data >> process_stat_m.dt;

  // Read the rest of the line as the command name (cmd)
  std::getline(iss, process_stat_m.cmd);

  process_stat_m.success = true;
  return process_stat_m;
}

void RefreshData::refresh_processes(bool initial) {
  std::vector<process_snap_t> snapshot;

  for (const auto& e : std::filesystem::directory_iterator(("/proc"))) {
    std::filesystem::path path = e.path();
    std::string filename = path.filename().string();

    if (std::all_of(filename.begin(), filename.end(), ::isdigit)) {
      pid_t pid = std::atoi(filename.c_str());

      if (pid != 0) {
        std::string stp = "/proc/" + std::to_string(pid) + "/stat";
        std::string stmp = "/proc/" + std::to_string(pid) + "/statm";

        pstat_t st = read_pstat(pid);
        pstatm_t stm = read_pstatm(pid);
        if (!st.success || !stm.success)
          continue;

        process_snap_t p = {};
        p.pid = st.pid;
        p.name = st.comm;
        p.state = st.state;
        p.pstat = st;
        p.pstatm = stm;

        snapshot.push_back(p);
      }
    }
  }

  if (initial) {
    this->processes.snap_past = snapshot;
    return;
  } else {
    this->processes.snap_past = this->processes.snap_pres;
    this->processes.snap_pres = snapshot;
  }

  std::vector<process_t> processes;
  for (auto pres : this->processes.snap_pres) {
    const auto last_process_it = std::find_if(
        this->processes.snap_past.begin(), this->processes.snap_past.end(),
        [&](const process_snap_t& p) { return p.pid == pres.pid; });

    if (last_process_it != this->processes.snap_past.end()) {
      const auto past = *last_process_it;

      uint32_t cpu_times_past = past.pstat.utime + past.pstat.stime;
      uint32_t cpu_times_pres = pres.pstat.utime + pres.pstat.stime;

      float cpu_usage = (processors * (cpu_times_pres - cpu_times_past) * 100) /
                        (float)(this->cpu.current.total - this->cpu.last.total);

      float resident = pres.pstat.rss * this->page_size;
      float mem_usage = 100 * (resident / this->total_memory);

      processes.push_back(process_t{
          pres.pid,
          pres.name,
          pres.state,
          cpu_usage,
          mem_usage,
          pres.pstat,
          past.pstat,
          pres.pstatm,
          past.pstatm,
      });
    } else {
      processes.push_back(process_t{
          pres.pid,
          pres.name,
          pres.state,
          0.0,
          0.0,
          pres.pstat,
          pstat_t{},
          pres.pstatm,
          pstatm_t{},
      });
    }
  }

  this->processes.processes = processes;
}

std::map<std::string, std::array<uint32_t, 16>> interfaces_values() {
  std::map<std::string, std::array<uint32_t, 16>> devices;

  char* buffer = (char*)malloc(IFNAMSIZ * sizeof(char));
  std::array<uint32_t, 16> values;

  size_t line_length = 256;
  char* line = (char*)malloc(line_length * sizeof(char));

  FILE* fp = fopen("/proc/net/dev", "r");

  getline((char**)&line, &line_length, fp);
  getline((char**)&line, &line_length, fp);

  while (getline((char**)&line, &line_length, fp) != -1) {
    char* ptr = strtok(line, " :");
    for (int i = 0; ptr != NULL; i++) {
      if (i == 0)
        strncpy(buffer, ptr, IFNAMSIZ);
      else
        values[i - 1] = strtol(ptr, NULL, 10);
      ptr = strtok(NULL, " :");
    }

    const std::string name = std::string(buffer);
    devices[name] = values;
  }

  free(line);
  free(buffer);

  fclose(fp);

  return devices;
}

void RefreshData::refresh_interfaces() {
  std::vector<interface_t> interfaces;

  struct ifaddrs *ifap, *ifa;
  getifaddrs(&ifap);

  for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr) {
      interface_t interface;
      interface.name = ifa->ifa_name;
      interface.is_up = (ifa->ifa_flags & IFF_UP) != 0;

      if (ifa->ifa_addr->sa_family == AF_INET) {
        char addr[INET_ADDRSTRLEN];
        struct sockaddr_in* in = (struct sockaddr_in*)ifa->ifa_addr;
        inet_ntop(AF_INET, &in->sin_addr, addr, sizeof(addr));
        interface.addr = addr;
        interface.is_ipv6 = false;
      } else if (ifa->ifa_addr->sa_family == AF_INET6) {
        char addr[INET6_ADDRSTRLEN];
        struct sockaddr_in6* in6 = (struct sockaddr_in6*)ifa->ifa_addr;
        inet_ntop(AF_INET6, &in6->sin6_addr, addr, sizeof(addr));
        interface.addr = addr;
        interface.is_ipv6 = true;
      } else
        continue;

      interfaces.push_back(interface);
    }
  }

  std::sort(interfaces.begin(), interfaces.end(),
            [](const interface_t& i1, const interface_t& i2) {
              return i1.name < i2.name;
            });

  auto ivs = interfaces_values();
  for (auto& interface : interfaces) {
    const auto it = ivs.find(interface.name);
    if (it != ivs.end()) {
      interface.values = (*it).second;
    }
  }

  this->network.interfaces = interfaces;
}
