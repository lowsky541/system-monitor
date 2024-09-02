#include "utilities.hpp"

std::string human_readable(uint64_t bytes) {
  char suffix[][3] = {"B", "KB", "MB", "GB", "TB"};
  char length = sizeof(suffix) / sizeof(suffix[0]);

  int i = 0;
  double dblBytes = bytes;

  if (bytes > 1024) {
    for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024)
      dblBytes = bytes / 1024.0;
  }

  static char output[200];
  sprintf(output, "%.02lf %s", dblBytes, suffix[i]);
  return std::string(output);
}

std::string human_readable_megabyte(uint64_t bytes) {
  char suffix[][3] = {"B", "KB", "MB", "GB", "TB"};
  char length = sizeof(suffix) / sizeof(suffix[0]);

  int i = 0;
  double dblBytes = bytes;

  if (bytes > 1000) {
    for (i = 0; (bytes / 1000) > 0 && i < length - 1; i++, bytes /= 1000)
      dblBytes = bytes / 1000.0;
  }

  static char output[200];
  sprintf(output, "%.02lf %s", dblBytes, suffix[i]);
  return std::string(output);
}

float average(const float* v, const int l) {
  float sum = 0;
  for (int i = 0; i < l; i++)
    sum += v[i];

  return sum / l;
}
