#include "../Common/Acpi/KernEfiAcpi.h"
#include "../Common/Memory/KernEfiMem.h"
#include "../Common/Graphics/KernGop.h"
#include "KernelLoader.h"
#include "Bootloader.h"

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
        Print(L"[ACPI]: Could not find Rsdp...");

        return Status;
    }

    else if (EFI_ERROR(Status))
    {
        Print(L"[ACPI]: Error occurred during discovery!\n");

        return Status;
    }

    Print(
        L"[RSDP]: OemId = %c%c%c%c%c%c\n",
        Rsdp->OemId[0],
        Rsdp->OemId[1],
        Rsdp->OemId[2],
        Rsdp->OemId[3],
        Rsdp->OemId[4],
        Rsdp->OemId[5]);

    Print(
        L"[RSDP]: Signature Valid = %s\n\n", 
        Rsdp->Signature == EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE
            ? L"YES"
            : L"NO");

    Print(
        L"[XSDT]: OemId = %c%c%c%c%c%c\n",
        Xsdt->OemId[0],
        Xsdt->OemId[1],
        Xsdt->OemId[2],
        Xsdt->OemId[3],
        Xsdt->OemId[4],
        Xsdt->OemId[5]);

    Print(
        L"[XSDT]: Signature Valid = %s\n\n",
        Xsdt->Signature == EFI_ACPI_2_0_EXTENDED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE
            ? L"YES"
            : L"NO");

    Print(
        L"[DSDT]: OemId = %c%c%c%c%c%c\n",
        Dsdt->Sdt.OemId[0],
        Dsdt->Sdt.OemId[1],
        Dsdt->Sdt.OemId[2],
        Dsdt->Sdt.OemId[3],
        Dsdt->Sdt.OemId[4],
        Dsdt->Sdt.OemId[5]);


    Print(
        L"[DSDT]: Dsdt Bytecode Count = %llu\n\n",
        Dsdt->BytecodeCount);

    EFI_GRAPHICS_OUTPUT_PROTOCOL            *GOP;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info;
    UINTN                                   SizeOfInfo = 0;

    Status = SystemTable->BootServices->AllocatePool (
        EfiLoaderCode,
        sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL),
        (VOID **)&GOP);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY POOL FOR GOP PTR\r\n");

    Status = SystemTable->BootServices->AllocatePool (
        EfiLoaderCode,
        sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
        (VOID **)&Info);
    HANDLE_STATUS(
        Status, 
        L"FAILED TO ALLOCATE MEMORY POOL FOR Info PTR\r\n");

    Status = KernLocateGop(SystemTable, &GOP);

    if (Status == EFI_NOT_FOUND)
    {
        Print(L"Failed to initialise GOP!\n");

        return Status;
    }

    if (GOP == NULL)
    {
        Print(L"[GOP]: GOP PTR is NULL. Halting...\r\n");

        return EFI_INVALID_PARAMETER;
    }

    Print(L"[GOP]: Successfully located the GOP instance\r\n");

    Print(L"[GOP]: GOP MODE = %lu\r\n", GOP->Mode == NULL ? 0 : GOP->Mode->Mode);

    Status = KernGetVideoMode(&GOP, &Info, &SizeOfInfo);

    if (Status == EFI_NOT_FOUND)
    {
        Print(L"Failed to find GOP mode info!\n");

        return Status;
    }

    Print(L"[GOP]: Successfully found GOP mode info\r\n");
    UINT32 SelectedMode = 0;

    if (KernModeAvailable(
        &SizeOfInfo,
        &GOP,
        &Info,
        &SelectedMode) == FALSE)
    {
        Print(L"Requested video mode not available!\n");

        return Status;
    }

    Print(L"[GOP]: Video mode is available!\r\n");

    KERN_FRAMEBUFFER *Framebuffer;

    Status = SystemTable->BootServices->AllocatePool (
        EfiLoaderCode,
        sizeof(KERN_FRAMEBUFFER),
        (VOID **)&Framebuffer);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY POOL FOR Framebuffer PTR\r\n");
    
    Framebuffer->FramebufferBase = GOP->Mode->FrameBufferBase;
    Framebuffer->FramebufferSize = GOP->Mode->FrameBufferSize;
    Framebuffer->VerticalRes     = GOP->Mode->Info->VerticalResolution;
    Framebuffer->HorizontalRes   = GOP->Mode->Info->HorizontalResolution;
    Framebuffer->PPS             = GOP->Mode->Info->PixelsPerScanLine;
    Framebuffer->Width           = 4;
    Framebuffer->Pitch           = Framebuffer->Width * Framebuffer->PPS;
    Framebuffer->PixelBitmask    = GOP->Mode->Info->PixelInformation;
    Framebuffer->CurrentMode     = SelectedMode;

    Print(L"[GOP]: Successfully obtained framebuffer.\r\n");

    RunKernelPE(
        ImageHandle,
        SystemTable,
        &Dsdt,
        &Framebuffer,
        &GOP
    );

    return EFI_NOT_FOUND;
}