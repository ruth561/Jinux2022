#include "elf.hpp"
#include "logging.hpp"
extern logging::Logger *logger;
int printk(const char *format, ...);


namespace
{
    /* 
     * セクション番号 num のセクションヘッダのアドレスを返す
     * ehdr にはELFヘッダの構造体を、num にはセクション番号を渡す 
     */
    Elf64_Shdr *get_shdr(Elf64_Ehdr *ehdr, int num){
        Elf64_Shdr *shdr;
        uint64_t head = reinterpret_cast<uint64_t>(ehdr);

        if (num > ehdr->e_shnum){
            // logger->debug("cannot find shdr (%d)\n", num);
            return NULL;
        }
        shdr = reinterpret_cast<Elf64_Shdr *>(head + ehdr->e_shoff) + num;
        return shdr;
    }

    /* 
     * セクションを名前で検索する関数
     * ehdrにはELFヘッダ構造体へのポインタを、nameにはセクションの名前を渡す。
     * 一致するものがあれば、そのセクションのヘッダ構造体へのポインタを返す。
     * 一致するものがなければ、NULLを返す。
     */
    Elf64_Shdr *search_shdr(Elf64_Ehdr *ehdr, const char *name){
        Elf64_Shdr *shdr;
        uint64_t head = reinterpret_cast<uint64_t>(ehdr); // ELFファイルの先頭アドレス
        char *shstrndx; // セクションの名前が格納されたセクションの先頭ポインタ

        shstrndx = reinterpret_cast<char *>(head + get_shdr(ehdr, ehdr->e_shstrndx)->sh_offset);
        for (int i = 0; i < ehdr->e_shnum; i++){
            shdr = get_shdr(ehdr, i);
            if (!strcmp(name, shstrndx + shdr->sh_name)){
                // logger->debug("found %s\n", name);
                return shdr;
            }
        }

        // logger->debug("cannot find shdr %s\n", name);
        return NULL;
    }
}


AppFunc *LoadElfFile(uint64_t head){
    Elf64_Ehdr *ehdr;
    Elf64_Phdr *phdr;
    Elf64_Shdr *shdr;

    ehdr = reinterpret_cast<Elf64_Ehdr *>(head);
    phdr = reinterpret_cast<Elf64_Phdr *>(head + ehdr->e_phoff);

    logger->info("[+] NOW LOADING ELF FILE...\n");
    for (int i = 0; i < ehdr->e_phnum; i++){
        // logger->debug("Program Header %d: ", i);
        switch (phdr->p_type){
            case PT_LOAD:
                // logger->debug("type: LOAD\n");
                // セグメントをそのまま仮想アドレス（phdr->p_vaddr）にコピーする
                // リンク時に、リンカスクリプトで先頭に空きアドレスをつくているので、
                // 新たにメモリを確保する必要はない。
                memcpy(reinterpret_cast<void *>(phdr->p_vaddr), 
                       reinterpret_cast<void *>(head + phdr->p_offset), 
                       phdr->p_filesz);
                // logger->debug("(loaded)\n");
                break;
            default:
                // logger->debug("type: OTHERS\n");
                break;
        }
        phdr++;
    }
    // ここまでで、ELFファイルのメモリへのマッピングが終了した。
    logger->info("[+] PROGRAM IS MAPPED TO MEMORY.\n");


    // BSS領域の０クリア
    shdr = search_shdr(ehdr, ".bss");
    if (shdr){
        logger->info("[+] CLEAR BSS: 0x%016lx ~ 0x%016lx\n", shdr->sh_addr, shdr->sh_addr + shdr->sh_size);
        memset(reinterpret_cast<void *>(shdr->sh_addr), 0, shdr->sh_size);
    }

    logger->info("[+] ENTRY POINT: 0x%016lx\n", ehdr->e_entry);
    return reinterpret_cast<AppFunc *>(ehdr->e_entry);
}
