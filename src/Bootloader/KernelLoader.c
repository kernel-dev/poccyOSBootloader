#include "../Common/Boot/KernelLoader.h"
#include "../Common/Boot/Bootloader.h"

#include "../Common/Memory/KernEfiMem.h"

#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiImageLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/PeCoffLib2.h>
#include <Library/BaseOverflowLib.h>

#include <IndustryStandard/PeImage2.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/LoadFile2.h>
#include <Protocol/SimpleFileSystem.h>

#include <Guid/FileInfo.h>

EFI_STATUS
RunKernelPE (
  IN EFI_HANDLE                                   ImageHandle,
  IN EFI_SYSTEM_TABLE                             *SystemTable,
  IN ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE  **Dsdt,
  IN KERN_FRAMEBUFFER                             *FB,
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL                 *GOP
  )
{
  EFI_STATUS                 Status;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_GUID                   LoadedImageProtocol = LOADED_IMAGE_PROTOCOL;

  Print (L"BEGINNING SEARCH FOR KERNEL PE...\r\n");

  //
  //  Allocate pool memory where we can place
  //  the Loaded Image buffer into.
  //
  Status = SystemTable->BootServices->AllocatePool (
                                        EfiBootServicesData,
                                        sizeof (EFI_LOADED_IMAGE_PROTOCOL),
                                        (VOID **)&LoadedImage
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO ALLOCATE MEMORY FOR LOADED IMAGE\r\n"
    );

  //
  //  Obtain the Loaded Image.
  //
  Status = SystemTable->BootServices->OpenProtocol (
                                        ImageHandle,
                                        &LoadedImageProtocol,
                                        (VOID **)&LoadedImage,
                                        ImageHandle,
                                        NULL,
                                        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO OPEN IMAGE\r\n"
    );

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem;
  EFI_GUID                         FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

  //
  //  Allocate pool memory for the FS handle.
  //
  Status = SystemTable->BootServices->AllocatePool (
                                        EfiBootServicesData,
                                        sizeof (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL),
                                        (VOID **)&FileSystem
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO ALLOCATE MEMORY FOR FILE SYSTEM\r\n"
    );

  //
  //  Obtain the FileSystem handle.
  //
  Status = SystemTable->BootServices->OpenProtocol (
                                        LoadedImage->DeviceHandle,
                                        &FileSystemProtocol,
                                        (VOID **)&FileSystem,
                                        ImageHandle,
                                        NULL,
                                        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO OPEN PROTOCOL BY HANDLE\r\n"
    );

  EFI_FILE  *CurrentDriveRoot;

  Status = SystemTable->BootServices->AllocatePool (
                                        EfiLoaderCode,
                                        sizeof (EFI_FILE_PROTOCOL),
                                        (VOID **)&CurrentDriveRoot
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO ALLOCATE MEMORY FOR CURRENT DRIVE ROOT\r\n"
    );

  //
  //  Open the current drive root using the FS handle.
  //
  Status = FileSystem->OpenVolume (FileSystem, &CurrentDriveRoot);
  HANDLE_STATUS (
    Status,
    L"FAILED TO OPEN CURRENT DRIVE\r\n"
    );

  EFI_FILE  *KernelFile;
  EFI_FILE  *FontFile;

  //
  //  Allocate pool memory to load the kernel
  //  file's buffer into.
  //
  Status = SystemTable->BootServices->AllocatePool (
                                        EfiLoaderCode,
                                        sizeof (EFI_FILE_PROTOCOL),
                                        (VOID **)&KernelFile
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO ALLOCATE MEMORY FOR KERNEL FILE\r\n"
    );

  //
  //  Allocate pool memory to load
  //  the PSF font file.
  //
  Status = SystemTable->BootServices->AllocatePool (
                                        EfiLoaderCode,
                                        sizeof (EFI_FILE_PROTOCOL),
                                        (VOID **)&FontFile
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO ALLOCATE MEMORY POOL FOR FONT FILE\r\n"
    );

  //
  //  Read the kernel file's contents
  //  into the previously allocated buffer.
  //
  Status = CurrentDriveRoot->Open (
                               CurrentDriveRoot,
                               &KernelFile,
                               L"kernel.bin",
                               EFI_FILE_MODE_READ,
                               EFI_FILE_READ_ONLY
                               );
  HANDLE_STATUS (
    Status,
    L"KERNEL FILE IS MISSING...\r\n"
    );

  //
  //  Read the font file's contents
  //  into the previously allocated buffer.
  //
  Status = CurrentDriveRoot->Open (
                               CurrentDriveRoot,
                               &FontFile,
                               L"font.psf",
                               EFI_FILE_MODE_READ,
                               EFI_FILE_READ_ONLY
                               );
  HANDLE_STATUS (
    Status,
    L"FONT FILE (font.psf) IS MISSING...\r\n"
    );

  UINTN          FontFileInfoSize;
  EFI_FILE_INFO  *FontFileInfo = NULL;
  VOID           *Font;

  //
  //  Obtain the pointer to the FileInfo struct.
  //
  Status = FontFile->GetInfo (
                       FontFile,
                       &gEfiFileInfoGuid,
                       &FontFileInfoSize,
                       NULL
                       );

  if (Status == EFI_BUFFER_TOO_SMALL) {
    Status = SystemTable->BootServices->AllocatePool (
                                          EfiLoaderCode,
                                          FontFileInfoSize,
                                          (VOID **)&FontFileInfo
                                          );
  }

  Status = FontFile->GetInfo (
                       FontFile,
                       &gEfiFileInfoGuid,
                       &FontFileInfoSize,
                       FontFileInfo
                       );

  HANDLE_STATUS (
    Status,
    L"FAILED TO OBTAIN FILE INFO FROM PSF FONT FILE\r\n"
    );

  Print (L"Font file name: %s\r\n", FontFileInfo->FileName);
  Print (L"Size: %llu\r\n", FontFileInfo->FileSize);
  Print (L"Physical size: %llu\r\n", FontFileInfo->PhysicalSize);
  Print (L"Attribute: %llx\r\n", FontFileInfo->Attribute);

  //
  //  Allocate pool memory for actually
  //  reading the font file's contents.
  //
  Status = SystemTable->BootServices->AllocatePool (
                                        EfiLoaderCode,
                                        FontFileInfo->FileSize,
                                        (VOID **)&Font
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO ALLOCATE POOL MEMORY FOR 'Font' BUFFER!\r\n"
    );

  //
  //  Place the contents of the file into
  //  the "Font" buffer so that we don't have to
  //  call `FontFile->Read()` constantly.
  //
  Status = FontFile->Read (
                       FontFile,
                       &(FontFileInfo->FileSize),
                       Font
                       );
  HANDLE_STATUS (
    Status,
    L"FAILED TO READ CONTENTS OF (font) FILE INTO BUFFER!\r\n"
    );

  //
  //  Nothing to read??
  //
  if (Font == NULL) {
    return EFI_NOT_FOUND;
  }

  UINTN          FileInfoSize;
  EFI_FILE_INFO  *FileInfo = NULL;

  //
  //  Obtain the pointer to the FileInfo struct.
  //
  Status = KernelFile->GetInfo (
                         KernelFile,
                         &gEfiFileInfoGuid,
                         &FileInfoSize,
                         NULL
                         );

  if (Status == EFI_BUFFER_TOO_SMALL) {
    Status = SystemTable->BootServices->AllocatePool (
                                          EfiLoaderCode,
                                          FileInfoSize,
                                          (VOID **)&FileInfo
                                          );
  }

  Status = KernelFile->GetInfo (
                         KernelFile,
                         &gEfiFileInfoGuid,
                         &FileInfoSize,
                         FileInfo
                         );

  HANDLE_STATUS (
    Status,
    L"FAILED TO OBTAIN FILE INFO FROM KERNEL\r\n"
    );

  /**
      NOTE: File attribute values

      0x0000000000000001  = READ ONLY
      0x0000000000000002  = HIDDEN
      0x0000000000000004  = SYSTEM
      0x0000000000000008  = RESERVED
      0x0000000000000010  = DIRECTORY
      0x0000000000000020  = ARCHIVE
      0x0000000000000037  = VALID ATTRIBUTE
   **/
  Print (L"File name: %s\r\n", FileInfo->FileName);
  Print (L"Size: %llu\r\n", FileInfo->FileSize);
  Print (L"Physical size: %llu\r\n", FileInfo->PhysicalSize);
  Print (L"Attribute: %llx\r\n", FileInfo->Attribute);

  VOID  *Kernel;

  //
  //  Allocate pool memory for actually
  //  reading the data of the kernel file.
  //
  Status = SystemTable->BootServices->AllocatePool (
                                        EfiLoaderCode,
                                        FileInfo->FileSize,
                                        (VOID **)&Kernel
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO ALLOCATE MEMORY POOL FOR FILE BUFFER\r\n"
    );

  //
  //  Place the contents of the file into
  //  the "Kernel" buffer so that we don't have to
  //  call `KernelFile->Read()' constantly.
  //
  Status = KernelFile->Read (
                         KernelFile,
                         &(FileInfo->FileSize),
                         Kernel
                         );
  HANDLE_STATUS (
    Status,
    L"FAILED TO READ CONTENTS OF FILE INTO BUFFER\r\n"
    );

  //
  //  Failed to read the file buffer...
  //
  if (Kernel == NULL) {
    Print (L"FAILED TO READ KERNEL... HALTING BOOT.\r\n");

    return EFI_NOT_FOUND;
  }

  //
  //  Set the wanted video mode (1366x768).
  //
  GOP->SetMode (GOP, FB->CurrentMode);

  Print (L"[GOP]: Mode = %lu\r\n", FB->CurrentMode);
  Print (L"[GOP]: Successfully set mode.\r\n");

  //
  //  "Populate" the framebuffer struct pointer
  //  with all the necessary information.
  //
  FB->FramebufferBase = GOP->Mode->FrameBufferBase;
  FB->FramebufferSize = GOP->Mode->FrameBufferSize;
  FB->HorizontalRes   = GOP->Mode->Info->HorizontalResolution;
  FB->VerticalRes     = GOP->Mode->Info->VerticalResolution;
  FB->BPP             = 4;  // 32bits / 8 = 4 bytes
  FB->PPS             = GOP->Mode->Info->PixelsPerScanLine;
  FB->Pitch           = FB->PPS * FB->BPP;
  FB->PixelBitmask    = GOP->Mode->Info->PixelInformation;

  Print (L"[GOP]: FramebufferBase = 0x%llx\r\n", GOP->Mode->FrameBufferBase);
  Print (L"[GOP]: FramebufferEnd = 0x%llx\r\n", GOP->Mode->FrameBufferBase + GOP->Mode->FrameBufferSize);

  Print (L"[GOP]: Horizontal resolution = %lu\r\n", GOP->Mode->Info->HorizontalResolution);
  Print (L"[GOP]: Vertical resolution = %lu\r\n", GOP->Mode->Info->VerticalResolution);
  Print (L"[GOP]: PixelsPerScanLine = %lu\r\n", GOP->Mode->Info->PixelsPerScanLine);

  //
  //  Special thank you to Marvin Häuser (https://github.com/mhaeuser)
  //  for providing me with a rich library (PeCoffLib2)
  //  which abstracts and minimizes my pain
  //  for PE32+ image relocations.
  //

  PE_COFF_LOADER_IMAGE_CONTEXT  Context;
  EFI_PHYSICAL_ADDRESS          LoadImg = 0;

  //
  //  Initialize image context.
  //
  Status = PeCoffInitializeContext (
             &Context,
             Kernel,
             FileInfo->FileSize
             );
  HANDLE_STATUS (
    Status,
    L"FAILED TO INITIALIZE CONTEXT\r\n"
    );

  //
  //  Get the size, in bytes, required
  //  for the destination Image memory space
  //  to load into.
  //
  UINT32  FinalSize      = PeCoffGetSizeOfImage (&Context);
  UINT32  ImageAlignment = PeCoffGetSectionAlignment (&Context);

  if (FinalSize < 1) {
    Print (L"INVALID DESTINATION SIZE\r\n");

    return EFI_INVALID_PARAMETER;
  }

  if (ImageAlignment > EFI_PAGE_SIZE) {
    BOOLEAN  Overflow = BaseOverflowAddU32 (
                          FinalSize,
                          ImageAlignment - EFI_PAGE_SIZE,
                          &FinalSize
                          );

    if (Overflow) {
      Print (L"ALIGNMENT OVERFLOW: FAIL!");
      return EFI_INVALID_PARAMETER;
    }
  }

  Print (L"FinalSize = %llu\r\n", FinalSize);
  Print (L"ImageAlignment = %llu\r\n", ImageAlignment);

  //
  //  Allocate a sufficient amount of 4KiB
  //  pages for the loaded image.
  //
  Status = SystemTable->BootServices->AllocatePages (
                                        AllocateAnyPages,
                                        EfiLoaderCode,
                                        EFI_SIZE_TO_PAGES (FinalSize),
                                        &LoadImg
                                        );
  HANDLE_STATUS (
    Status,
    L"FAILED TO ALLOCATE MEMORY FOR IMAGE\r\n"
    );

  Print (L"SUCCESSFULLY ALLOCATED MEMORY FOR IMAGE\r\n");

  //
  //  Actually load the image from the context.
  //
  Status = PeCoffLoadImage (
             &Context,
             (VOID *)LoadImg,
             FinalSize
             );
  HANDLE_STATUS (
    Status,
    L"FAILED TO LOAD IMAGE FROM CONTEXT\r\n"
    );

  Print (L"SUCCESSFULLY LOADED IMAGE FROM CONTEXT\r\n");

  Print (L"Size of Image = %llu\r\n", Context.SizeOfImage);
  Print (L"Address of entry point = 0x%llx\r\n", Context.AddressOfEntryPoint);

  //
  //  Get the base address of the Image.
  //
  UINTN  BaseAddress = PeCoffLoaderGetImageAddress (&Context);

  //
  //  Ensure that the Image relocs
  //  are not stripped.
  //
  if (PeCoffGetRelocsStripped (&Context)) {
    Print (L"PE/COFF IMAGE RELOCS ARE STRIPPED! HALTING\r\n");

    return EFI_INVALID_PARAMETER;
  }

  //
  //  Relocate the image to its requested
  //  destination address for boot-time usage.
  //
  Status = PeCoffRelocateImage (
             &Context,
             BaseAddress,
             NULL,
             0
             );
  HANDLE_STATUS (
    Status,
    L"FAILED TO RELOCATE IMAGE TO ADDRESS 0x%llx\r\n",
    BaseAddress
    );

  Print (L"BaseAddress = 0x%llx\r\n", (UINT64)BaseAddress);

  //
  //  Get the kernel's EP RVA.
  //
  UINTN  EntryPoint = PeCoffGetAddressOfEntryPoint (&Context);

  Print (L"Base address = 0x%llx\r\n", (UINTN)BaseAddress);
  Print (L"Address of entry point = 0x%llx\r\n", EntryPoint);

  //
  //  Attempt to obtain the system memory map.
  //
  UINTN                  MemoryMapSize;
  UINTN                  MMapKey;
  UINTN                  DescriptorSize;
  UINT32                 DescriptorVersion;
  EFI_MEMORY_DESCRIPTOR  *MemoryMap = NULL;

  Status = SystemTable->BootServices->GetMemoryMap (
                                        &MemoryMapSize,
                                        MemoryMap,
                                        &MMapKey,
                                        &DescriptorSize,
                                        &DescriptorVersion
                                        );

  if (Status == EFI_BUFFER_TOO_SMALL) {
    Status = SystemTable->BootServices->AllocatePool (
                                          EfiLoaderCode,
                                          MemoryMapSize + DescriptorSize,
                                          (VOID **)&MemoryMap
                                          );

    HANDLE_STATUS (
      Status,
      L"FAILED TO ALLOCATE MEMORY FOR MEMORY MAP\r\n"
      );

    Status = SystemTable->BootServices->GetMemoryMap (
                                          &MemoryMapSize,
                                          MemoryMap,
                                          &MMapKey,
                                          &DescriptorSize,
                                          &DescriptorVersion
                                          );

    if (EFI_ERROR (Status)) {
      SystemTable->BootServices->FreePool (MemoryMap);

      HANDLE_STATUS (
        Status,
        L"FAILED TO FREE MEMORY MAP!!!\r\n"
        );
    }
  }

  if (MemoryMap == NULL) {
    Print (L"SYSTEM MEMORY MAP IS NULL!!\r\n");

    return EFI_NOT_FOUND;
  }

  //
  //  Exit boot services.
  //
  Status = SystemTable->BootServices->ExitBootServices (
                                        ImageHandle,
                                        MMapKey
                                        );

  if (EFI_ERROR (Status)) {
    SystemTable->BootServices->FreePool (MemoryMap);

    HANDLE_STATUS (
      Status,
      L"FAILED TO FREE MEM POOL!!!!\r\n"
      );

    MemoryMapSize = 0;

    Status = SystemTable->BootServices->GetMemoryMap (
                                          &MemoryMapSize,
                                          MemoryMap,
                                          &MMapKey,
                                          &DescriptorSize,
                                          &DescriptorVersion
                                          );

    if (Status == EFI_BUFFER_TOO_SMALL) {
      Status = SystemTable->BootServices->AllocatePool (
                                            EfiLoaderData,
                                            MemoryMapSize + DescriptorSize,
                                            (VOID **)&MemoryMap
                                            );

      HANDLE_STATUS (
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR MEMORY MAP...AGAIN!\r\n"
        );

      Status = SystemTable->BootServices->GetMemoryMap (
                                            &MemoryMapSize,
                                            MemoryMap,
                                            &MMapKey,
                                            &DescriptorSize,
                                            &DescriptorVersion
                                            );

      HANDLE_STATUS (
        Status,
        L"FAILED TO GET MEMORY MAP 2nD TIME AROUND...\r\n"
        );

      if (MemoryMap == NULL) {
        Print (L"MEMORY MAP IS NULL AGAIN???\r\n");

        return EFI_NOT_FOUND;
      }
    }

    Status = SystemTable->BootServices->ExitBootServices (
                                          ImageHandle,
                                          MMapKey
                                          );

    HANDLE_STATUS (
      Status,
      L"FAILED TO EXIT BOOT SERVICES\r\n"
      );
  }

  EFI_KERN_MEMORY_MAP  KernMemoryMap = (EFI_KERN_MEMORY_MAP) {
    .MemoryMapSize     = MemoryMapSize,
    .MMapKey           = MMapKey,
    .DescriptorSize    = DescriptorSize,
    .DescriptorVersion = DescriptorVersion,
    .MemoryMap         = MemoryMap,
    .Empty             = FALSE
  };

  //
  //  Locate the EP function and call it with the arguments.
  //
  typedef void (__attribute__ ((ms_abi)) *EntryPointFunction)(
  EFI_RUNTIME_SERVICES                         *RT,            /// Pointer to the runtime services.
  EFI_KERN_MEMORY_MAP                          *KernMemoryMap, /// Pointer to the EFI_KERN_MEMORY_MAP.
  ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE  **Dsdt,         /// Pointer to the DSDT pointer.
  KERN_FRAMEBUFFER                             *Framebuffer,   /// Pointer to the KERN_FRAMEBUFFER.
  VOID                                         *TerminalFont   /// Pointer to the PSF font file contents
  );

  EntryPointFunction  EntryPointPlaceholder = (EntryPointFunction)(BaseAddress + EntryPoint);

  EntryPointPlaceholder (
    SystemTable->RuntimeServices,
    &KernMemoryMap,
    Dsdt,
    FB,
    Font
    );

  // Should never reach here...
  return Status;
}
