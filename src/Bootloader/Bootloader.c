#include "../Common/Acpi/KernEfiAcpi.h"
#include "../Common/Memory/KernEfiMem.h"
#include "../Common/Graphics/KernGop.h"
#include "KernelLoader.h"

#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

EFI_STATUS
EFIAPI
KernEfiMain (
    EFI_HANDLE ImageHandle, 
    EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS              Status;

    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp;
    EFI_ACPI_DESCRIPTION_HEADER                  *Rsdt;
    EFI_ACPI_DESCRIPTION_HEADER                  *Xsdt;
    EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE    *Fadt;
    ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE  *Dsdt;

    Status = EfiGetTables(&Rsdp, &Rsdt, &Xsdt, &Fadt, &Dsdt);

    if (Rsdp == NULL)
    {
        Print(L"===> [ACPI]: Could not find Rsdp...");

        return Status;
    }

    else if (EFI_ERROR(Status))
    {
        Print(L"===> [ACPI]: Error occurred during discovery!\n");

        return Status;
    }

    Print(
        L"===> [RSDP]: OemId = %c%c%c%c%c%c\n",
        Rsdp->OemId[0],
        Rsdp->OemId[1],
        Rsdp->OemId[2],
        Rsdp->OemId[3],
        Rsdp->OemId[4],
        Rsdp->OemId[5]);

    Print(
        L"===> [RSDP]: Signature Valid = %s\n\n", 
        Rsdp->Signature == EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE
            ? L"YES"
            : L"NO");

    Print(
        L"===> [XSDT]: OemId = %c%c%c%c%c%c\n",
        Xsdt->OemId[0],
        Xsdt->OemId[1],
        Xsdt->OemId[2],
        Xsdt->OemId[3],
        Xsdt->OemId[4],
        Xsdt->OemId[5]);

    Print(
        L"===> [XSDT]: Signature Valid = %s\n\n",
        Xsdt->Signature == EFI_ACPI_2_0_EXTENDED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE
            ? L"YES"
            : L"NO");

    Print(
        L"===> [DSDT]: OemId = %c%c%c%c%c%c\n",
        Dsdt->Sdt.OemId[0],
        Dsdt->Sdt.OemId[1],
        Dsdt->Sdt.OemId[2],
        Dsdt->Sdt.OemId[3],
        Dsdt->Sdt.OemId[4],
        Dsdt->Sdt.OemId[5]);


    Print(
        L"===> [DSDT]: Dsdt Bytecode Count = %llu\n\n",
        Dsdt->BytecodeCount);

    RunKernelPE(
        ImageHandle,
        SystemTable,
        Dsdt,
        NULL
    );

    EFI_GRAPHICS_OUTPUT_PROTOCOL            *GOP  = NULL;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info = NULL;
    UINTN                                   SizeOfInfo;

    Status = KernLocateGop(SystemTable, GOP);

    if (Status == EFI_NOT_FOUND)
    {
        Print(L"Failed to initialise GOP!\n");

        return Status;
    }

    Status = KernGetVideoMode(GOP, Info, &SizeOfInfo);

    if (Status == EFI_NOT_FOUND)
    {
        Print(L"Failed to find GOP mode info!\n");

        return Status;
    }

    if (KernModeAvailable(
        KERN_GRAPHICS_VIDEO_MODE,
        &SizeOfInfo,
        GOP,
        Info) == FALSE)
    {
        Print(L"Requested video mode not available!\n");

        return Status;
    }

    GOP->SetMode(GOP, KERN_GRAPHICS_VIDEO_MODE);

    KERN_FRAMEBUFFER Framebuffer = {
        .FramebufferBase = GOP->Mode->FrameBufferBase,
        .FramebufferSize = GOP->Mode->FrameBufferSize,
        .VerticalRes     = GOP->Mode->Info->VerticalResolution,
        .HorizontalRes   = GOP->Mode->Info->HorizontalResolution,
        .PPS             = GOP->Mode->Info->PixelsPerScanLine,
        .Pitch           = 8 * GOP->Mode->Info->PixelsPerScanLine,
        .Width           = 8,
        .PixelBitmask    = GOP->Mode->Info->PixelInformation
    };

    RunKernelPE(
        ImageHandle,
        SystemTable,
        Dsdt,
        &Framebuffer
    );


    return EFI_SUCCESS;
}