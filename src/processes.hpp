#ifndef __IG_PROCESSES_HPP__
#define __IG_PROCESSES_HPP__

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <string>
#include <fstream>
#include <map>

#include "system.hpp"

using System::proc::process_t;
using System::proc::proc_pstat_t;
using System::proc::proc_pstatm_t;

namespace System
{
    std::map<pid_t, process_t> Processes();
}

#endif