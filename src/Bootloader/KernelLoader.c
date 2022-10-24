#include "KernelLoader.h"
#include "Bootloader.h"

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


// FIXME: Improper relocations yield an invalid entry point.
EFI_STATUS
RunKernelPE (
    IN EFI_HANDLE                                   ImageHandle,
    IN EFI_SYSTEM_TABLE                             *SystemTable,
    IN ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE  *Dsdt,
    IN KERN_FRAMEBUFFER                             *FB)
{
    EFI_STATUS                  Status;
    EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage;
    EFI_GUID                    LoadedImageProtocol = LOADED_IMAGE_PROTOCOL;
    
    Print(L"BEGINNING SEARCH FOR KERNEL PE...\r\n");

    Status = SystemTable->BootServices->AllocatePool (
                EfiBootServicesData,
                sizeof(EFI_LOADED_IMAGE_PROTOCOL),
                (VOID **)&LoadedImage);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR LOADED IMAGE\r\n");

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

    Status = SystemTable->BootServices->AllocatePool (
                EfiBootServicesData,
                sizeof(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL),
                (VOID **)&FileSystem);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR FILE SYSTEM\r\n");

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

    Status = FileSystem->OpenVolume (FileSystem, &CurrentDriveRoot);
    HANDLE_STATUS(
        Status,
        L"FAILED TO OPEN CURRENT DRIVE\r\n");

    EFI_FILE *KernelFile;
    Status = SystemTable->BootServices->AllocatePool (
                EfiLoaderCode,
                sizeof(EFI_FILE_PROTOCOL),
                (VOID **)&KernelFile);
    HANDLE_STATUS(
        Status, 
        L"FAILED TO ALLOCATE MEMORY FOR KERNEL FILE\r\n");

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
    KernelFile->GetInfo (
        KernelFile, 
        &gEfiFileInfoGuid, 
        &FileInfoSize,
        NULL);

    if (FileInfoSize == 0)
    {
        return EFI_NOT_FOUND;
    }


    EFI_FILE_INFO *FileInfo;
    Status = SystemTable->BootServices->AllocatePool (
                EfiLoaderCode,
                FileInfoSize,
                (VOID **)&FileInfo);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR FILE INFO PTR\r\n");

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

    VOID            *Kernel;

    Status = SystemTable->BootServices->AllocatePool (
                EfiLoaderCode, 
                FileInfo->FileSize, 
                &Kernel);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY POOL FOR FILE BUFFER\r\n");

    Status = KernelFile->Read (
                KernelFile,
                &(FileInfo->FileSize),
                (VOID *)Kernel);
    HANDLE_STATUS(
        Status,
        L"FAILED TO READ CONTENTS OF FILE INTO BUFFER\r\n");

    if (Kernel == NULL)
    {
        Print(L"FAILED TO READ KERNEL... HALTING BOOT.\r\n");

        return EFI_NOT_FOUND;
    }

    // CONST EFI_IMAGE_DOS_HEADER *Header = (CONST EFI_IMAGE_DOS_HEADER *) (CONST VOID *) ((CONST CHAR8 *)Kernel);

    // if (Header->e_magic != EFI_IMAGE_DOS_SIGNATURE) // Not MZ (in UINT16)
    // {
    //     Print(L"INVALID DOS HEADER SIGNATURE!\r\n");

    //     return EFI_INVALID_PARAMETER;
    // }

    // CONST EFI_IMAGE_NT_HEADERS_COMMON_HDR *NtHeader = (CONST EFI_IMAGE_NT_HEADERS_COMMON_HDR *) (CONST VOID *) (
    //     ((CONST CHAR8 *)Kernel) + (UINT64)Header->e_lfanew
    // );

    // if (NtHeader->Signature != EFI_IMAGE_NT_SIGNATURE) // Not "PE\0\0" (in UINT32)
    // {
    //     Print(L"INVALID SIGNATURE FOR NT!\r\n");

    //     return EFI_INVALID_PARAMETER;
    // }

    // Print(L"NT Signature: 0x%lx\r\n", NtHeader->Signature);

    // UINT16                  NumberOfSections = NtHeader->FileHeader.NumberOfSections;

    // Print(L"PE Header machine: 0x%x\r\n", NtHeader->FileHeader.Machine);
    // Print(L"PE Header numofsect: %lu\r\n", NumberOfSections);
    // Print(L"PE Header numofsyms: %llu\r\n", NtHeader->FileHeader.NumberOfSymbols);
    // Print(L"PE Header size of opt: %lu\r\n", NtHeader->FileHeader.SizeOfOptionalHeader);

    // CONST EFI_IMAGE_NT_HEADERS64 *OptionalHeader = (CONST EFI_IMAGE_NT_HEADERS64 *) (CONST VOID *) (
    //     ((CONST CHAR8 *)Kernel) + (UINT64)Header->e_lfanew + sizeof(EFI_IMAGE_NT_HEADERS_COMMON_HDR)
    // );

    // Print(L"Optional header magic: 0x%x\r\n", OptionalHeader->Magic);
    // Print(L"Optional header img_base: %llu\r\n", OptionalHeader->ImageBase);
    // Print(L"Optional header entry: %llu\r\n", OptionalHeader->AddressOfEntryPoint);
    // Print(L"Optional header major OS: %lu\r\n", OptionalHeader->MajorOperatingSystemVersion);
    // Print(L"Optional header minor OS: %lu\r\n", OptionalHeader->MinorOperatingSystemVersion);
    // Print(L"--> Image base: 0x%llx\r\n", OptionalHeader->ImageBase);
    // Print(L"--> Entry point addr: 0x%lx\r\n", OptionalHeader->AddressOfEntryPoint);

    // if (NtHeader->FileHeader.NumberOfSections == 0)
    // {
    //     Print(L"No data for sections... halting boot.\r\n");

    //     return EFI_UNSUPPORTED;
    // }

    // Print(L"ATTEMPTING TO READ SECTION HEADER TABLE...\r\n");

    // UINT32                              SectionAlignment = EFI_PAGE_SIZE;
    // UINT64                              VirtSize = 0;
    // CONST EFI_IMAGE_SECTION_HEADER      *SectionHeadersTable = (CONST EFI_IMAGE_SECTION_HEADER *) (CONST VOID *) (
    //     ((CONST CHAR8 *)Kernel) + sizeof(EFI_IMAGE_NT_HEADERS64) + (UINT64)Header->e_lfanew
    // );

    // Print(L"--> NumOfSec: %u\r\n", NumberOfSections);

    // Print(L"SUCCESSFULLY READ SECTION HEADER TABLE!\r\n");

    // if (OptionalHeader->SizeOfHeaders > FileSize)
    // {
    //     Print(L"Size of section headers is out of bounds.\r\n");

    //     return EFI_INVALID_PARAMETER;
    // }

    PE_COFF_LOADER_IMAGE_CONTEXT *Context = NULL;
    EFI_PHYSICAL_ADDRESS         LoadImg = 0;

    Status = PeCoffInitializeContext (
                Context,
                Kernel, 
                FileInfo->FileSize);
    HANDLE_STATUS(
        Status,
        L"FAILED TO INITIALIZE CONTEXT\r\n");

    Print(L"SUCCESSFULLY INITIALIZED CONTEXT\r\n");

    UINT32 FinalSize;
    HANDLE_STATUS(
        UefiImageLoaderGetDestinationSize (
            Context, 
            &FinalSize),
        L"FAILED TO OBTAIN DESTINATION SIZE\r\n");

    if (FinalSize < 1)
    {
        Print(L"INVALID DESTINATION SIZE\r\n");

        return EFI_INVALID_PARAMETER;
    }

    Print(L"FinalSize = %llu\r\n", FinalSize);

    Status = SystemTable->BootServices->AllocatePages (
                AllocateAnyPages, 
                EfiLoaderCode,
                EFI_SIZE_TO_PAGES(FinalSize),
                &LoadImg);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY FOR IMAGE\r\n");
    
    Print(L"SUCCESSFULLY ALLOCATED MEMORY FOR IMAGE\r\n");

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

    UINTN BaseAddress = PeCoffLoaderGetImageAddress (Context);

    if (PeCoffGetRelocsStripped (Context))
    {
        Print(L"PE/COFF IMAGE RELOCS ARE STRIPPED! HALTING\r\n");

        return EFI_INVALID_PARAMETER;
    }

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

    UINT32 EntryPoint = PeCoffGetAddressOfEntryPoint (Context);

    Print(L"Address of entry point ctx = 0x%llx\r\n", Context->AddressOfEntryPoint);
    Print(L"Address of entry point = 0x%llx\r\n", EntryPoint);

    Print(L"ALLOCATING MEMORY POOL FOR ENTRY FUNCTION'S PARAMETERS...\r\n");

    LOADER_PARAMS *LoaderBlock;
    Status = SystemTable->BootServices->AllocatePool (
                EfiLoaderCode,
                sizeof(LOADER_PARAMS),
                (VOID **)&LoaderBlock);
    HANDLE_STATUS(
        Status,
        L"FAILED TO ALLOCATE MEMORY POOL FOR PARAMETERS\r\n");

    Print(L"ALLOCATED MEMORY POOL!\r\n");

    Status = SystemTable->BootServices->FreePages (
                LoadImg, 
                EFI_SIZE_TO_PAGES(FinalSize));
    HANDLE_STATUS(
        Status,
        L"FAILED TO FREE ALLOCATED MEMORY PAGES\r\n");

    Print(L"FREED ALLOCATED MEMORY PAGES!\r\n");

    EFI_KERN_MEMORY_MAP     MemoryMap;

    MemoryMap = EfiKernGetMemoryMap (ImageHandle, SystemTable);

    if (MemoryMap.Empty == TRUE)
    {
        Print(L"KERN MEMORY MAP IS EMPTY!\r\n");

        return EFI_NOT_FOUND;
    }

    else if (MemoryMap.MemoryMap == NULL)
    {
        Print(L"EFI MEMORY MAP IS NULL!\r\n");

        return EFI_NOT_FOUND;
    }

    return EFI_NOT_FOUND;

    // else if (MemoryMap.MemoryMap != NULL)
    // {
    //     UINT64 Region                           = 1;
    //     UINT64 MemorySize                       = 0;
    //     EFI_MEMORY_DESCRIPTOR *MemoryMapEntry   = MemoryMap.MemoryMap;
    //     EFI_MEMORY_DESCRIPTOR *MemoryMapEnd     = (EFI_MEMORY_DESCRIPTOR *) (
    //         (UINT8 *)(MemoryMap.MemoryMap) + (MemoryMap.MemoryMapSize)
    //     );

    //     Print(
    //         L"===> [EFIMEM]: First mem map entry = %llu | Second mem map entry = %llu\n",
    //         MemoryMapEntry,
    //         MemoryMapEnd);

    //     while (MemoryMapEntry < MemoryMapEnd)
    //     {
    //         if (
    //             MemoryMapEntry->PhysicalStart > 0 && 
    //             MemoryMapEntry->PhysicalStart < MEMORY_UPPER_BOUNDARY
    //         )
    //         {
    //             Print(L"===> [EFIMEM]: Region = %llu\n", Region);
    //             Print(L"===> [EFIMEM]: Number of pages = %llu\n", MemoryMapEntry->NumberOfPages);
    //             Print(L"===> [EFIMEM]: Physical start = 0x%lx\n", MemoryMapEntry->PhysicalStart);
    //             Print(L"===> [EFIMEM]: Virtual start = 0x%lx\n\n", MemoryMapEntry->VirtualStart);
    //             Print(L"===> [EFIMEM]: Type = %u\n", MemoryMapEntry->Type);
    //             Print(L"===> [EFIMEM]: Attribute = %llu\n\n", MemoryMapEntry->Attribute);

    //             MemorySize += MemoryMapEntry->NumberOfPages * 4096;
    //         }

    //         MemoryMapEntry = NEXT_MEMORY_DESCRIPTOR(MemoryMapEntry, MemoryMap.DescriptorSize);
    //         Region++;
    //     }

    //     Print(L"===> [EFIMEM]: Memory size = %llu\n", MemorySize);
    //     Print(L"===> [EFIMEM]: Size of memory map = %llu\n", MemoryMap.MemoryMapSize);
    //     Print(L"===> [EFIMEM]: Size of each region = %llu\n\n", MemoryMap.DescriptorSize);
    // }

    LoaderBlock->MemoryMap = &MemoryMap;
    LoaderBlock->Dsdt      = Dsdt;
    LoaderBlock->RT        = SystemTable->RuntimeServices;
    LoaderBlock->FB        = FB;

    HANDLE_STATUS(
        SystemTable->BootServices->ExitBootServices(
            ImageHandle, 
            MemoryMap.MMapKey),
        L"FAILED TO EXIT BOOT SERVICES\r\n");

    typedef void (__attribute__((ms_abi)) *EntryPointFunction) (LOADER_PARAMS *LP);
    EntryPointFunction EntryPointPlaceholder = (EntryPointFunction) (BaseAddress + EntryPoint);
    EntryPointPlaceholder (LoaderBlock);

    // Should never reach here...
    return Status;
}