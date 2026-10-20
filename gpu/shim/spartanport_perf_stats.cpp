// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_perf_stats.h"

#include <chrono>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>
#include "video_core/renderer_vulkan/vk_instance.h"

#ifdef _WIN32
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#else
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#endif

namespace SpartanPerfStats {

namespace {

std::mutex mutex;
Sample latest;
std::once_flag started;
constexpr double GiB = 1024.0 * 1024.0 * 1024.0;

// VK_EXT_memory_budget, any platform: this process's VRAM and the driver's budget for it.
void FillVram(Sample& s, const Vulkan::Instance* instance) {
    if (instance->CanReportMemoryUsage()) {
        s.vram_used_gb = double(instance->GetDeviceMemoryUsage()) / GiB;
        s.vram_total_gb = double(instance->GetDeviceMemoryBudgetNow()) / GiB;
    }
}

#ifdef _WIN32
uint64_t Ticks(const FILETIME& time) {
    return (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

// This process's use of the GPU's 3D engines, as Task Manager shows it ("GPU Engine" counters,
// any vendor). Negative until the second sample or without the counters.
class GpuUsage {
public:
    GpuUsage() {
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) {
            query = nullptr;
            return;
        }
        if (PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter) !=
            ERROR_SUCCESS) {
            PdhCloseQuery(query);
            query = nullptr;
            return;
        }
        PdhCollectQueryData(query);
        pid = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";
    }

    float Percent() {
        if (!query || PdhCollectQueryData(query) != ERROR_SUCCESS) {
            return -1.0f;
        }
        DWORD size = 0, count = 0;
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count,
                                         nullptr) != PDH_MORE_DATA) {
            return -1.0f;
        }
        buffer.resize(size);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count,
                                         items) != ERROR_SUCCESS) {
            return -1.0f;
        }
        double total = 0.0;
        for (DWORD i = 0; i < count; ++i) {
            const wchar_t* name = items[i].szName;
            if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA && wcsstr(name, pid.c_str()) &&
                wcsstr(name, L"engtype_3D")) {
                total += items[i].FmtValue.doubleValue;
            }
        }
        return std::clamp(float(total), 0.0f, 100.0f);
    }

private:
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    std::wstring pid;
    std::vector<unsigned char> buffer;
};

