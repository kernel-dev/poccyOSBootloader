#include "../Common/Acpi/KernEfiAcpi.h"
#include "../Common/Memory/KernEfiMem.h"
#include "../Common/Graphics/KernGop.h"
#include "../Common/Boot/KernelLoader.h"
#include "../Common/Boot/Bootloader.h"

#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

EFI_STATUS
EFIAPI
KernEfiMain (
  EFI_HANDLE        ImageHandle,
  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp;
  EFI_ACPI_DESCRIPTION_HEADER                   *Rsdt;
  EFI_ACPI_DESCRIPTION_HEADER                   *Xsdt;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE     *Fadt;
  ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE   *Dsdt;

  //
  //  Obtain all the necessary ACPI tables
  //  through the RSDP (but first locate the RSDP.)
  //
  Status = EfiGetTables (
             &Rsdp,
             &Rsdt,
             &Xsdt,
             &Fadt,
             &Dsdt
             );

  HANDLE_STATUS (
    Status,
    L"FAILED TO OBTAIN ALL NECESSARY ACPI TABLES!\r\n"
    );

  if (Rsdp == NULL) {
    Print (L"[ACPI]: Could not find Rsdp...");

    return Status;
  } else if (EFI_ERROR (Status)) {
    Print (L"[ACPI]: Error occurred during discovery!\n");

    return Status;
  }

  Print (
    L"[RSDP]: OemId = %c%c%c%c%c%c\n",
    Rsdp->OemId[0],
    Rsdp->OemId[1],
    Rsdp->OemId[2],
    Rsdp->OemId[3],
    Rsdp->OemId[4],
    Rsdp->OemId[5]
    );

  Print (
    L"[RSDP]: Signature Valid = %s\n\n",
    Rsdp->Signature == EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE
            ? L"YES"
            : L"NO"
    );

  Print (
    L"[XSDT]: OemId = %c%c%c%c%c%c\n",
    Xsdt->OemId[0],
    Xsdt->OemId[1],
    Xsdt->OemId[2],
    Xsdt->OemId[3],
    Xsdt->OemId[4],
    Xsdt->OemId[5]
    );

  Print (
    L"[XSDT]: Signature Valid = %s\n\n",
    Xsdt->Signature == EFI_ACPI_2_0_EXTENDED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE
            ? L"YES"
            : L"NO"
    );

  Print (
    L"[DSDT]: OemId = %c%c%c%c%c%c\n",
    Dsdt->Sdt.OemId[0],
    Dsdt->Sdt.OemId[1],
    Dsdt->Sdt.OemId[2],
    Dsdt->Sdt.OemId[3],
    Dsdt->Sdt.OemId[4],
    Dsdt->Sdt.OemId[5]
    );

  Print (
    L"[DSDT]: Dsdt Bytecode Count = %llu\n\n",
    Dsdt->BytecodeCount
    );

  EFI_GRAPHICS_OUTPUT_PROTOCOL          *GOP       = NULL;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info      = NULL;
  UINTN                                 SizeOfInfo = 0;

  //
  //  Locate the GOP handle.
  //
  Status = KernLocateGop (
             SystemTable,
             &GOP
             );

  if (Status == EFI_NOT_FOUND) {
    Print (L"Failed to initialise GOP!\n");

    return Status;
  }

  if (GOP == NULL) {
    Print (L"[GOP]: GOP PTR is NULL. Halting...\r\n");

    return EFI_INVALID_PARAMETER;
  }

  Print (L"[GOP]: Successfully located the GOP instance\r\n");

  UINT32  SelectedMode = 0;

  if (KernModeAvailable (
        &SizeOfInfo,
        GOP,
        Info,
        &SelectedMode
        ) == FALSE)
  {
    Print (L"Requested video mode not available!\n");

    return Status;
  }

  Print (L"[GOP]: Video mode is available!\r\n");

  Print (L"[GOPInfo]: PixelBltOnly = %d\r\n", GOP->Mode->Info->PixelFormat == PixelBltOnly);
  Print (L"[GOPInfo]: PixelRGBA = %d\r\n", GOP->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor);
  Print (L"[GOPInfo]: PixelBGRA = %d\r\n", GOP->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor);
  Print (L"[GOPInfo]: PixelBitMask = %d\r\n", GOP->Mode->Info->PixelFormat == PixelBitMask);
  Print (L"[GOPInfo]: PixelFormatMax = %d\r\n", GOP->Mode->Info->PixelFormat == PixelFormatMax);

  //
  //  Ensure we're dealing with a valid 32-bit
  //  color pixel format.
  //
  if (
      (GOP->Mode->Info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor) &&
      (GOP->Mode->Info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor)
      )
  {
    Print (L"[GOPPixels]: Unsupported pixel format! Halting boot...\r\n");

    return EFI_INVALID_PARAMETER;
  }

  KERN_FRAMEBUFFER  *Framebuffer;

  //
  //  Allocate pool memory for the
  //  internal representation of the framebuffer
  //  obtained via the GOP handle.
  //
  Status = SystemTable->BootServices->AllocatePool (
                                        EfiLoaderCode,
                                        sizeof (KERN_FRAMEBUFFER),
                                        (VOID **)&Framebuffer
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO ALLOCATE MEMORY POOL FOR Framebuffer PTR\r\n"
    );

  //
  //  Only need the wanted video mode (1366x768),
  //  that is supported value to be set.
  //
  //  Setting the mode itself is handled
  //  within KernelLoader.c
  //
  Framebuffer->CurrentMode = SelectedMode;

  Print (L"[GOP]: Successfully obtained framebuffer.\r\n");

  //
  //  Go over the PE32+ image and
  //  attempt to successfully jump to
  //  the kernel's EP after everything has been done.
  //
  RunKernelPE (
    ImageHandle,
    SystemTable,
    &Dsdt,
    Framebuffer,
    GOP
    );

  return EFI_NOT_FOUND;
}
