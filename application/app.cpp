
#include "sys/syscall.hpp"


extern "C" int main() {
    SyscallLogString("hello, world!\n");
    
    SyscallExit(1105);
}