#include <iostream>
#include <utility>
#include <cstdint>
#include <cstdlib>
#include <setjmp.h>
#include <signal.h>

#if defined(_WIN32) && !defined(__MSYS__)
#include <windows.h>

std::pair<void*, void*> get_stack_bounds() {
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(&mbi, &mbi, sizeof(mbi));
    void* stack_top = mbi.BaseAddress;
    void* stack_bottom = mbi.AllocationBase;
    return { stack_bottom, stack_top };
}

std::pair<void*, void*> get_heap_bounds() {
    void* heap_alloc = HeapAlloc(GetProcessHeap(), 0, 1);
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(heap_alloc, &mbi, sizeof(mbi));
    void* heap_base = mbi.AllocationBase;
    void* heap_end = static_cast<char*>(mbi.BaseAddress) + mbi.RegionSize;
    HeapFree(GetProcessHeap(), 0, heap_alloc);
    return { heap_base, heap_end };
}

void try_access_memory(uintptr_t addr) {
    std::cout << "    [*] Attempting to access memory at address: 0x"
              << std::hex << addr << std::dec << "\n";
    __try {
        volatile int value = *reinterpret_cast<volatile int*>(addr);
        std::cout << "    [*] Value: " << value << "\n";
    }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
              ? EXCEPTION_EXECUTE_HANDLER
              : EXCEPTION_CONTINUE_SEARCH) {
        std::cout << "    [!] Caught access violation at address 0x"
                  << std::hex << addr << std::dec << "\n";
    }
}

#elif defined(__APPLE__)
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_region.h>
#include <sys/mman.h>
#include <pthread.h>

static sigjmp_buf env;

void segv_handler(int sig) {
    std::cerr << "    [!] Caught SIGSEGV at signal: " << sig << "\n";
    siglongjmp(env, 1);
}

bool is_address_readable(uintptr_t addr) {
    mach_vm_address_t address = addr;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    memory_object_name_t object;
    kern_return_t kr = mach_vm_region(mach_task_self(),
                                      &address,
                                      &size,
                                      VM_REGION_BASIC_INFO_64,
                                      reinterpret_cast<vm_region_info_t>(&info),
                                      &count,
                                      &object);
    if (kr != KERN_SUCCESS) {
        std::cout << "    [!] mach_vm_region failed for 0x" << std::hex << addr
                  << ": " << mach_error_string(kr) << std::dec << "\n";
        return false;
    }
    bool readable = (info.protection & VM_PROT_READ) != 0;
    std::cout << "    [*] Address 0x" << std::hex << addr << " is "
              << (readable ? "readable" : "not readable") << std::dec << "\n";
    return readable;
}

std::pair<void*, void*> get_stack_bounds() {
    pthread_t self = pthread_self();
    void* stack_top = pthread_get_stackaddr_np(self);
    size_t stack_size = pthread_get_stacksize_np(self);
    void* stack_bottom = static_cast<char*>(stack_top) - stack_size;
    return { stack_bottom, stack_top };
}

std::pair<void*, void*> get_heap_bounds() {
    void* sample_alloc = malloc(1);
    if (!sample_alloc) {
        std::cerr << "Failed to allocate memory for heap detection\n";
        return {nullptr, nullptr};
    }
    mach_vm_address_t address = reinterpret_cast<mach_vm_address_t>(sample_alloc);
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    memory_object_name_t object;
    kern_return_t kr = mach_vm_region(mach_task_self(),
                                      &address,
                                      &size,
                                      VM_REGION_BASIC_INFO_64,
                                      reinterpret_cast<vm_region_info_t>(&info),
                                      &count,
                                      &object);
    free(sample_alloc);
    if (kr != KERN_SUCCESS) {
        std::cerr << "mach_vm_region failed for heap: " << mach_error_string(kr) << "\n";
        return {nullptr, nullptr};
    }
    return { reinterpret_cast<void*>(address),
             reinterpret_cast<void*>(address + size) };
}

