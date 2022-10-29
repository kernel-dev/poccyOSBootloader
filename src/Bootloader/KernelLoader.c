#include "../Common/Boot/KernelLoader.h"
#include "../Common/Boot/Bootloader.h"

#include "../Common/Memory/KernEfiMem.h"

#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiImageLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/PeCoffLib2.h>

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
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL                 *GOP)
{
    EFI_STATUS                  Status;
    EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage;
    EFI_GUID                    LoadedImageProtocol = LOADED_IMAGE_PROTOCOL;
    
    Print(L"BEGINNING SEARCH FOR KERNEL PE...\r\n");

    //
    //  Allocate pool memory where we can place
    //  the Loaded Image buffer into.
    //
    Status = SystemTable->BootServices->AllocatePool (
                EfiBootServicesData,
                sizeof(EFI_LOADED_IMAGE_PROTOCOL),
                (VOID **)&LoadedImage);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR LOADED IMAGE\r\n");

    //
    //  Obtain the Loaded Image.
    //
    Status = SystemTable->BootServices->OpenProtocol (
                ImageHandle,
                &LoadedImageProtocol,
                (VOID **)&LoadedImage,
                ImageHandle,
                NULL,
                EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    HANDLE_STATUS(
        Status,
        L"FAILED TO OPEN IMAGE\r\n");

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_GUID                        FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    //
    //  Allocate pool memory for the FS handle.
    //
    Status = SystemTable->BootServices->AllocatePool (
                EfiBootServicesData,
                sizeof(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL),
                (VOID **)&FileSystem);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR FILE SYSTEM\r\n");

    //
    //  Obtain the FileSystem handle.
    //
    Status = SystemTable->BootServices->OpenProtocol (
                LoadedImage->DeviceHandle,
                &FileSystemProtocol,
                (VOID **)&FileSystem,
                ImageHandle,
                NULL,
                EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    HANDLE_STATUS(
        Status,
        L"FAILED TO OPEN PROTOCOL BY HANDLE\r\n");

    EFI_FILE    *CurrentDriveRoot;

    Status = SystemTable->BootServices->AllocatePool (
                EfiLoaderCode,
                sizeof(EFI_FILE_PROTOCOL),
                (VOID **)&CurrentDriveRoot);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR CURRENT DRIVE ROOT\r\n");

    //
    //  Open the current drive root using the FS handle.
    //
    Status = FileSystem->OpenVolume (FileSystem, &CurrentDriveRoot);
    HANDLE_STATUS(
        Status,
        L"FAILED TO OPEN CURRENT DRIVE\r\n");

    EFI_FILE *KernelFile;

    //
    //  Allocate pool memory to load the kernel
    //  file's buffer into.
    //
    Status = SystemTable->BootServices->AllocatePool (
                EfiLoaderCode,
                sizeof(EFI_FILE_PROTOCOL),
                (VOID **)&KernelFile);
    HANDLE_STATUS(
        Status, 
        L"FAILED TO ALLOCATE MEMORY FOR KERNEL FILE\r\n");

    //
    //  Read the kernel file's contents
    //  into the previously allocated buffer.
    //
    Status = CurrentDriveRoot->Open (
        CurrentDriveRoot,
        &KernelFile,
        L"kernel.bin",
        EFI_FILE_MODE_READ,
        EFI_FILE_READ_ONLY);
    HANDLE_STATUS(
        Status,
        L"KERNEL FILE IS MISSING...\r\n");

    EFI_PHYSICAL_ADDRESS    HeaderMemory = 0;
    UINTN                   FileInfoSize;

    //
    //  Get the size of the FileInfo buffer.
    //
    KernelFile->GetInfo (
        KernelFile, 
        &gEfiFileInfoGuid, 
        &FileInfoSize,
        NULL);

    //
    //  FileInfo not available?
    //
    if (FileInfoSize == 0)
    {
        return EFI_NOT_FOUND;
    }


    EFI_FILE_INFO *FileInfo;

    //
    //  Allocate pool memory for the FileInfo struct.
    //
    Status = SystemTable->BootServices->AllocatePool (
                EfiLoaderCode,
                FileInfoSize,
                (VOID **)&FileInfo);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR FILE INFO PTR\r\n");

    //
    //  Obtain the pointer to the FileInfo struct.
    //
    Status = KernelFile->GetInfo (
                KernelFile, 
                &gEfiFileInfoGuid, 
                &FileInfoSize, 
                FileInfo);
    HANDLE_STATUS(
        Status,
        L"FAILED TO OBTAIN FILE INFO FROM KERNEL\r\n");

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
    Print(L"File name: %s\r\n", FileInfo->FileName);
    Print(L"Size: %llu\r\n", FileInfo->FileSize);
    Print(L"Physical size: %llu\r\n", FileInfo->PhysicalSize);
    Print(L"Attribute: %llx\r\n", FileInfo->Attribute);

    VOID *Kernel;

    //
    //  Allocate pool memory for actually
    //  reading the data of the kernel file.
    //
    Status = SystemTable->BootServices->AllocatePool (
                EfiLoaderCode, 
                FileInfo->FileSize, 
                &Kernel);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY POOL FOR FILE BUFFER\r\n");

    //
    //  Place the contents of the file into
    //  the "Kernel" buffer so that we don't have to
    //  call `KernelFile->Read()' constantly.
    //
    Status = KernelFile->Read (
                KernelFile,
                &(FileInfo->FileSize),
                (VOID *)Kernel);
    HANDLE_STATUS(
        Status,
        L"FAILED TO READ CONTENTS OF FILE INTO BUFFER\r\n");

    //
    //  Failed to read the file buffer...
    //
    if (Kernel == NULL)
    {
        Print(L"FAILED TO READ KERNEL... HALTING BOOT.\r\n");

        return EFI_NOT_FOUND;
    }

    //
    //  Special thank you to Marvin Häuser (https://github.com/mhaeuser)
    //  for providing me with a rich library (PeCoffLib2)
    //  which abstracts and minimizes my pain
    //  for PE32+ image relocations.
    //

    PE_COFF_LOADER_IMAGE_CONTEXT *Context = NULL;
    EFI_PHYSICAL_ADDRESS         LoadImg = 0;

    //
    //  Initialize image context.
    //
    Status = PeCoffInitializeContext (
                Context,
                Kernel, 
                FileInfo->FileSize);
    HANDLE_STATUS(
        Status,
        L"FAILED TO INITIALIZE CONTEXT\r\n");

    Print(L"SUCCESSFULLY INITIALIZED CONTEXT\r\n");

    UINT32 FinalSize;

    //
    //  Calculate the size, in bytes, required
    //  for the destination Image memory space
    //  to load into.
    //
    Status = UefiImageLoaderGetDestinationSize (
            Context, 
            &FinalSize);
    HANDLE_STATUS(
        Status,
        L"FAILED TO OBTAIN DESTINATION SIZE\r\n");

    if (FinalSize < 1)
    {
        Print(L"INVALID DESTINATION SIZE\r\n");

        return EFI_INVALID_PARAMETER;
    }

    Print(L"FinalSize = %llu\r\n", FinalSize);

    //
    //  Allocate a sufficient amount of 4KiB
    //  pages for the loaded image.
    //
    Status = SystemTable->BootServices->AllocatePages (
                AllocateAnyPages, 
                EfiLoaderCode,
                EFI_SIZE_TO_PAGES(FinalSize),
                &LoadImg);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR IMAGE\r\n");
    
    Print(L"SUCCESSFULLY ALLOCATED MEMORY FOR IMAGE\r\n");

    //
    //  Actually load the image from the context.
    //
    Status = PeCoffLoadImage (
                Context,
                (VOID *)LoadImg, 
                FinalSize);
    HANDLE_STATUS(
        Status,
        L"FAILED TO LOAD IMAGE FROM CONTEXT\r\n");

    Print(L"SUCCESSFULLY LOADED IMAGE FROM CONTEXT\r\n");

    Print(L"Size of Image = %llu\r\n", Context->SizeOfImage);
    Print(L"Address of entry point = 0x%llx\r\n", Context->AddressOfEntryPoint);

    //
    //  Get the base address of the Image.
    //
    UINTN BaseAddress = PeCoffLoaderGetImageAddress (Context);

    //
    //  Ensure that the Image relocs
    //  are not stripped.
    //
    if (PeCoffGetRelocsStripped (Context))
    {
        Print(L"PE/COFF IMAGE RELOCS ARE STRIPPED! HALTING\r\n");

        return EFI_INVALID_PARAMETER;
    }

    //
    //  Relocate the image to its requested
    //  destination address for boot-time usage.
    //
    Status = PeCoffRelocateImage (
                Context,
                BaseAddress,
                NULL,
                0);
    HANDLE_STATUS(
        Status,
        L"FAILED TO RELOCATE IMAGE TO ADDRESS 0x%llx\r\n",
        BaseAddress);

    Print(L"BaseAddress = 0x%llx\r\n", (UINT64)BaseAddress);

    //
    //  Get the kernel's EP RVA.
    //
    UINT32 EntryPoint = PeCoffGetAddressOfEntryPoint (Context);

    Print(L"Address of entry point ctx = 0x%llx\r\n", Context->AddressOfEntryPoint);
    Print(L"Address of entry point = 0x%llx\r\n", EntryPoint);

    Print(L"ALLOCATING MEMORY POOL FOR ENTRY FUNCTION'S PARAMETERS...\r\n");

    LOADER_PARAMS *LoaderBlock;

    //
    //  Allocate pool memory for the parameters
    //  to pass down to the kernel EP.
    //
    Status = SystemTable->BootServices->AllocatePool (
                EfiLoaderCode,
                sizeof(LOADER_PARAMS),
                (VOID **)&LoaderBlock);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY POOL FOR PARAMETERS\r\n");

    Print(L"ALLOCATED MEMORY POOL!\r\n");

    Print(L"Hello, Kernel!\r\n");

    //
    //  Set the wanted video mode (1366x768).
    //
    GOP->SetMode (GOP, FB->CurrentMode);

    Print(L"[GOP]: Mode = %lu\r\n", FB->CurrentMode);
    Print(L"[GOP]: Successfully set mode.\r\n");

    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY POOL FOR GOP INFO\r\n");

    //
    //  "Populate" the framebuffer struct pointer
    //  with all the necessary information.
    //
    FB->FramebufferBase = GOP->Mode->FrameBufferBase;
    FB->FramebufferSize = GOP->Mode->FrameBufferSize;
    FB->HorizontalRes   = GOP->Mode->Info->HorizontalResolution;
    FB->VerticalRes     = GOP->Mode->Info->VerticalResolution;
    FB->PPS             = GOP->Mode->Info->PixelsPerScanLine;
    FB->PixelBitmask    = GOP->Mode->Info->PixelInformation;

    Print(L"[GOP]: FramebufferBase = 0x%llx\r\n", FB->FramebufferBase);
    Print(L"[GOP]: FramebufferSize = %llu\r\n", FB->FramebufferSize);

    Print(L"[GOP]: RedMask = %lu\r\n", FB->PixelBitmask.RedMask);
    Print(L"[GOP]: GreenMask = %lu\r\n", FB->PixelBitmask.GreenMask);
    Print(L"[GOP]: BlueMask = %lu\r\n", FB->PixelBitmask.BlueMask);
    Print(L"[GOP]: ReservedMask = %lu\r\n", FB->PixelBitmask.ReservedMask);

    EFI_KERN_MEMORY_MAP     MemoryMap;

    //
    //  Attempt to obtain the system memory map.
    //
    MemoryMap = EfiKernGetMemoryMap (ImageHandle, SystemTable);

    //
    //  This usually shouldn't happen...
    //
    if (MemoryMap.Empty == TRUE)
    {
        Print(L"KERN MEMORY MAP IS EMPTY!\r\n");

        return EFI_NOT_FOUND;
    }

    //
    //  Even weirder.
    //
    //  This should technically be
    //  a redundant check that could
    //  never possibly happen.
    //
    else if (MemoryMap.MemoryMap == NULL)
    {
        Print(L"EFI MEMORY MAP IS NULL BUT STRUCT MARKED AS NON-EMPTY!\r\n");

        return EFI_NOT_FOUND;
    }

    //
    //  Prepare the arguments to be passed down.
    //
    LoaderBlock->MemoryMap          = &MemoryMap;
    LoaderBlock->Dsdt               = Dsdt;
    LoaderBlock->RT                 = SystemTable->RuntimeServices;
    LoaderBlock->Framebuffer        = FB;

    //
    //  Exit boot services.
    //
    HANDLE_STATUS(
        SystemTable->BootServices->ExitBootServices(
            ImageHandle, 
            MemoryMap.MMapKey),
        L"FAILED TO EXIT BOOT SERVICES\r\n");

    //
    //  Locate the EP function and call it with the arguments.
    //
    typedef void (__attribute__((ms_abi)) *EntryPointFunction) (LOADER_PARAMS *LP);
    EntryPointFunction EntryPointPlaceholder = (EntryPointFunction) (BaseAddress + EntryPoint);
    EntryPointPlaceholder (LoaderBlock);

    // Should never reach here...
    return Status;
}