#ifndef KERNELLOADER_H
#define KERNELLOADER_H

#include "../Common/Acpi/KernEfiAcpi.h"
#include "../Common/Graphics/KernGop.h"
#include "../Common/Memory/KernEfiMem.h"

#include <Uefi.h>

#include <Library/UefiLib.h>

typedef struct {
    ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE *Dsdt;
    EFI_KERN_MEMORY_MAP                         *MemoryMap;
    KERN_FRAMEBUFFER                            *FB;
    EFI_RUNTIME_SERVICES                        *RT;
} LOADER_PARAMS;

EFI_STATUS
RunKernelPE (
    IN EFI_HANDLE                                   ImageHandle,
    IN EFI_SYSTEM_TABLE                             *SystemTable,
    IN ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE  *Dsdt,
    IN KERN_FRAMEBUFFER                             *FB);

#endif /* KernelLoader.h */