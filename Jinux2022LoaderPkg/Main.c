#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include  <Library/MemoryAllocationLib.h>
#include  <Library/BaseMemoryLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/BlockIo.h>
#include <Guid/FileInfo.h>
#include <elf.h>

struct MemoryMap {
    UINTN                   map_size;
    EFI_MEMORY_DESCRIPTOR   *mem_map;
    UINTN                   map_key;
    UINTN                   descriptor_size;
    UINT32                  descriptor_version;
};

enum PixelFormat {
    kPixelRGBResv8BitPerColor,
    kPixelBGRResv8BitPerColor
};

struct FrameBufferConfig {
    UINT8               *FrameBuffer;
    UINT32              PixelsPerScanLine;
    UINT32              HolizontalResolution;
    UINT32              VerticalResolution;
    enum PixelFormat    PixelFormat;
};

VOID Halt(VOID)
{
    while (1) asm("hlt");
}

VOID SettingConsoleView(EFI_SYSTEM_TABLE *system_table)
{
    /* コンソールの色を変更したり、カーソルを目に見えるようにしたりする。
       コンソールの背景色はシアンに、文字の色は白に設定した。 */
    system_table->ConOut->SetAttribute(system_table->ConOut, EFI_WHITE|EFI_BACKGROUND_CYAN);
    system_table->ConOut->ClearScreen(system_table->ConOut);
    system_table->ConOut->EnableCursor(system_table->ConOut, TRUE);
}

const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
    /* メモリタイプを渡して、そのタイプを表す文字列を返す関数 */
    switch (type) {
        case EfiReservedMemoryType: return L"EfiReservedMemoryType";
        case EfiLoaderCode: return L"EfiLoaderCode";
        case EfiLoaderData: return L"EfiLoaderData";
        case EfiBootServicesCode: return L"EfiBootServicesCode";
        case EfiBootServicesData: return L"EfiBootServicesData";
        case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
        case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
        case EfiConventionalMemory: return L"EfiConventionalMemory";
        case EfiUnusableMemory: return L"EfiUnusableMemory";
        case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
        case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
        case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
        case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
        case EfiPalCode: return L"EfiPalCode";
        case EfiPersistentMemory: return L"EfiPersistentMemory";
        case EfiMaxMemoryType: return L"EfiMaxMemoryType";
        default: return L"InvalidMemoryType";
    }
}

EFI_STATUS GetMemoryMap(struct MemoryMap *map)
{
    return gBS->GetMemoryMap(
        &map->map_size,
        map->mem_map,
        &map->map_key,
        &map->descriptor_size,
        &map->descriptor_version
    );
}

VOID PrintMemoryMap(struct MemoryMap map)
{
    UINTN i;
    EFI_PHYSICAL_ADDRESS iter;
    EFI_MEMORY_DESCRIPTOR *desc;
    
    Print(L"MemoryMapSize       -> 0x%016lx\n", map.map_size);
    Print(L"MemoryMap           -> 0x%016p\n",  map.mem_map);
    Print(L"MapKey              -> 0x%016lx\n", map.map_key);
    Print(L"DescriptorSize      -> 0x%016lx\n", map.descriptor_size);
    Print(L"DescriptorVersion   -> 0x%016lx\n", map.descriptor_version);
    Print(L"\n");

    /* MemoryMap の各ディスクリプタを走査し、表形式で出力する */
    Print(L"   PhysicalStart      Pages      Attribute          MemoryType\n");
    for (iter = (EFI_PHYSICAL_ADDRESS) map.mem_map, i = 0;
         iter < (EFI_PHYSICAL_ADDRESS) map.mem_map + map.map_size;
         iter += map.descriptor_size, i++) {
        desc = (EFI_MEMORY_DESCRIPTOR *) iter;
        
        Print(L"%2u 0x%016lx 0x%08lx 0x%016lx %s\n",
            i, desc->PhysicalStart, 
            desc->NumberOfPages, desc->Attribute,
            GetMemoryTypeUnicode(desc->Type)
        );
    }
}

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL **root)
{
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_STATUS Status;

    Status = gBS->OpenProtocol(
        image_handle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **) &loaded_image,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );
    if (Status != EFI_SUCCESS) {
        Print(L"Failed to open protocol EFI_LOADED_IMAGE_PROTOCOL\n");
    }

    Status = gBS->OpenProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **) &fs,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );
    if (Status != EFI_SUCCESS) {
        Print(L"Failed to open protocol EFI_SIMPLE_FILE_SYSTEM_PROTOCOL\n");
    }

    return fs->OpenVolume(fs, root);
}

