#include <iostream>
#include <csignal>
#include <cstdlib>

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

    std::signal(SIGSEGV, signalHandler);

    std::cout << "[*] Attempting access to kernel-space memory (0xFFFFFFFF)...\n";

    volatile int* ptr = reinterpret_cast<int*>(0xFFFFFFFF);
    std::cout << "[*] Value: " << *ptr << "\n";

    std::cout << "[*] This line will not execute unless memory access is valid.\n";
    return 0;
}
