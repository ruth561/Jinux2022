#pragma once

#include <elf.h>
#include <stdint.h>
#include <cstring>

/*
 * アプリケーションの取る形式
 * TODO: int argc, char *argv[]を引数に入れたい 
 */
using AppFunc = void ();

/* 
 * headから始まるELFファイルをメモリにロードする関数
 * BSSの初期化も行う。
 * 成功すれば、エントリーポイントのアドレスを返す。 
 */
AppFunc *LoadElfFile(uint64_t head);