EFI_STATUS SaveMemoryMap(struct MemoryMap *map, EFI_FILE_PROTOCOL *file)
{
    CHAR8 buffer[256];
    UINTN len;
    EFI_STATUS Status;
    int i;
    EFI_PHYSICAL_ADDRESS iter;
    EFI_MEMORY_DESCRIPTOR *desc;
    
    CHAR8 *header = "Index, Type, Type(name), PhysicalStart, NemberOfPages, Attribute\n";
    len = AsciiStrLen(header);
    Status = file->Write(file, &len, header);
    if (Status == EFI_SUCCESS) {
        Print(L"Success to write header\n");
    } else {
        Print(L"Failed to write header\n");
    }
    Print(L"map->map_size = %08lx, map->mem_map = %p\n", map->map_size, map->mem_map);

    for (iter = (EFI_PHYSICAL_ADDRESS) map->mem_map, i = 0;
         iter < (EFI_PHYSICAL_ADDRESS) map->mem_map + map->map_size;
         iter += map->descriptor_size, i++) {
        desc = (EFI_MEMORY_DESCRIPTOR *) iter;
        
        len = AsciiSPrint(
            buffer, sizeof(buffer), 
            "%u, %x, %-ls, %08lx, %lx, %lx\n",
            i, desc->Type, GetMemoryTypeUnicode(desc->Type),
            desc->PhysicalStart, desc->NumberOfPages,
            desc->Attribute & 0xffffflu);
        file->Write(file, &len, buffer);
    }

    return EFI_SUCCESS;
}

EFI_STATUS Shell(EFI_SYSTEM_TABLE *system_table)
{
    /* キーからの入力を待ち受け、入力文字を画面に出力する関数
       入力を待ち受けるには、ひたすらキー入力バッファを確認する方法もあるが、
       イベント機能を用いることで、キーが押し込まれるまで待機するように
       したほうが CPU にはやさしい。
       イベントの待受状態にするには、ブートサービスの WaitForEvent 関数を使用する。
       入力から得られるデータは、キーボードから送信されるスキャンコードと、
       そのスキャンコードが対応しているユニコード文字からなるデータ構造である。
        */
    EFI_STATUS Status;
    UINTN index, i;
    EFI_INPUT_KEY key;
    CHAR16 String[80], Char[2];
    
    /* 一度画面をクリアする */
    system_table->ConOut->ClearScreen(system_table->ConOut);
    Print(L"Welecome to JINUX SHELL\n");

    /* 入力バッファに溜まっている文字をクリアする */
    Status = system_table->ConIn->Reset(system_table->ConIn, FALSE);
    if (Status == EFI_DEVICE_ERROR) {
        Print(L"[DEBUG] Device error on ConIn\n");
    }
    
    /* シェルっぽい何か */
    while (1) {
        Print(L"ruth@jinux> ");

        i = 0;

        /* 改行が入力されるまで文字入力を受け付ける */
        while (1) {
            /* キー入力まで待機する */
            gBS->WaitForEvent(1, &system_table->ConIn->WaitForKey, &index);

            system_table->ConIn->ReadKeyStroke(system_table->ConIn, &key);
            if (key.UnicodeChar == 0xd) {           // Enter Key
                Print(L"\n");
                String[i] = 0;
                break;
            } else if (key.UnicodeChar == 0x8) {    // Back Space Key
                if (i <= 0) continue;
                Char[0] = key.UnicodeChar;
                Char[1] = 0;
                system_table->ConOut->OutputString(system_table->ConOut, Char);
                i--;
            } else if (key.UnicodeChar != 0) {
                Char[0] = key.UnicodeChar;
                Char[1] = 0;
                system_table->ConOut->OutputString(system_table->ConOut, Char);
                String[i] = key.UnicodeChar;
                i++;
            }
        }

        /* 受け取った文字列がコマンドを起動できるか確認
           一致するコマンドがあれば、そのコマンドを実行する。 */
        if (!StrCmp(L"hello", String)) {
            Print(L"Hello, JINUX!!\n");
        } else if (!StrCmp(L"exit", String)) {
            Print(L"\n\nSee You ..\n");
            break;
        } else {
            Print(L"%s Command not found.\n", String);
        }
    }

    system_table->ConOut->ClearScreen(system_table->ConOut);
    return EFI_SUCCESS;
}

