#pragma once

#include "asmfunc.h"
#include "logging.hpp"
#include "msr.hpp"
#include "segment.hpp"


/* 
 * MSR（Macine Specific Register）の中でシステムコールの設定に使われている
 * 部分の設定を行う。syscallはIA-32eの64-bitモードでしか使用できないため、
 * 設定はMSRということになっている。
 */
void InitializeSyscall();