void try_access_memory(uintptr_t addr) {
    std::cout << "    [*] Setting up sigsetjmp for address: 0x"
              << std::hex << addr << std::dec << "\n";
    if (!is_address_readable(addr)) {
        std::cout << "    [!] Skipping access to non-readable address 0x"
                  << std::hex << addr << std::dec << "\n";
        return;
    }
    if (sigsetjmp(env, 1) == 0) {
        std::cout << "    [*] Attempting to access memory at address: 0x"
                  << std::hex << addr << std::dec << "\n";
        volatile int* ptr = reinterpret_cast<int*>(addr);
        volatile int value = *ptr;
        std::cout << "    [*] Value: " << value << "\n";
    } else {
        std::cout << "    [+] Recovered from segmentation fault.\n";
    }
}

#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#include <string>
#include <sstream>

static sigjmp_buf env;

void segv_handler(int sig) {
    std::cerr << "    [!] Caught SIGSEGV at signal: " << sig << "\n";
    siglongjmp(env, 1);
}

std::pair<void*, void*> get_stack_bounds() {
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("[stack]") != std::string::npos) {
            std::istringstream iss(line);
            std::string addr_range;
            iss >> addr_range;
            auto dash = addr_range.find('-');
            void* start = reinterpret_cast<void*>(
                std::stoull(addr_range.substr(0, dash), nullptr, 16));
            void* end = reinterpret_cast<void*>(
                std::stoull(addr_range.substr(dash + 1), nullptr, 16));
            return {start, end};
        }
    }
    std::cerr << "Failed to find stack in /proc/self/maps\n";
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
            void* start = reinterpret_cast<void*>(
                std::stoull(addr_range.substr(0, dash), nullptr, 16));
            void* end = reinterpret_cast<void*>(
                std::stoull(addr_range.substr(dash + 1), nullptr, 16));
            return {start, end};
        }
    }
    std::cerr << "Failed to find heap in /proc/self/maps\n";
    return {nullptr, nullptr};
}

void try_access_memory(uintptr_t addr) {
    std::cout << "    [*] Setting up sigsetjmp for address: 0x"
              << std::hex << addr << std::dec << "\n";
    if (sigsetjmp(env, 1) == 0) {
        std::cout << "    [*] Attempting to access memory at address: 0x"
                  << std::hex << addr << std::dec << "\n";
        volatile int* ptr = reinterpret_cast<int*>(addr);
        volatile int value = *ptr;
        std::cout << "    [*] Value: " << value << "\n";
    } else {
        std::cout << "    [+] Recovered from segmentation fault.\n";
    }
}

#elif defined(__MSYS__)
#include <windows.h>
#include <unistd.h>

static sigjmp_buf env;

void segv_handler(int sig) {
    std::cerr << "    [!] Caught SIGSEGV at signal: " << sig << "\n";
    siglongjmp(env, 1);
}

bool is_address_readable(uintptr_t addr) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0) {
        std::cout << "    [!] VirtualQuery failed for 0x" << std::hex << addr << std::dec << "\n";
        return false;
    }
    bool readable = (mbi.State & MEM_COMMIT) != 0 &&
                    (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE)) != 0;
    if (readable) {
        uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (addr >= region_end - sizeof(int)) {
            std::cout << "    [!] Address 0x" << std::hex << addr
                      << " is too close to region end 0x" << region_end << std::dec << "\n";
            return false;
        }
    }
    std::cout << "    [*] Address 0x" << std::hex << addr << " is "
              << (readable ? "readable" : "not readable") << std::dec << "\n";
    return readable;
}

std::pair<void*, void*> get_stack_bounds() {
    MEMORY_BASIC_INFORMATION mbi;
    void* local_var = &mbi;
    if (VirtualQuery(local_var, &mbi, sizeof(mbi)) == 0) {
        std::cerr << "VirtualQuery failed for stack\n";
        return {nullptr, nullptr};
    }
    void* stack_top = mbi.BaseAddress;
    void* stack_bottom = mbi.AllocationBase;
    return {stack_bottom, stack_top};
}

