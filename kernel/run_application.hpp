#pragma once

#include <cstring>

#include "paging.hpp"
#include "memory_manager.hpp"
#include "asmfunc.h"
#include "logging.hpp"
#include "segment.hpp"
#include "elf.hpp"

/* 
 * 独自のPML4ページング構造体を実装し、0xffff800000000000アドレス以降を利用して
 * アプリケーションを特権レベル３で実行する関数。
 * タスクの１つとして実行されることを念頭に置いている。
 */
void RunApplication(uint64_t a, int64_t b);

/*
 * 指定したリニアアドレスが含まれるページに物理アドレスをマップする。
 * この関数はアプリケーション用のメモリ領域にのみ適応される。
 * つまり、linear address < 0xffff800000000000の時はマップしない。
 * writableは、その領域が書き込み可能として設定されるかを選べる。
 * 
 * 成功時１、失敗時０を返す
 */
int SetupPageMapForApp(LinearAddress4Level linear_address, bool writable);
