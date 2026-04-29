#include "metrics.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>


//
// Metrics collection functions
//

struct CpuData {
  long user, nice, system, idle, iowait, irq, softirq, steal;
};

CpuData read_cpu_data() {
  std::ifstream stat_file("/proc/stat");
  std::string line;
  std::getline(stat_file, line);
  
  std::istringstream iss(line);
  std::string cpu;
  
  CpuData data;
  iss >> cpu
      >> data.user
      >> data.nice
      >> data.system
      >> data.idle
      >> data.iowait
      >> data.irq
      >> data.softirq
      >> data.steal;

  return data;
}

double get_cpu_usage() {
  CpuData a = read_cpu_data();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  CpuData b = read_cpu_data();


  long idleA = a.idle + a.iowait;
  long idleB = b.idle + b.iowait;

  long totalA = a.user + a.nice + a.system + a.idle + a.iowait + a.irq + a.softirq + a.steal;
  long totalB = b.user + b.nice + b.system + b.idle + b.iowait + b.irq + b.softirq + b.steal;

  long totalDiff = totalB - totalA;
  long idleDiff = idleB - idleA;

  return 100.0 * (totalDiff - idleDiff) / totalDiff;
}

struct MemoryInfo {
  long totalKB;
  long availableKB;
  long usedKB;
  double usedPercent;
};

std::unordered_map<std::string, long> read_memory_info() {
  std::ifstream file("/proc/meminfo");
  std::string key;
  long value;
  std::string unit;

  std::unordered_map<std::string, long> mem;

  while (file >> key >> value >> unit) {
    key.pop_back(); // remove ':'
    mem[key] = value;
  }

  return mem;
}

MemoryInfo get_memory_info() {
  auto mem = read_memory_info();
  MemoryInfo info;
  info.totalKB = mem["MemTotal"];
  info.availableKB = mem["MemAvailable"];
  info.usedKB = info.totalKB - info.availableKB;
  info.usedPercent = 100.0 * info.usedKB / info.totalKB;
  return info;
}

double get_memory_usage() {
  auto info = get_memory_info();

  return info.usedPercent;
}