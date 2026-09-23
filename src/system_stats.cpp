#include "system_stats.hpp"
#include <windows.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;

static std::string WideToUtf8(const wchar_t* value) {
    if (!value) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string out(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), size, nullptr, nullptr);
    return out;
}

SystemStatsProvider::SystemStatsProvider() {
    ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        ComPtr<IDXGIAdapter1> adapter;
        if (SUCCEEDED(factory->EnumAdapters1(0, &adapter))) {
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                cached_.gpuName = WideToUtf8(desc.Description);
                cached_.gpuMemoryTotalMB = static_cast<std::uint64_t>(desc.DedicatedVideoMemory / (1024ull * 1024ull));
            }
        }
    }
}

SystemStats SystemStatsProvider::Get() {
    FILETIME idle{}, kernel{}, user{};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        auto to64 = [](const FILETIME& t) -> unsigned long long {
            return (static_cast<unsigned long long>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
        };

        const auto i = to64(idle);
        const auto k = to64(kernel);
        const auto u = to64(user);

        if (cpuReady_) {
            const auto idleDelta = i - previousIdle_;
            const auto kernelDelta = k - previousKernel_;
            const auto userDelta = u - previousUser_;
            const auto total = kernelDelta + userDelta;

            if (total > 0) {
                cached_.cpuUsage = std::clamp(100.0 * (1.0 - static_cast<double>(idleDelta) / static_cast<double>(total)), 0.0, 100.0);
            }
        }

        previousIdle_ = i;
        previousKernel_ = k;
        previousUser_ = u;
        cpuReady_ = true;
    }

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        cached_.memoryTotalMB = memory.ullTotalPhys / (1024ull * 1024ull);
        const auto freeMB = memory.ullAvailPhys / (1024ull * 1024ull);
        cached_.memoryUsedMB = cached_.memoryTotalMB > freeMB ? cached_.memoryTotalMB - freeMB : 0;
        cached_.memoryUsage = memory.dwMemoryLoad;
    }

    cached_.uptimeSeconds = GetTickCount64() / 1000ull;

    ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        ComPtr<IDXGIAdapter1> adapter;
        if (SUCCEEDED(factory->EnumAdapters1(0, &adapter))) {
            ComPtr<IDXGIAdapter3> adapter3;
            if (SUCCEEDED(adapter.As(&adapter3))) {
                DXGI_QUERY_VIDEO_MEMORY_INFO info{};
                if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) {
                    cached_.gpuMemoryUsedMB = info.CurrentUsage / (1024ull * 1024ull);
                    cached_.gpuMemoryTotalMB = info.Budget / (1024ull * 1024ull);
                }
            }
        }
    }

    return cached_;
}