#ifndef __IG_HUMANIZE_HPP__
#define __IG_HUMANIZE_HPP__

#include <string>
#include <stdint.h>

std::string human_readable(uint64_t bytes);
std::string human_readable_megabyte(uint64_t bytes);

#endif