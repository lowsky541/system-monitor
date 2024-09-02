#include "processes.hpp"

/*
bool System::Processes::ReadStatus(std::string pid, std::string& name, std::string& state)
{
    std::string line;
    std::ifstream fstatus("/proc/" + pid + "/status");
    if (!fstatus.good()) return false;

    while (std::getline(fstatus, line))
    {
        std::string _name;

        std::istringstream iss(line);
        if (line.rfind("Name:", 0) != std::string::npos)
            iss >> _name >> name;
        else if (line.rfind("State:", 0) != std::string::npos)
            iss >> _name >> state;
    }


    if (state.compare("R") == 0) {
        state = "Running";
    } else if (state.compare("S") == 0) {
        state = "Sleeping (Interruptable)";
    } else if (state.compare("D") == 0) {
        state = "Sleeping (Uninterruptable)";
    } else if (state.compare("Z") == 0) {
        state = "Zombie";
    } else if (state.compare("T") == 0) {
        state = "Stopped";
    } else if (state.compare("t") == 0) {
        state = "Tracing stop";
    } else if (state.compare("W") == 0) {
        state = "Paging/Waking";
    } else if (state.compare("X") == 0) {
        state = "Dead";
    } else if (state.compare("x") == 0) {
        state = "Dead";
    } else if (state.compare("K") == 0) {
        state = "Wakekill";
    } else if (state.compare("P") == 0) {
        state = "Parked";
    } else if (state.compare("I") == 0) {
        state = "Idle kernel thread";
    }

    return true;
}
*/

std::map<pid_t, process_t> System::Processes()
{
    std::map<pid_t, process_t> ps;
    for (const auto &e : std::filesystem::directory_iterator(("/proc")))
    {
        std::filesystem::path path = e.path();
        std::string filename = path.filename().string();
        if (std::all_of(filename.begin(), filename.end(), ::isdigit))
        {
            pid_t pid = std::atoi(filename.c_str());
            if (pid != 0)
            {
                proc_pstat_t st = {};
                proc_pstatm_t stm = {};
                std::string stp = "/proc/" + std::to_string(pid) + "/stat";
                std::string stmp = "/proc/" + std::to_string(pid) + "/statm";

                if (read_pstat(stp.c_str(), st) != 0 || read_pstatm(stmp.c_str(), stm) != 0)
                    return std::map<pid_t, process_t>();

                process_t p = {};
                p.pid = st.pid;
                p.name = st.comm;
                p.state = st.state;
                p.proc.pstat = st;
                p.proc.pstatm = stm;

                ps[pid] = p;
            }
        }
    }

    return ps;
}