#include <iostream>
#include <csignal>
#include <cstdlib>
#include <setjmp.h>
#include <utility>

sigjmp_buf env;


#if defined(_WIN32)
#include <windows.h>

std::pair<void*, void*> get_stack_bounds() {
    MEMORY_BASIC_INFORMATION mbi;
    int local = 0;
    VirtualQuery(&local, &mbi, sizeof(mbi));
    void* stack_top = mbi.BaseAddress;
    void* stack_bottom = mbi.AllocationBase;
    return {stack_bottom, stack_top};
}

std::pair<void*, void*> get_heap_bounds() {
    void* heap = HeapAlloc(GetProcessHeap(), 0, 1);
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(heap, &mbi, sizeof(mbi));
    void* heap_base = mbi.AllocationBase;
    void* heap_end = static_cast<char*>(mbi.BaseAddress) + mbi.RegionSize;
    HeapFree(GetProcessHeap(), 0, heap);
    return {heap_base, heap_end};
}

#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/vm_map.h>
#include <sys/mman.h>
#include <unistd.h>

std::pair<void*, void*> get_stack_bounds() {
    void* addr = &addr;
    vm_address_t base = reinterpret_cast<vm_address_t>(addr);
    vm_size_t size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    memory_object_name_t object;
    kern_return_t kr = mach_vm_region(mach_task_self(),
                                      reinterpret_cast<mach_vm_address_t*>(&base),
                                      &size,
                                      VM_REGION_BASIC_INFO_64,
                                      reinterpret_cast<vm_region_info_t>(&info),
                                      &count,
                                      &object);
    void* start = reinterpret_cast<void*>(base);
    void* end = reinterpret_cast<void*>(base + size);
    return {start, end};
}

std::pair<void*, void*> get_heap_bounds() {
    void* heap_end = sbrk(0);
    void* heap_start = nullptr;

    long pagesize = sysconf(_SC_PAGESIZE);
    heap_start = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(heap_end) / pagesize - 1000) * pagesize);
    return {heap_start, heap_end};
}

#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#include <string>
#include <sstream>

std::pair<void*, void*> get_stack_bounds() {
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("[stack]") != std::string::npos) {
            std::istringstream iss(line);
            std::string addr_range;
            iss >> addr_range;
            auto dash = addr_range.find('-');
            void* start = reinterpret_cast<void*>(std::stoull(addr_range.substr(0, dash), nullptr, 16));
            void* end = reinterpret_cast<void*>(std::stoull(addr_range.substr(dash + 1), nullptr, 16));
            return {start, end};
        }
    }
    return {nullptr, nullptr};
}

std::pair<void*, void*> get_heap_bounds() {
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("[heap]") != std::string::npos) {
            std::istringstream iss(line);
            std::string addr_range;
            iss >> addr_range;
            auto dash = addr_range.find('-');
            void* start = reinterpret_cast<void*>(std::stoull(addr_range.substr(0, dash), nullptr, 16));
            void* end = reinterpret_cast<void*>(std::stoull(addr_range.substr(dash + 1), nullptr, 16));
            return {start, end};
        }
    }
    return {nullptr, nullptr};
}

#else
#error "Unsupported platform"
#endif

void signalHandler(int signal) {
    switch (signal) {
        case SIGSEGV:
            std::cerr << "    [!] Caught SIGSEGV: Segmentation fault.\n";
            break;
        case SIGBUS:
            std::cerr << "    [!] Caught SIGBUS: Bus error (bad alignment or page).\n";
            break;
        case SIGABRT:
            std::cerr << "    [!] Caught SIGABRT: Aborted.\n";
            break;
        case SIGILL:
            std::cerr << "    [!] Caught SIGILL: Illegal instruction.\n";
            break;
        default:
            std::cerr << "    [!] Caught unknown signal: " << signal << "\n";
            break;
    }
    std::cerr << "    [!] Attempted to access restricted memory (likely kernel space).\n";
    std::cerr << "    [!] Jumping back to recovery point.\n";
    siglongjmp(env, 1);
}

void setupSignalHandlers() {
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGBUS, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGILL, signalHandler);
}

void try_access_memory(auto addr) {
    if (sigsetjmp(env, 1) == 0) {
        std::cout << "    [*] Attempting to access memory at address: " << std::hex << addr << "\n";

        volatile int* ptr = reinterpret_cast<int*>(addr);
        std::cout << "    [*] Value: " << *ptr << "\n";
    } else
        std::cout << "    [+] Recovered from segmentation fault and continued execution.\n";
}

void test_memory_range(uintptr_t start, uintptr_t end, std::string name) 
{
    std::cout << "[*] " << name << std::endl;
    std::cout << "[*] " << name << " start - 1: " << start - 1 << "\n";
    try_access_memory(start - 1);
    std::cout << std::endl;
    std::cout << "[*] " << name << " start: " << start << "\n";
    try_access_memory(start);
    std::cout << std::endl;
    std::cout << "[*] " << name << " middle: " << start + ((end - start) / 2) << "\n";
    try_access_memory(start + ((end - start) / 2));
    std::cout << std::endl;
    std::cout << "[*] " << name << " end - 1: " << end - 1 << "\n";
    try_access_memory(end - 1);
    std::cout << std::endl;
    std::cout << "[*] " << name << " end: " << end << "\n";
    try_access_memory(end);
    std::cout << std::endl;
    std::cout << "[*] " << name << " end + 1: " << end + 1 << "\n";
    try_access_memory(end + 1);
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Memory Violation Exploration with sigsetjmp ===\n\n";

    setupSignalHandlers();
    auto [stack_start, stack_end] = get_stack_bounds();
    auto [heap_start, heap_end] = get_heap_bounds();
    if (stack_start == nullptr || stack_end == nullptr || heap_start == nullptr || heap_end == nullptr) {
        std::cerr << "[!] Failed to get stack or heap bounds.\n";
        return 1;
    }
    std::cout << "Stack  : " << stack_start << " - " << stack_end << "\n";
    std::cout << "Heap   : " << heap_start << " - " << heap_end << "\n";

    std::cout << "\n[*] Attempting to access memory in various regions...\n\n";
    test_memory_range(reinterpret_cast<uintptr_t>(stack_start), reinterpret_cast<uintptr_t>(stack_end), "Stack");
    test_memory_range(reinterpret_cast<uintptr_t>(heap_start), reinterpret_cast<uintptr_t>(heap_end), "Heap");

    return 0;
}