void Run(const Vulkan::Instance* instance) {
    GpuUsage gpu;
    FILETIME created, ended, kernel, user, now;
    uint64_t last_process = 0, last_wall = 0;
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    while (true) {
        Sample s;
        s.gpu_name = std::string(instance->GetModelName());
        GetSystemTimeAsFileTime(&now);
        if (GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user)) {
            const uint64_t process = Ticks(kernel) + Ticks(user), wall = Ticks(now);
            if (last_wall) {
                s.cpu_percent = CpuPercent(process - last_process, wall - last_wall, cores);
            }
            last_process = process;
            last_wall = wall;
        }
        PROCESS_MEMORY_COUNTERS memory{};
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory))) {
            s.ram_used_gb = double(memory.WorkingSetSize) / GiB;
        }
        MEMORYSTATUSEX system{.dwLength = sizeof(MEMORYSTATUSEX)};
        if (GlobalMemoryStatusEx(&system)) {
            s.ram_total_gb = double(system.ullTotalPhys) / GiB;
        }
        FillVram(s, instance);
        s.gpu_percent = gpu.Percent();
        {
            std::scoped_lock lock{mutex};
            latest = std::move(s);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
#else
// The Vulkan GPU's PCI address (VK_EXT_pci_bus_info, a physical-device query: no need to enable it);
// empty without the extension.
std::string VulkanPciBusId(const Vulkan::Instance* instance) {
    const vk::PhysicalDevice physical = instance->GetPhysicalDevice();
    const auto [result, extensions] = physical.enumerateDeviceExtensionProperties();
    if (result != vk::Result::eSuccess ||
        std::none_of(extensions.begin(), extensions.end(), [](const vk::ExtensionProperties& e) {
            return std::string_view(e.extensionName) == VK_EXT_PCI_BUS_INFO_EXTENSION_NAME;
        })) {
        return {};
    }
    const auto chain =
        physical.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDevicePCIBusInfoPropertiesEXT>();
    const auto& pci = chain.get<vk::PhysicalDevicePCIBusInfoPropertiesEXT>();
    return PciBusId(pci.pciDomain, pci.pciBus, pci.pciDevice, pci.pciFunction);
}

// The use of the GPU Vulkan runs on, all processes (the drivers give no cheap per-process figure):
// amdgpu's sysfs gpu_busy_percent, or NVML on NVIDIA, opened at run time so the library is not a
// dependency (it comes with the NVIDIA driver). Intel and the rest: negative.
class GpuUsage {
public:
    explicit GpuUsage(const std::string& pci) {
        if (pci.empty()) {
            return;
        }
        amd_path = "/sys/bus/pci/devices/" + pci + "/gpu_busy_percent";
        if (access(amd_path.c_str(), R_OK) == 0) {
            return;
        }
        amd_path.clear();
        void* nvml = dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!nvml) {
            return;
        }
        const auto init = reinterpret_cast<int (*)()>(dlsym(nvml, "nvmlInit_v2"));
        const auto by_pci =
            reinterpret_cast<int (*)(const char*, void**)>(dlsym(nvml, "nvmlDeviceGetHandleByPciBusId_v2"));
        utilization = reinterpret_cast<int (*)(void*, Utilization*)>(dlsym(nvml, "nvmlDeviceGetUtilizationRates"));
        // Kept open: the sampling thread runs until the game exits.
        if (!init || !by_pci || !utilization || init() != 0 || by_pci(pci.c_str(), &device) != 0) {
            device = nullptr;
        }
    }

    float Percent() {
        if (!amd_path.empty()) {
            int percent = -1;
            if (FILE* file = std::fopen(amd_path.c_str(), "r")) {
                if (std::fscanf(file, "%d", &percent) != 1) {
                    percent = -1;
                }
                std::fclose(file);
            }
            return percent < 0 ? -1.0f : std::clamp(float(percent), 0.0f, 100.0f);
        }
        Utilization rates{};
        if (device && utilization(device, &rates) == 0) {
            return std::clamp(float(rates.gpu), 0.0f, 100.0f);
        }
        return -1.0f;
    }

private:
    struct Utilization { // nvmlUtilization_t
        unsigned gpu, memory;
    };
    std::string amd_path;
    void* device = nullptr; // nvmlDevice_t
    int (*utilization)(void*, Utilization*) = nullptr;
};

uint64_t Nanoseconds(clockid_t clock) {
    timespec time{};
    clock_gettime(clock, &time);
    return uint64_t(time.tv_sec) * 1000000000ull + uint64_t(time.tv_nsec);
}

void Run(const Vulkan::Instance* instance) {
    GpuUsage gpu(VulkanPciBusId(instance));
    uint64_t last_process = 0, last_wall = 0;
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    const long page = sysconf(_SC_PAGESIZE), pages = sysconf(_SC_PHYS_PAGES);
    while (true) {
        Sample s;
        s.gpu_name = std::string(instance->GetModelName());
        const uint64_t process = Nanoseconds(CLOCK_PROCESS_CPUTIME_ID), wall = Nanoseconds(CLOCK_MONOTONIC);
        if (last_wall) {
            s.cpu_percent = CpuPercent(process - last_process, wall - last_wall, cores);
        }
        last_process = process;
        last_wall = wall;
        if (FILE* file = std::fopen("/proc/self/statm", "r")) {
            char text[128]{};
            if (std::fgets(text, sizeof(text), file)) {
                const double bytes = StatmResidentBytes(text, page);
                s.ram_used_gb = bytes < 0.0 ? -1.0 : bytes / GiB;
            }
            std::fclose(file);
        }
        if (page > 0 && pages > 0) {
            s.ram_total_gb = double(page) * double(pages) / GiB;
        }
        FillVram(s, instance);
        s.gpu_percent = gpu.Percent();
        {
            std::scoped_lock lock{mutex};
            latest = std::move(s);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
#endif

} // namespace

void Start(const Vulkan::Instance& instance) {
    // ponytail: never joined; a thread that ended while the game ran crashed in ntdll
    // (vk_pipeline_serialization.cpp, StopWarmUp). It sleeps between samples.
    std::call_once(started, [&] { std::thread(Run, &instance).detach(); });
}

Sample Latest() {
    std::scoped_lock lock{mutex};
    return latest;
}

} // namespace SpartanPerfStats
