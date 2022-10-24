#include "../../Common/Graphics/KernGop.h"

#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS
KernLocateGop (
    IN  EFI_SYSTEM_TABLE                *SystemTable,
    OUT EFI_GRAPHICS_OUTPUT_PROTOCOL    *GOP)
{
    EFI_STATUS  Status;
    EFI_GUID    GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    Status = SystemTable->BootServices->LocateProtocol(
        &GopGuid, 
        NULL, 
        (VOID **)&GOP);

    if (EFI_ERROR(Status))
        return EFI_NOT_FOUND;

    return EFI_SUCCESS;
}

EFI_STATUS
KernGetVideoMode (
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL            *GOP,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info,
    OUT UINTN                                   *SizeOfInfo)
{
    EFI_STATUS  Status;

    Status = GOP->QueryMode(
        GOP, 
        GOP->Mode == NULL ? 0 : GOP->Mode->Mode,
        SizeOfInfo,
        &Info);

    if (Status == EFI_NOT_STARTED)
        GOP->SetMode(GOP, 0);

    else if (EFI_ERROR(Status))
        return Status;

    return EFI_SUCCESS;
}

BOOLEAN
KernModeAvailable (
    IN UINT32                                   VideoMode,
    IN UINTN                                    *SizeOfInfo,
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL             *GOP,
    IN EFI_GRAPHICS_OUTPUT_MODE_INFORMATION     *Info)
{
    UINTN NumberOfModes = GOP->Mode->MaxMode;
    UINTN NativeMode    = GOP->Mode->Mode;

    for (UINT64 Index = 0; Index < NumberOfModes; Index++)
    {
        GOP->QueryMode(GOP, Index, SizeOfInfo, &Info);

        if (Index == VideoMode)
            return TRUE;
    }

    return FALSE;
}