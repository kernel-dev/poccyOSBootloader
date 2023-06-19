#ifndef KERNELLOADER_H
#define KERNELLOADER_H

#include "../Common/Acpi/KernEfiAcpi.h"
#include "../Common/Graphics/KernGop.h"
#include "../Common/Memory/KernEfiMem.h"

#include <Uefi.h>

#include <Library/UefiLib.h>

/**
    Function that handles reading, parsing,
    appropriately relocating the image and calling the EP
    from a PE32+ image. Thank you to Marvin Häuser (https://github.com/mhaeuser)
    for providing a rich, abstracted library (PeCoffLib2) to ease
    my pain with this.

    @param[in]  ImageHandle             The image handle.
    @param[in]  SystemTable             Pointer to the system table.
    @param[in]  Dsdt                    Pointer to the DSDT.
    @param[in]  FB                      Pointer to the KERN_FRAMEBUFFER
    @param[in]  GOP                     Pointer to the GOP handle.

    @retval     EFI_NOT_FOUND           Some instance was unable to be located. GOP, MemoryMap, etc.
    @retval     EFI_INVALID_PARAMETER   A value that was used internally was not the expected one.
    @retval     EFI_SUCCESS             This should never be reached. The kernel should take
                                        over control after its EP has been called.
 **/
EFI_STATUS
RunKernelPE (
  IN EFI_HANDLE                                   ImageHandle,
  IN EFI_SYSTEM_TABLE                             *SystemTable,
  IN ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE  **Dsdt,
  IN KERN_FRAMEBUFFER                             *FB,
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL                 *GOP
  );

#endif /* KernelLoader.h */
