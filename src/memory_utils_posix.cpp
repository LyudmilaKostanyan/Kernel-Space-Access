#include "includes.hpp"
#include <unistd.h>

static sigjmp_buf env;
static void segv_handler(int) { siglongjmp(env, 1); }

std::pair<void*, void*> get_stack_bounds() {
    void* base   = nullptr;
    size_t size  = 0;

#if defined(__linux__)
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstack(&attr, &base, &size);
    pthread_attr_destroy(&attr);
#elif defined(__APPLE__)
    void* top = pthread_get_stackaddr_np(pthread_self());
    size      = pthread_get_stacksize_np(pthread_self());
    base      = static_cast<char*>(top) - size;
#endif

    return { base, static_cast<char*>(base) + size };
}

std::pair<void*, void*> get_heap_bounds() {
    void* end = sbrk(0);
    return { nullptr, end };
}

void try_access_memory(uintptr_t addr) {
    struct sigaction sa = {};
    sa.sa_handler = segv_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);

    if (sigsetjmp(env, 1) == 0) {
        volatile int value = *reinterpret_cast<volatile int*>(addr);
        std::cout << "[*] Value at 0x" << std::hex << addr
                  << std::dec << ": " << value << "\n";
    } else {
        std::cout << "[!] Caught SIGSEGV at 0x" << std::hex << addr << std::dec << "\n";
    }
}
