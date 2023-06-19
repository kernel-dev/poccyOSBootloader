#include "../../Common/Graphics/KernGop.h"

#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS
KernLocateGop (
  IN  EFI_SYSTEM_TABLE              *SystemTable,
  OUT EFI_GRAPHICS_OUTPUT_PROTOCOL  **GOP
  )
{
  EFI_STATUS  Status;
  EFI_GUID    GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

  //
  //  Locate the GOP using the GOP's default GUID.
  //
  Status = SystemTable->BootServices->LocateProtocol (
                                        &GopGuid,
                                        NULL,
                                        (VOID **)GOP
                                        );

  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
KernGetVideoMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *GOP,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info,
  OUT UINTN                                 *SizeOfInfo
  )
{
  EFI_STATUS  Status;

  Print (L"[GOPMode]: Querying current mode...\r\n");

  //
  //  Query for information about the default
  //  (or previously set) video mode.
  //
  Status = GOP->QueryMode (
                  GOP,
                  GOP->Mode == NULL ? 0 : GOP->Mode->Mode,
                  SizeOfInfo,
                  &Info
                  );

  //
  //  GOP not initialized yet, set video mode
  //  to the default value.
  //
  if (Status == EFI_NOT_STARTED) {
    Print (L"[GOPMode]: GOP was not initialized; setting to default...");

    Status = GOP->SetMode (GOP, 0);

    //
    //  Maybe the default mode is somehow
    //  unsupported?
    //
    if (EFI_ERROR (Status)) {
      Print (L"[GOPMode]: Failed to set GOP mode to default! | Status = %llu\r\n", Status);

      return Status;
    }
  } else if (EFI_ERROR (Status)) {
    Print (L"[GOPMode]: Unknown error. | Status = %llu\r\n", Status);

    return Status;
  }

  Print (L"[GOPMode]: Successfully queried current mode\r\n");

  return EFI_SUCCESS;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
BOOLEAN
KernModeAvailable (
  IN  UINTN                                     *SizeOfInfo,
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL              *GOP,
  IN  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info,
  OUT UINT32                                    *Mode
  )
{
  //
  //  The "top most" index of video modes.
  //
  UINT32  NumberOfModes = GOP->Mode->MaxMode;

  //
  //  The current specified video mode.
  //
  UINT32  NativeMode = GOP->Mode->Mode;

  //
  //  The wanted mode (in kernelOS's case, one that supports 1366x768)
  //
  UINT32  WantedMode = 0;

  Print (L"[GOPMode]: NumberOfModes = %llu\r\n", NumberOfModes);
  Print (L"[GOPMode]: NativeMode = %llu\r\n", NativeMode);

  for (UINT32 Index = 0; Index < NumberOfModes; Index++) {
    GOP->QueryMode (
           GOP,
           Index,
           SizeOfInfo,
           &Info
           );

    if (
        (Info->HorizontalResolution == 1280) &&
        (Info->VerticalResolution == 1024)
        )
    {
      WantedMode = Index;

      break;
    }
  }

  //
  //  If the wanted mode isn't the default
  //  mode, we know we can use it.
  //
  if (WantedMode != 0) {
    Mode = &WantedMode;

    return TRUE;
  }

  //
  //  Something went wrong.
  //
  return FALSE;
}

#pragma GCC diagnostic pop
