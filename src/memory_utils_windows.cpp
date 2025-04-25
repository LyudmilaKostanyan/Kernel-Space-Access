#include "includes.hpp"

#if defined(_WIN32) && !defined(__MSYS__)

std::pair<void*, void*> get_stack_bounds() {
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(&mbi, &mbi, sizeof(mbi));
    void* stack_top    = mbi.BaseAddress;
    void* stack_bottom = mbi.AllocationBase;
    return { stack_bottom, stack_top };
}

std::pair<void*, void*> get_heap_bounds() {
    void* heap_alloc = HeapAlloc(GetProcessHeap(), 0, 1);
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(heap_alloc, &mbi, sizeof(mbi));
    void* heap_base = mbi.AllocationBase;
    void* heap_end  = static_cast<char*>(mbi.BaseAddress) + mbi.RegionSize;
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

#else
#warning "windows.cpp compiled on non-Windows"
#endif
