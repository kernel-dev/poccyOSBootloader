#include "../../Common/Graphics/KernGop.h"

#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS
KernLocateGop (
    IN  EFI_SYSTEM_TABLE                *SystemTable,
    OUT EFI_GRAPHICS_OUTPUT_PROTOCOL    **GOP)
{
    EFI_STATUS  Status;
    EFI_GUID    GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    Status = SystemTable->BootServices->LocateProtocol(
        &GopGuid,
        NULL, 
        (VOID **)GOP);

    if (EFI_ERROR(Status))
        return EFI_NOT_FOUND;

    return EFI_SUCCESS;
}

EFI_STATUS
KernGetVideoMode (
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL            **GOP,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    **Info,
    OUT UINTN                                   *SizeOfInfo)
{
    EFI_STATUS  Status;

    Print(L"[GOPMode]: Querying current mode...\r\n");

    Status = (*GOP)->QueryMode(
        *GOP,
        (*GOP)->Mode == NULL ? 0 : (*GOP)->Mode->Mode,
        SizeOfInfo,
        Info);

    if (Status == EFI_NOT_STARTED)
    {
        Print(L"[GOPMode]: GOP was not initialized; setting to default...");

        Status = (*GOP)->SetMode(*GOP, 0);

        if (EFI_ERROR(Status))
        {
            Print(L"[GOPMode]: Failed to set GOP mode to default! | Status = %llu\r\n", Status);

            return Status;
        }
    }

    else if (EFI_ERROR(Status))
    {
        Print(L"[GOPMode]: Unknown error. | Status = %llu\r\n", Status);

        return Status;
    }

    Print(L"[GOPMode]: Successfully queried current mode\r\n");

    return EFI_SUCCESS;
}

BOOLEAN
KernModeAvailable (
    IN  UINTN                                    *SizeOfInfo,
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL             **GOP,
    IN  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info,
    OUT UINT32                                   *Mode)
{
    UINT32 NumberOfModes = (*GOP)->Mode->MaxMode;
    UINT32 NativeMode    = (*GOP)->Mode->Mode;
    UINT32 CurrentMode   = 0;

    Print(L"[GOPMode]: NumberOfModes = %llu\r\n", NumberOfModes);
    Print(L"[GOPMode]: NativeMode = %llu\r\n", NativeMode);

    for (UINT32 Index = 0; Index < NumberOfModes; Index++)
    {
        (*GOP)->QueryMode((*GOP), Index, SizeOfInfo, Info);

        if (
            (*Info)->HorizontalResolution == 1366 &&
            (*Info)->VerticalResolution == 768
        ) {
            Print(L"INDEX = %lu\r\n", Index);

            CurrentMode = Index;

            break;
        }
    }

    if (CurrentMode != 0)
    {
        *Mode = CurrentMode;

        return TRUE;
    }

    return FALSE;
}