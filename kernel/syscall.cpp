#include "syscall.hpp"
#include "task.hpp"
#include <cstring>

int printk(const char *format, ...);
extern logging::Logger *logger;
extern TaskManager* task_manager;


#define IA32_EFER_ADDRESS 0xc000'0080u
#define IA32_STAR_ADDRESS 0xc000'0081u
#define IA32_LSTAR_ADDRESS 0xc000'0082u
#define IA32_FMASK_ADDRESS 0xc000'0084u

namespace syscall
{
    #define SYSCALL(name) \
        int64_t name(uint64_t arg1, uint64_t arg2, uint64_t arg3, \
                     uint64_t arg4, uint64_t arg5, uint64_t arg6)

    SYSCALL(Exit) { // 単に第一引数に渡された値を返り値として返す
        // logger->debug("SYSCALL EXIT HANDLE!!\n");
        return static_cast<int64_t>(arg1); // exit(0)のように呼ばれたら、0を返す
    }

    SYSCALL(SyscallLogString) { // 文字列の出力（syscall number 1）
        printk(reinterpret_cast<char *>(arg1));
        return 0;
    }

    #undef SYSCALL

}

/* 
 * rax: 返り値
 * rdi, rsi, rdx, rcx, r8, r9が引数
 */
using SyscallFuncType = int64_t (uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);

/* 
 * syscallが呼ばれた時に実行される関数を管理
 * syscall_table[rax]が呼び出される関数
 */
extern "C" std::array<SyscallFuncType*, 2> syscall_table{
    syscall::Exit,
    syscall::SyscallLogString, 
};


/* 
 * システムコール関連で他から呼び出される関数群
 */

// 現在実行中のアプリケーションを呼び出したときのOSスタックの値を返す。
extern "C" uint64_t GetOSStackInExitSyscall()
{
    __asm__("cli");
    Task *task = task_manager->CurrentTask();
    __asm__("sti");
    return task->GetOSStackPointer();
}


void InitializeSyscall()
{
    /* MSRの値を設定してゆく、、、 */
    logger->info("[+] Set syscall enable\n");

    IA32_EFER efer;
    efer.bits.ia32e_mode_active = 1;
    efer.bits.ia32e_mode_enable = 1;
    efer.bits.syscall_enable = 1;
    logger->debug("IA32_EFER: 0x%lx\n", efer.data);
    WriteMSR(IA32_EFER_ADDRESS, efer.data);

    IA32_STAR star;
    star.bits.syscall_cs_ss = kKernelCS; 
    star.bits.sysret_cs_ss = (kUserCS - 16) | 3; // 16の補正を行う
    logger->debug("IA32_STAR: 0x%lx\n", star.data);
    WriteMSR(IA32_STAR_ADDRESS, star.data);

    IA32_LSTAR lstar;
    lstar.syscall_target_address = reinterpret_cast<uint64_t>(SyscallEntry);
    logger->debug("IA32_LSTAR: 0x%lx\n", lstar.data);
    WriteMSR(IA32_LSTAR_ADDRESS, lstar.data);

    IA32_FMASK fmask;
    fmask.syscall_rflags = 0;
    logger->debug("IA32_FMASK: 0x%lx\n", fmask.data);
    WriteMSR(IA32_FMASK_ADDRESS, fmask.data);
}
