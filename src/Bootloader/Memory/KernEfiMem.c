#include "../../Common/Memory/KernEfiMem.h"

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

EFI_KERN_MEMORY_MAP
EfiKernGetMemoryMap (
    IN  EFI_HANDLE          ImageHandle, 
    IN  EFI_SYSTEM_TABLE    *SystemTable)
{
    EFI_STATUS              Status;
    EFI_KERN_MEMORY_MAP     EfiMemMap;
    UINTN                   MemoryMapSize = 0;
    UINTN                   MMapKey;
    UINTN                   DescriptorSize;
    UINT32                  DescriptorVersion;
    EFI_MEMORY_DESCRIPTOR   *MemoryMap;
    EFI_MEMORY_DESCRIPTOR   *MemoryMapEnd = NULL;

    Status = SystemTable->BootServices->GetMemoryMap(
        &MemoryMapSize, 
        MemoryMap, 
        &MMapKey, 
        &DescriptorSize, 
        &DescriptorVersion);
 
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        MemoryMap = (EFI_MEMORY_DESCRIPTOR *)AllocatePool(MemoryMapSize + (DescriptorSize * 2));
        UINTN NewMemoryMapSize = MemoryMapSize + (2 * DescriptorSize);

        Status = SystemTable->BootServices->GetMemoryMap(
            &NewMemoryMapSize,
            MemoryMap,
            &MMapKey,
            &DescriptorSize,
            &DescriptorVersion);

        if (EFI_ERROR(Status))
        {
            FreePool(MemoryMap);

            return (EFI_KERN_MEMORY_MAP){ .Empty = TRUE };
        }
    }

    else if (Status == EFI_INVALID_PARAMETER)
    {
        Print(L"===> [EFIMEM]: Invalid parameter!\n");
    }

    else if (EFI_ERROR(Status))
    {
        Print(
            L"===> [EFIMEM]: Obtaining memory map for loader failed!!\nSTATUS = %llu\n", 
            EFIERR(Status));

        return (EFI_KERN_MEMORY_MAP){ .Empty = TRUE };
    }

    if (MemoryMap == NULL)
        EfiMemMap = (EFI_KERN_MEMORY_MAP){ .Empty = TRUE }; // Memory map not found; the function should never reach this condition.

    else
        EfiMemMap = (EFI_KERN_MEMORY_MAP){
            .MemoryMapSize      = MemoryMapSize,
            .MMapKey            = MMapKey,
            .DescriptorSize     = DescriptorSize,
            .DescriptorVersion  = DescriptorVersion,
            .MemoryMap          = MemoryMap,
            .Empty              = FALSE
        };

    return EfiMemMap;
}