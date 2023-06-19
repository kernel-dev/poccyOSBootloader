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

#endif /* Bootloader.h */
