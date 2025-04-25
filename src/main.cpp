#include "includes.hpp"

void test_memory_range(uintptr_t start, uintptr_t end, const std::string& name) {
    std::cout << "[*] Testing " << name << " range: 0x" << std::hex << start
              << " - 0x" << end << std::dec << "\n";
    std::cout << "[*] " << name << " start - 1: 0x"
              << std::hex << (start - 1) << std::dec << "\n";
    try_access_memory(start - 1); std::cout << "\n";

    std::cout << "[*] " << name << " start: 0x"
              << std::hex << start << std::dec << "\n";
    try_access_memory(start);     std::cout << "\n";

    std::cout << "[*] " << name << " middle: 0x"
              << std::hex << (start + (end - start) / 2) << std::dec << "\n";
    try_access_memory(start + (end - start) / 2); std::cout << "\n";

    std::cout << "[*] " << name << " end - 1: 0x"
              << std::hex << (end - 1) << std::dec << "\n";
    try_access_memory(end - 1);   std::cout << "\n";

    std::cout << "[*] " << name << " end: 0x"
              << std::hex << end << std::dec << "\n";
    try_access_memory(end);       std::cout << "\n";

    std::cout << "[*] " << name << " end + 1: 0x"
              << std::hex << (end + 1) << std::dec << "\n";
    try_access_memory(end + 1);   std::cout << "\n";
}

int main() {
    std::cout << "=== Memory Violation Exploration ===\n\n";

#if defined(_WIN32)
    auto [stack_start, stack_end] = get_stack_bounds();
    auto [heap_start, heap_end]   = get_heap_bounds();
#else
    struct sigaction sa;
    sa.sa_handler = segv_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGSEGV, &sa, nullptr) != 0) {
        std::cerr << "[!] Failed to set up SIGSEGV handler\n";
        return 1;
    }
    auto [stack_start, stack_end] = get_stack_bounds();
    auto [heap_start, heap_end]   = get_heap_bounds();
#endif

    if (!stack_start || !stack_end || !heap_start || !heap_end) {
        std::cerr << "[!] Failed to get stack or heap bounds.\n";
        return 1;
    }

    std::cout << "Stack  : " << stack_start << " - " << stack_end << "\n";
    std::cout << "Heap   : " << heap_start  << " - " << heap_end  << "\n\n";

    std::cout << "[*] Attempting to access memory in various regions...\n\n";
    test_memory_range(
        reinterpret_cast<uintptr_t>(stack_start),
        reinterpret_cast<uintptr_t>(stack_end),
        "Stack"
    );
    test_memory_range(
        reinterpret_cast<uintptr_t>(heap_start),
        reinterpret_cast<uintptr_t>(heap_end),
        "Heap"
    );

    return 0;
}
