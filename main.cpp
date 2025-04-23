#include <iostream>
#include <csignal>
#include <cstdlib>
#include <cstdint>

#if defined(_WIN32)
    #include <windows.h>
    #include <psapi.h>
#elif defined(__APPLE__)
    #include <mach/mach.h>
#elif defined(__linux__)
    #include <fstream>
    #include <sstream>
    #include <string>
#endif

std::size_t getHeapUsageBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS memInfo;
    GetProcessMemoryInfo(GetCurrentProcess(), &memInfo, sizeof(memInfo));
    return static_cast<std::size_t>(memInfo.PagefileUsage);
#elif defined(__APPLE__)
    struct task_basic_info info;
    mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &size) != KERN_SUCCESS) {
        return 0;
    }
    return static_cast<std::size_t>(info.virtual_size);
#elif defined(__linux__)
    std::ifstream maps("/proc/self/maps");
    std::string line;
    std::size_t heapSize = 0;

    while (std::getline(maps, line)) {
        if (line.find("[heap]") != std::string::npos) {
            std::istringstream iss(line);
            std::string address;
            iss >> address;
            auto dash = address.find('-');
            std::size_t start = std::stoull(address.substr(0, dash), nullptr, 16);
            std::size_t end = std::stoull(address.substr(dash + 1), nullptr, 16);
            heapSize = end - start;
            break;
        }
    }
    return heapSize;
#else
    return 0;
#endif
}


void signalHandler(int signal) {
    if (signal == SIGSEGV) {
        std::cerr << "[!] Caught SIGSEGV: Segmentation fault occurred.\n";
        std::cerr << "[!] Attempted to access restricted memory (likely kernel space).\n";
        std::cerr << "[!] Recovering safely and exiting with code 0.\n";
        std::exit(0);
    }
}

int main() {
    std::cout << "=== Memory Violation Exploration ===\n";
    
    std::cout << "[*] Current heap usage: " << getHeapUsageBytes() << " bytes\n";

    std::signal(SIGSEGV, signalHandler);

    std::cout << "[*] Attempting access to kernel-space memory (0xFFFFFFFF)...\n";

    volatile int* ptr = reinterpret_cast<int*>(0xFFFFFFFF);
    std::cout << "[*] Value: " << *ptr << "\n";

    std::cout << "[*] This line will not execute unless memory access is valid.\n";
    std::cout << "[*] Program terminated safely.\n";
    return 0;
}