EFI_STATUS OpenGop(EFI_HANDLE image_handle, EFI_GRAPHICS_OUTPUT_PROTOCOL **gop)
{
    UINTN HandleCount = 0;
    EFI_HANDLE *HandleBuffer = NULL;
    EFI_STATUS Status;

    // プロトコルを使用しているハンドルを検索する。
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        &HandleCount,
        &HandleBuffer);
    if (Status == EFI_SUCCESS) {
        Print(L"[DEBUG] Graphics Output Protocol is supported by %ld handles.\n", HandleCount);
    } else {
        Print(L"Failed to LocateHandleBuffer\n");
    }

    Status = gBS->OpenProtocol(
        HandleBuffer[0],
        &gEfiGraphicsOutputProtocolGuid,
        (VOID **) gop,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    return Status;
}

void CalcLoadAddressRange(Elf64_Ehdr *ehdr, UINT64 *first, UINT64 *last)
{
    /* 与えられたELF形式をメモリに展開して時の、メモリの範囲を計算し、
       firstとlastに格納しかえす。メモリアドレスは、物理アドレスであるので注意する。 */
    Elf64_Phdr *phdr = (Elf64_Phdr *) ((char *) ehdr + ehdr->e_phoff);
    *first = MAX_UINT64;
    *last = 0;
    for (int i = 0; i < ehdr->e_phnum; i++, phdr++) {
        if (phdr->p_type != PT_LOAD) continue;
        *first = MIN(*first, phdr->p_paddr);
        *last = MAX(*last, phdr->p_paddr + phdr->p_memsz);
    }
}

void CopyLoadSegments(Elf64_Ehdr *ehdr)
{
    Elf64_Phdr *phdr = (Elf64_Phdr *) ((char *) ehdr + ehdr->e_phoff);
    UINT64 segm_in_file;

    for (int i = 0; i < ehdr->e_phnum; i++, phdr++) {
        if (phdr->p_type != PT_LOAD) continue;

        segm_in_file = (UINT64) ehdr + phdr->p_offset;
        CopyMem((VOID*) phdr->p_vaddr, (VOID*)segm_in_file, phdr->p_filesz);

        UINTN remain_bytes = phdr->p_memsz - phdr->p_filesz;
        SetMem((VOID*)(phdr->p_vaddr + phdr->p_filesz), remain_bytes, 0);
    }
}






EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) 
{
    CHAR16 *buffer[4 * 0x1000];
    struct MemoryMap map;
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *root, *memmap_file;


    SettingConsoleView(system_table);
    Print(L"Hello, Jinux\n");

    /* メモリマップの取得＆コンソールに出力 */
    map.map_size    = sizeof(buffer);
    map.mem_map     = (EFI_MEMORY_DESCRIPTOR *) buffer;
    GetMemoryMap(&map);
    // PrintMemoryMap(map);

    /* ルートディレクトリを開く */
    Print(L"Try to open root dir...\n");
    Status = OpenRootDir(image_handle, &root);
    if (Status != EFI_SUCCESS) {
        Print(L"Failed to open root dir\n");
    } else {
        Print(L"Success to open root dir\n");
    }

    /* 開いたルートディレクトリから、相対アドレスで指定したファイルを開く */
    Status = root->Open(root, &memmap_file, L"\\memmap",
        EFI_FILE_MODE_CREATE|EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE, 0);
    if (Status != EFI_SUCCESS) {
        Print(L"Failed to open memmap\n");
    } else {
        Print(L"Success to open memmap\n");
    }

    /* 開いたファイルに、取得したメモリマップを書き込む */
    Status = SaveMemoryMap(&map, memmap_file);
    if (Status != EFI_SUCCESS) {
        Print(L"Failed to save memory map\n");
    } else {
        Print(L"Success to save memory map\n");
    }
    memmap_file->Close(memmap_file);

    // Shell(system_table);


    /* \kernel に位置するカーネルのファイルを起動する */
    EFI_FILE_PROTOCOL *kernel_file;
    root->Open(
        root, 
        &kernel_file, 
        L"\\kernel.elf", 
        EFI_FILE_MODE_READ,
        0);
    
    /* ファイルの情報を取得する。
       ファイルの形式は EFI_FILE_INFO とする。
       これは、最後のファイル名を格納するメンバが可変なので、
       以下のようにバッファのサイズを決めた。 */
    UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 20;
    CHAR8 file_info_buffer[file_info_size];
    kernel_file->GetInfo(
        kernel_file, 
        &gEfiFileInfoGuid,
        &file_info_size, 
        &file_info_buffer);
    EFI_FILE_INFO *file_info = (EFI_FILE_INFO *) file_info_buffer;

    Print(L"Information about \\kernel.elf\n");
    Print(L"    FileName     -> %s\n", file_info->FileName);
    Print(L"    FileSize     -> 0x%lx\n", file_info->FileSize);
    Print(L"    PhysicalSize -> 0x%lx\n", file_info->PhysicalSize);
    
    /* 一旦カーネルファイルを適当なメモリ領域に展開し、
       ELF 形式の解析を行う。AllocatePool は、指定した大きさ分のメモリ領域を
       確保する関数である。バイト単位での確保が行われる。 */
    UINTN kernel_file_size = file_info->FileSize;
    VOID *kernel_buffer;
    Status = gBS->AllocatePool(EfiLoaderData, kernel_file_size, &kernel_buffer);
    if (EFI_ERROR(Status)) {
        Print(L"failed to allocate pool: %r\n", Status);
        Halt();
    }

    Status = kernel_file->Read(kernel_file, &kernel_file_size, kernel_buffer);
    if (EFI_ERROR(Status)) {
        Print(L"error: %r\n", Status);
        Halt();
    }

    Elf64_Ehdr *kernel_ehdr = (Elf64_Ehdr *) kernel_buffer;
    UINT64 kernel_first_addr, kernel_last_addr;
    CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);



    /* kernel.elfを解析して得られた情報から、ELF形式をマッピングするアドレスの範囲が
       分かったので、それにしたがってメモリ領域を確保しデータを読み込む。 */
    Print(L"[DEBUG] Kernel: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);
    Print(L"[DEBUG] Allocate Pages is %ld\n", (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000);
    Status = gBS->AllocatePages(
        AllocateAddress,
        EfiLoaderData,
        (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000,
        &kernel_first_addr);
    if (EFI_ERROR(Status)) {
        Print(L"failed to allocate pages: %r\n", Status);
        Halt();
    }
    Print(L"[DEBUG] Allocate Done!!!!\n");


    CopyLoadSegments(kernel_ehdr);
    Print(L"Kernel: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);

    Status = gBS->FreePool(kernel_buffer);                      // 確保した Pool 領域を開放する
    if (EFI_ERROR(Status)) {
        Print(L"failed to free pool: %r\n", Status);
        Halt();
    }





    /* グラフィック出力のためのフレームバッファを取得するために、
       Graphics Output Protocol を開く。 */
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    Status = OpenGop(image_handle, &gop);



    /* kernel を実行する前に、ブートサービスを終了する必要がある。
       ExitBootServices を実行するために、現在のメモリマップを取得する。
       その後、ExitBootServices を実行する。 */
    map.map_size    = sizeof(buffer);
    Status = GetMemoryMap(&map);
    if (EFI_ERROR(Status)) {
        Print(L"failed to get memory map: %r\n", Status);
        Halt();
    }
    Status = gBS->ExitBootServices(image_handle, map.map_key);
    if (EFI_ERROR(Status)) {
        Print(L"failed to exit BootService: %r\n", Status);
        Halt();
    }



    /* エントリポイントに渡すフレームバッファの設定を作成する */
    struct FrameBufferConfig config = {
        (UINT8 *) gop->Mode->FrameBufferBase,
        gop->Mode->Info->PixelsPerScanLine,
        gop->Mode->Info->HorizontalResolution,
        gop->Mode->Info->VerticalResolution,
        0
    };
    switch (gop->Mode->Info->PixelFormat) {
        case PixelRedGreenBlueReserved8BitPerColor:
            config.PixelFormat = kPixelRGBResv8BitPerColor;
            break;
        case PixelBlueGreenRedReserved8BitPerColor:
            config.PixelFormat = kPixelBGRResv8BitPerColor;
            break;
        default:
            Print(L"Unimplemented Pixel Format: %d\n", gop->Mode->Info->PixelFormat);
            Halt();
    }



    /* エントリポイント用の関数のタイプを定義している */
    kernel_ehdr = (Elf64_Ehdr *) kernel_first_addr;
    typedef void EntryPointType(struct FrameBufferConfig *, 
                                struct MemoryMap*);
    EntryPointType *entry_point = (EntryPointType *) kernel_ehdr->e_entry;


    entry_point(&config, &map);

    while (1) asm("hlt");
    return EFI_SUCCESS;
}
