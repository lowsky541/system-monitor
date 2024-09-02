#ifndef __UTILITIES_HPP__
#define __UTILITIES_HPP__

#include <numeric>
#include <stdint.h>
#include <string>
#include <utility>

std::string human_readable(uint64_t bytes);
std::string human_readable_megabyte(uint64_t bytes);
float average(const float* v, const int l);

#endif