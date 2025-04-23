#include <iostream>
#include <csignal>
#include <cstdlib>

void signalHandler(int signal) {
    if (signal == SIGSEGV) {
        std::cerr << "Caught signal: SIGSEGV (Segmentation Fault)\n";
        std::cerr << "You tried to access an invalid memory region (likely kernel space or unmapped memory).\n";
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    std::cout << "=== Memory Violation Test ===\n";

    std::signal(SIGSEGV, signalHandler);

    std::cout << "Attempting to access invalid memory (0xFFFFFFFF)...\n";
    
    volatile int* kernel_ptr = reinterpret_cast<int*>(0xFFFFFFFF);
    std::cout << "Value: " << *kernel_ptr << std::endl;

    std::cout << "This message won't be printed.\n";
    return 0;
}
