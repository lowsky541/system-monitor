#include "system.hpp"

// get cpu id and information, you can use `proc/cpuinfo`
std::string CPUInfo()
{
    char CPUBrandString[0x40];
    unsigned int CPUInfo[4] = {0, 0, 0, 0};

    // unix System
    // for windoes maybe we must add the following
    // __cpuid(regs, 0);
    // regs is the array of 4 positions
    __cpuid(0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
    unsigned int nExIds = CPUInfo[0];

    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    for (unsigned int i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(i, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);

        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }
    std::string str(CPUBrandString);
    return str;
}
std::string OperatingSystem()
{
#ifdef _WIN32
    return "Windows (32bit)";
#elif _WIN64
    return "Windows (64bit)";
#elif __APPLE__ || __MACH__
    return "Mac OSX";
#elif __linux__
    return "Linux";
#elif __FreeBSD__
    return "FreeBSD";
#elif __unix || __unix__
    return "Unix";
#else
    return "Other";
#endif
}
std::string Hostname()
{
    char hn[HOST_NAME_MAX];
    if (gethostname(hn, HOST_NAME_MAX) == 0) return hn;
    return std::string("<failure>");
}
std::string LoggedUser()
{
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (pw) return std::string(pw->pw_name);
    return std::string("<failure>");
}

float System::proc::cpu_diff(const cpu_stat_t &info1, const cpu_stat_t &info2, bool percent)
{
    uint64_t active1 = info1.user + info1.nice + info1.System + info1.idle + info1.iowait + info1.irq + info1.softirq;
    uint64_t idle1 = info1.idle;

    uint64_t active2 = info2.user + info2.nice + info2.System + info2.idle + info2.iowait + info2.irq + info2.softirq;
    uint64_t idle2 = info2.idle;

    return (float) (active2 - active1 - (idle2 - idle1)) / (active2 - active1) * (percent ? 100 : 0);
}

int System::proc::read_pstatm(const char *filepath, proc_pstatm_t &info)
{
    FILE *fp = fopen(filepath, "r");
    if (fp == NULL)
        return -1;

    if (fscanf(fp, "%ud %ud %ud %ud %ud %ud %ud",
               &info.size,
               &info.resident,
               &info.shared,
               &info.trs,
               &info.lrs,
               &info.drs,
               &info.dt) == EOF)
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    return 0;
}

int System::proc::read_pstat(const char *filepath, proc_pstat_t &info)
{
    FILE *fp = fopen(filepath, "r");
    if (fp == NULL)
        return -1;

    if (fscanf(fp, "%d ", &info.pid) == EOF) {
        fclose(fp);
        return -1;
    }

    std::string comm = "";

    char lc = '\0';
    for (;;) {
        char c = fgetc(fp);
        if (lc == ')' && c == ' ') {
            comm.pop_back(); 
            break;
        } else if (lc == '\0' && c == '(') {
            lc = c;
            continue;
        }
        else {
            comm += c;
            lc = c;
        }
    }

    if (fscanf(fp, " %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu"
                   "%lu %lu %lu %*d %*d %*d %*d %*u %lu %lu",
               &info.state,
               &info.utime,
               &info.stime,
               &info.cutime,
               &info.cstime,
               &info.vsize,
               &info.rss) == EOF)
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    memcpy(info.comm, comm.c_str(), comm.length());

    return 0;
}
int System::proc::read_stat(cpu_stat_t &info)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL)
        return false;

    char cpu[5];
    if (fscanf(fp, "%4s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
               cpu,
               &info.user,
               &info.nice,
               &info.System,
               &info.idle,
               &info.iowait,
               &info.irq,
               &info.softirq,
               &info.steal,
               &info.guest,
               &info.guest_nice) == EOF)
    {
        fclose(fp);
        return -1;
    }

    info.total = info.user +
                 info.nice +
                 info.System +
                 info.idle +
                 info.iowait +
                 info.irq +
                 info.softirq +
                 info.steal +
                 info.guest +
                 info.guest_nice;

    fclose(fp);

    return 0;
}
int System::proc::cpu_times(uint64_t &cpu_times_total)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL)
        return -1;

    uint64_t cpu_times[10];
    if (fscanf(fp, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
               &cpu_times[0], &cpu_times[1], &cpu_times[2], &cpu_times[3],
               &cpu_times[4], &cpu_times[5], &cpu_times[6], &cpu_times[7],
               &cpu_times[8], &cpu_times[9]) == EOF)
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    cpu_times_total = 0;
    for (size_t i = 0; i < 10; ++i)
        cpu_times_total += cpu_times[i];

    return 0;
}

std::string System::Power::Status()
{
    std::string status = "";

    std::ifstream input("/sys/class/power_supply/BAT0/status");
    if (!input.good()) return 0;

    input >> status;

    return status;
}

uint64_t System::Power::ChargeNow()
{
    float out = 0.0f;

    std::ifstream input("/sys/class/power_supply/BAT0/charge_now");
    if (!input.good()) {
        fprintf(stderr, "System::Power::ChargeNow(): cannot read charge_now.");
        return 0;
    }

    input >> out;

    return out;
}

uint64_t System::Power::ChargeFull()
{
    float out = 0.0f;

    std::ifstream input("/sys/class/power_supply/BAT0/charge_full");
    if (!input.good()) return 0;

    input >> out;

    return out;
}

std::filesystem::path System::Thermal::FindCPUSensor()
{
    for (const auto &sensor : std::filesystem::directory_iterator("/sys/class/hwmon"))
    {
        const std::filesystem::path labelPath = sensor.path() / "temp1_label";

        std::ifstream t(labelPath);
        if (t.good())
        {
            std::stringstream buffer;
            buffer << t.rdbuf();

            if (buffer.str().find("Package") != std::string::npos)
                return sensor.path();
        }
    }

    return "";
}

float System::Thermal::CPUTemperature(const std::filesystem::path &sensorPath)
{
    float out = 0.0f;

    std::ifstream input(sensorPath / "temp1_input");
    if (!input.good())
        return 0;

    input >> out;

    return out / 1000;
}

float System::Thermal::CPUTemperatureMax(const std::filesystem::path &sensorPath, float& max)
{
    float out = 0.0f;

    printf("%s\n", sensorPath.c_str());
    std::ifstream input(sensorPath / "temp1_max");
    if (!input.good())
        return 0;

    input >> out;

    return out / 1000;
}
