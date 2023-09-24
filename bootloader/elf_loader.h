#ifndef ELF_LOADER_H
#define ELF_LOADER_H
#include "file_loader.h"
#include "elf.h"

EFI_STATUS ValidateElfHeader(
    Elf64_Ehdr* ElfHeader
);

EFI_STATUS ReadElfHeader(
    EFI_FILE* File,
    Elf64_Ehdr* ElfHeader
);

EFI_STATUS ReadProgramHeaders(
    EFI_FILE* File,
    Elf64_Ehdr* ElfHeader,
    Elf64_Phdr** ProgramHeaders
);

EFI_STATUS LoadElfSegments(
    EFI_FILE* File,
    Elf64_Phdr* ProgramHeaders,
    UINT16 NumHeaders,
    VOID** PhysicalBase,
    VOID** VirtualBase,
    UINT64* TotalSize
);

EFI_STATUS LoadElfFile(
    EFI_FILE* RootDir,
    CHAR16* FileName,
    VOID** EntryPoint,
    VOID** ElfPhysicalBase,
    VOID** ElfVirtualBase,
    UINT64* ElfSize
);

#endif // ELF_LOADER_H