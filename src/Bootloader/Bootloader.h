#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <Uefi.h>
#include <Library/UefiLib.h>

#define HANDLE_STATUS(STATUS, ...) \
    if (STATUS != EFI_SUCCESS) { \
        Print(__VA_ARGS__); \
        Print(L"==> STATUS CODE: %d\r\n", STATUS); \
        return STATUS; \
    }

// Source: https://github.com/mhaeuser/edk2/blob/2021-gsoc-secure-loader/MdePkg/Include/Library/PeCoffLib2.h#L30-L41
#define PCD_ALIGNMENT_POLICY_CONTIGUOUS_SECTIONS     BIT0
#define PCD_ALIGNMENT_POLICY_RELOCATION_BLOCK_SIZES  BIT1
#define PCD_ALIGNMENT_POLICY_CERTIFICATE_SIZES       BIT2

#endif /* Bootloader.h */