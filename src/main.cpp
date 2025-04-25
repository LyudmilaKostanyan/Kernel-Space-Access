#include "includes.hpp"


void test_range(uintptr_t start, uintptr_t end, const std::string& name) {
    auto try_addr = [&](uintptr_t a) {
        std::cout << "[" << name << "] probing 0x"
                  << std::hex << a << std::dec << "\n";
        try_access_memory(a);
    };

    try_addr(start - 1);
    try_addr(start);
    try_addr(start + (end - start) / 2);
    try_addr(end - 1);
    try_addr(end);
    try_addr(end + 1);
    std::cout << "\n";
}

int main() {
    auto [stk_begin, stk_end] = get_stack_bounds();
    auto [hp_begin,  hp_end]  = get_heap_bounds();

    std::cout << "Stack: [0x" << std::hex << stk_begin
              << " - 0x" << stk_end << "]\n";
    std::cout << "Heap : [0x" << std::hex << hp_begin
              << " - 0x" << hp_end  << "]\n\n";

    test_range(reinterpret_cast<uintptr_t>(stk_begin),
               reinterpret_cast<uintptr_t>(stk_end), "STACK");
    test_range(reinterpret_cast<uintptr_t>(hp_begin),
               reinterpret_cast<uintptr_t>(hp_end),  "HEAP");
    return 0;
}
