#include "includes.hpp"

#if defined(__APPLE__)

static sigjmp_buf env;

void segv_handler(int sig) {
    std::cerr << "    [!] Caught SIGSEGV at signal: " << sig << "\n";
    siglongjmp(env, 1);
}

bool is_address_readable(uintptr_t addr) {
    mach_vm_address_t      address = addr;
    mach_vm_size_t         size    = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count   = VM_REGION_BASIC_INFO_COUNT_64;
    memory_object_name_t   object;
    kern_return_t          kr = mach_vm_region(
        mach_task_self(),
        &address,
        &size,
        VM_REGION_BASIC_INFO_64,
        reinterpret_cast<vm_region_info_t>(&info),
        &count,
        &object
    );
    if (kr != KERN_SUCCESS) {
        std::cout << "    [!] mach_vm_region failed for 0x"
                  << std::hex << addr << ": " << mach_error_string(kr)
                  << std::dec << "\n";
        return false;
    }
    bool readable = (info.protection & VM_PROT_READ) != 0;
    std::cout << "    [*] Address 0x" << std::hex << addr << " is "
              << (readable ? "readable" : "not readable") << std::dec << "\n";
    return readable;
}

std::pair<void*, void*> get_stack_bounds() {
    pthread_t self       = pthread_self();
    void*     stack_top  = pthread_get_stackaddr_np(self);
    size_t    stack_size = pthread_get_stacksize_np(self);
    void*     stack_bottom = static_cast<char*>(stack_top) - stack_size;
    return { stack_bottom, stack_top };
}

std::pair<void*, void*> get_heap_bounds() {
    void* sample_alloc = malloc(1);
    if (!sample_alloc) {
        std::cerr << "Failed to allocate memory for heap detection\n";
        return {nullptr, nullptr};
    }
    mach_vm_address_t            address = reinterpret_cast<mach_vm_address_t>(sample_alloc);
    mach_vm_size_t               size    = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t       count   = VM_REGION_BASIC_INFO_COUNT_64;
    memory_object_name_t         object;
    kern_return_t kr = mach_vm_region(
        mach_task_self(),
        &address,
        &size,
        VM_REGION_BASIC_INFO_64,
        reinterpret_cast<vm_region_info_t>(&info),
        &count,
        &object
    );
    free(sample_alloc);
    if (kr != KERN_SUCCESS) {
        std::cerr << "mach_vm_region failed for heap: "
                  << mach_error_string(kr) << "\n";
        return {nullptr, nullptr};
    }
    return {
        reinterpret_cast<void*>(address),
        reinterpret_cast<void*>(address + size)
    };
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
        volatile int  value = *ptr;
        std::cout << "    [*] Value: " << value << "\n";
    } else {
        std::cout << "    [+] Recovered from segmentation fault.\n";
    }
}

#elif defined(__linux__)

static sigjmp_buf env;

void segv_handler(int sig) {
    std::cerr << "    [!] Caught SIGSEGV at signal: " << sig << "\n";
    siglongjmp(env, 1);
}

std::pair<void*, void*> get_stack_bounds() {
    std::ifstream maps("/proc/self/maps");
    std::string   line;
    while (std::getline(maps, line)) {
        if (line.find("[stack]") != std::string::npos) {
            std::istringstream iss(line);
            std::string        addr_range;
            iss >> addr_range;
            auto dash = addr_range.find('-');
            void* start = reinterpret_cast<void*>(
                std::stoull(addr_range.substr(0, dash), nullptr, 16)
            );
            void* end = reinterpret_cast<void*>(
                std::stoull(addr_range.substr(dash + 1), nullptr, 16)
            );
            return { start, end };
        }
    }
    std::cerr << "Failed to find stack in /proc/self/maps\n";
    return {nullptr, nullptr};
}

std::pair<void*, void*> get_heap_bounds() {
    std::ifstream maps("/proc/self/maps");
    std::string   line;
    while (std::getline(maps, line)) {
        if (line.find("[heap]") != std::string::npos) {
            std::istringstream iss(line);
            std::string        addr_range;
            iss >> addr_range;
            auto dash = addr_range.find('-');
            void* start = reinterpret_cast<void*>(
                std::stoull(addr_range.substr(0, dash), nullptr, 16)
            );
            void* end = reinterpret_cast<void*>(
                std::stoull(addr_range.substr(dash + 1), nullptr, 16)
            );
            return { start, end };
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
        volatile int  value = *ptr;
        std::cout << "    [*] Value: " << value << "\n";
    } else {
        std::cout << "    [+] Recovered from segmentation fault.\n";
    }
}

#elif defined(__MSYS__)

static sigjmp_buf env;

void segv_handler(int sig) {
    std::cerr << "    [!] Caught SIGSEGV at signal: " << sig << "\n";
    siglongjmp(env, 1);
}

bool is_address_readable(uintptr_t addr) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0) {
        std::cout << "    [!] VirtualQuery failed for 0x"
                  << std::hex << addr << std::dec << "\n";
        return false;
    }
    bool readable = (mbi.State & MEM_COMMIT) != 0 &&
                    (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE)) != 0;
    if (readable) {
        uintptr_t region_end =
            reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (addr >= region_end - sizeof(int)) {
            std::cout << "    [!] Address 0x" << std::hex << addr
                      << " is too close to region end 0x" << region_end
                      << std::dec << "\n";
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
    void* stack_top    = mbi.BaseAddress;
    void* stack_bottom = mbi.AllocationBase;
    return { stack_bottom, stack_top };
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
    void* heap_end  = static_cast<char*>(mbi.BaseAddress) + mbi.RegionSize;
    HeapFree(GetProcessHeap(), 0, heap_alloc);
    return { heap_base, heap_end };
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
        volatile int  value = *ptr;
        std::cout << "    [*] Value: " << value << "\n";
    } else {
        std::cout << "    [+] Recovered from segmentation fault.\n";
    }
}

#else
#error "Unsupported POSIX platform"
#endif
