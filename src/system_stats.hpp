#pragma once
#include <string>
#include <cstdint>

struct SystemStats {
    double cpuUsage = 0.0;
    double memoryUsage = 0.0;
    std::uint64_t memoryUsedMB = 0;
    std::uint64_t memoryTotalMB = 0;
    std::uint64_t gpuMemoryUsedMB = 0;
    std::uint64_t gpuMemoryTotalMB = 0;
    std::string gpuName = "Unknown GPU";
    std::uint64_t uptimeSeconds = 0;
};

class SystemStatsProvider {
public:
    SystemStatsProvider();
    SystemStats Get();

private:
    unsigned long long previousIdle_ = 0;
    unsigned long long previousKernel_ = 0;
    unsigned long long previousUser_ = 0;
    bool cpuReady_ = false;
    SystemStats cached_;
};