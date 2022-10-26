#ifndef KERNELLOADER_H
#define KERNELLOADER_H

#include "../Common/Acpi/KernEfiAcpi.h"
#include "../Common/Graphics/KernGop.h"
#include "../Common/Memory/KernEfiMem.h"

#include <Uefi.h>

#include <Library/UefiLib.h>

typedef struct {
    EFI_RUNTIME_SERVICES                         *RT;
    EFI_KERN_MEMORY_MAP                          *MemoryMap;
    ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE  **Dsdt;
    KERN_FRAMEBUFFER                             *Framebuffer;
} LOADER_PARAMS;

EFI_STATUS
RunKernelPE (
    IN EFI_HANDLE                                   ImageHandle,
    IN EFI_SYSTEM_TABLE                             *SystemTable,
    IN ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE  **Dsdt,
    IN KERN_FRAMEBUFFER                             *FB,
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL                 *GOP);

#endif /* KernelLoader.h */