std::pair<void*, void*> get_heap_bounds() {
    void* heap_alloc = HeapAlloc(GetProcessHeap(), 0, 1);
    if (!heap_alloc) {
        std::cerr << "HeapAlloc failed\n";
        return {nullptr, nullptr};
    }
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(heap_alloc, &mbi, sizeof(mbi)) == 0) {
        std::cerr << "VirtualQuery failed for heap\n";
        HeapFree(GetProcessHeap(), 0, heap_alloc);
        return {nullptr, nullptr};
    }
    void* heap_base = mbi.AllocationBase;
    void* heap_end = static_cast<char*>(mbi.BaseAddress) + mbi.RegionSize;
    HeapFree(GetProcessHeap(), 0, heap_alloc);
    return {heap_base, heap_end};
}

void try_access_memory(uintptr_t addr) {
    std::cout << "    [*] Setting up sigsetjmp for address: 0x"
              << std::hex << addr << std::dec << "\n";
    if (!is_address_readable(addr)) {
        std::cout << "    [!] Skipping access to non-readable address 0x"
                  << std::hex << addr << std::dec << "\n";
        return;
    }
    if (sigsetjmp(env, 1) == 0) {
        std::cout << "    [*] Attempting to access memory at address: 0x"
                  << std::hex << addr << std::dec << "\n";
        volatile int* ptr = reinterpret_cast<int*>(addr);
        volatile int value = *ptr;
        std::cout << "    [*] Value: " << value << "\n";
    } else {
        std::cout << "    [+] Recovered from segmentation fault.\n";
    }
}

#else
#error "Unsupported platform"
#endif

void test_memory_range(uintptr_t start, uintptr_t end, const std::string& name) {
    std::cout << "[*] Testing " << name << " range: 0x" << std::hex << start
              << " - 0x" << end << std::dec << "\n";
    std::cout << "[*] " << name << " start - 1: 0x" << std::hex << (start - 1) << std::dec << "\n";
    try_access_memory(start - 1); std::cout << "\n";
    std::cout << "[*] " << name << " start: 0x" << std::hex << start << std::dec << "\n";
    try_access_memory(start); std::cout << "\n";
    std::cout << "[*] " << name << " middle: 0x"
              << std::hex << (start + (end - start) / 2) << std::dec << "\n";
    try_access_memory(start + (end - start) / 2); std::cout << "\n";
    std::cout << "[*] " << name << " end - 1: 0x" << std::hex << (end - 1) << std::dec << "\n";
    try_access_memory(end - 1); std::cout << "\n";
    std::cout << "[*] " << name << " end: 0x" << std::hex << end << std::dec << "\n";
    try_access_memory(end); std::cout << "\n";
    std::cout << "[*] " << name << " end + 1: 0x" << std::hex << (end + 1) << std::dec << "\n";
    try_access_memory(end + 1); std::cout << "\n";
}

int main() {
    std::cout << "=== Memory Violation Exploration ===\n\n";

#if defined(_WIN32)
    auto [stack_start, stack_end] = get_stack_bounds();
    auto [heap_start, heap_end] = get_heap_bounds();
#else
    struct sigaction sa;
    sa.sa_handler = segv_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGSEGV, &sa, nullptr) != 0) {
        std::cerr << "[!] Failed to set up SIGSEGV handler\n";
        return 1;
    }
    auto [stack_start, stack_end] = get_stack_bounds();
    auto [heap_start, heap_end] = get_heap_bounds();
#endif

    if (!stack_start || !stack_end || !heap_start || !heap_end) {
        std::cerr << "[!] Failed to get stack or heap bounds.\n";
        return 1;
    }

    std::cout << "Stack  : " << stack_start << " - " << stack_end << "\n";
    std::cout << "Heap   : " << heap_start << " - " << heap_end << "\n\n";

    std::cout << "[*] Attempting to access memory in various regions...\n\n";
    test_memory_range(reinterpret_cast<uintptr_t>(stack_start),
                      reinterpret_cast<uintptr_t>(stack_end), "Stack");
    test_memory_range(reinterpret_cast<uintptr_t>(heap_start),
                      reinterpret_cast<uintptr_t>(heap_end), "Heap");

    return 0;
}