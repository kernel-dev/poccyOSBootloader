#include "KernelLoader.h"
#include "Bootloader.h"

#include "../Common/Acpi/KernEfiAcpi.h"
#include "../Common/Graphics/KernGop.h"
#include "../Common/Memory/KernEfiMem.h"

#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/PeCoffExtraActionLib.h>

#include <IndustryStandard/PeImage.h>

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

    HANDLE_STATUS(
        SystemTable->BootServices->AllocatePool(
            EfiBootServicesData,
            sizeof(EFI_LOADED_IMAGE_PROTOCOL),
            (VOID **)&LoadedImage),
        L"FAILED TO ALLOCATE MEMORY FOR LOADED IMAGE\r\n");

    HANDLE_STATUS(
        SystemTable->BootServices->OpenProtocol(
            ImageHandle,
            &LoadedImageProtocol,
            (VOID **)&LoadedImage,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL),
        L"FAILED TO OPEN IMAGE\r\n");

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_GUID                        FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    HANDLE_STATUS(
        SystemTable->BootServices->AllocatePool(
            EfiBootServicesData,
            sizeof(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL),
            (VOID **)&FileSystem),
        L"FAILED TO ALLOCATE MEMORY FOR FILE SYSTEM\r\n");

    HANDLE_STATUS(
        SystemTable->BootServices->OpenProtocol(
            LoadedImage->DeviceHandle,
            &FileSystemProtocol,
            (VOID **)&FileSystem,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL),
        L"FAILED TO OPEN PROTOCOL BY HANDLE\r\n");

    EFI_FILE    *CurrentDriveRoot;
    HANDLE_STATUS(
        SystemTable->BootServices->AllocatePool(
            EfiLoaderCode,
            sizeof(EFI_FILE_PROTOCOL),
            (VOID **)&CurrentDriveRoot),
        L"FAILED TO ALLOCATE MEMORY FOR CURRENT DRIVE ROOT\r\n");

    HANDLE_STATUS(
        FileSystem->OpenVolume(FileSystem, &CurrentDriveRoot),
        L"FAILED TO OPEN CURRENT DRIVE\r\n");

    EFI_FILE *KernelFile;
    HANDLE_STATUS(
        SystemTable->BootServices->AllocatePool(
            EfiLoaderCode,
            sizeof(EFI_FILE_PROTOCOL),
            (VOID **)&KernelFile), 
        L"FAILED TO ALLOCATE MEMORY FOR KERNEL FILE\r\n");

    Status = CurrentDriveRoot->Open(
        CurrentDriveRoot,
        &KernelFile,
        L"kernel.bin",
        EFI_FILE_MODE_READ,
        EFI_FILE_READ_ONLY);

    if (EFI_ERROR(Status))
    {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Kernel file is missing...\r\n");

        return Status;
    }

    EFI_PHYSICAL_ADDRESS    HeaderMemory = 0x400000;
    UINTN                   FileInfoSize;
    HANDLE_STATUS(
        KernelFile->GetInfo(
            KernelFile, 
            &gEfiFileInfoGuid, 
            &FileInfoSize,
            NULL),
        L"OBTAINING CORRECT SIZE FOR FILE INFO BUFFER...\r\n");

    EFI_FILE_INFO *FileInfo;
    HANDLE_STATUS(
        SystemTable->BootServices->AllocatePool(
            EfiLoaderCode,
            FileInfoSize,
            (VOID **)&FileInfo),
        L"FAILED TO ALLOCATE MEMORY FOR FILE INFO PTR\r\n");

    HANDLE_STATUS(
        KernelFile->GetInfo(
            KernelFile, 
            &gEfiFileInfoGuid, 
            &FileInfoSize, 
            FileInfo),
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

    CONST VOID      *Kernel;
    UINTN           FileSize = FileInfo->FileSize;

    HANDLE_STATUS(
        SystemTable->BootServices->AllocatePool(
            EfiLoaderCode, 
            FileSize, 
            &Kernel),
        L"FAILED TO ALLOCATE MEMORY POOL FOR FILE BUFFER\r\n");

    HANDLE_STATUS(
        KernelFile->Read(
            KernelFile,
            &FileSize,
            (VOID *)Kernel),
        L"FAILED TO READ CONTENTS OF FILE INTO BUFFER\r\n");

    if (Kernel == NULL)
    {
        Print(L"FAILED TO READ KERNEL... HALTING BOOT.\r\n");

        return EFI_NOT_FOUND;
    }

    CONST EFI_IMAGE_DOS_HEADER *Header = (CONST EFI_IMAGE_DOS_HEADER *) (CONST VOID *) ((CONST CHAR8 *)Kernel);

    if (Header->e_magic != EFI_IMAGE_DOS_SIGNATURE) // Not MZ (in UINT16)
    {
        Print(L"INVALID DOS HEADER SIGNATURE!\r\n");

        return EFI_INVALID_PARAMETER;
    }

    CONST EFI_IMAGE_NT_HEADERS64 *NtHeader = (CONST EFI_IMAGE_NT_HEADERS64 *) (CONST VOID *) (
        ((CONST CHAR8 *)Kernel) + (UINT64)Header->e_lfanew
    );

    if (NtHeader->Signature != EFI_IMAGE_NT_SIGNATURE) // Not "PE\0\0" (in UINT32)
    {
        Print(L"INVALID SIGNATURE FOR NT!\r\n");

        return EFI_INVALID_PARAMETER;
    }

    Print(L"NT Signature: 0x%lx\r\n", NtHeader->Signature);
    Print(L"NT file header addr: 0x%llx\r\n", &(NtHeader->FileHeader));
    Print(L"NT opt header addr: 0x%llx\r\n", &(NtHeader->OptionalHeader));

    EFI_IMAGE_FILE_HEADER   PE_Header = NtHeader->FileHeader;
    UINT16                  NumberOfSections = PE_Header.NumberOfSections;

    Print(L"PE Header machine: 0x%x\r\n", PE_Header.Machine);
    Print(L"PE Header numofsect: %lu\r\n", NumberOfSections);
    Print(L"PE Header numofsyms: %llu\r\n", PE_Header.NumberOfSymbols);
    Print(L"PE Header size of opt: %lu\r\n", PE_Header.SizeOfOptionalHeader);

    EFI_IMAGE_OPTIONAL_HEADER64 OptionalHeader = NtHeader->OptionalHeader;

    Print(L"Optional header magic: 0x%x\r\n", OptionalHeader.Magic);
    Print(L"Optional header img_base: %llu\r\n", OptionalHeader.ImageBase);
    Print(L"Optional header entry: %llu\r\n", OptionalHeader.AddressOfEntryPoint);
    Print(L"Optional header major OS: %lu\r\n", OptionalHeader.MajorOperatingSystemVersion);
    Print(L"Optional header minor OS: %lu\r\n", OptionalHeader.MinorOperatingSystemVersion);
    Print(L"--> Image base: 0x%llx\r\n", OptionalHeader.ImageBase);
    Print(L"--> Entry point addr: 0x%lx\r\n", OptionalHeader.AddressOfEntryPoint);

    if (PE_Header.NumberOfSections == 0)
    {
        Print(L"No data for sections... halting boot.\r\n");

        return EFI_UNSUPPORTED;
    }

    Print(L"ATTEMPTING TO READ SECTION HEADER TABLE...\r\n");

    UINT32                              SectionAlignment = EFI_PAGE_SIZE;
    UINT64                              VirtSize = 0;
    CONST EFI_IMAGE_SECTION_HEADER      *SectionHeadersTable = (CONST EFI_IMAGE_SECTION_HEADER *) (CONST VOID *) (
        ((CONST CHAR8 *)Kernel) + sizeof(EFI_IMAGE_NT_HEADERS64) + (UINT64)Header->e_lfanew
    );

    Print(L"--> NumOfSec: %u\r\n", NumberOfSections);

    Print(L"SUCCESSFULLY READ SECTION HEADER TABLE!\r\n");

    if (OptionalHeader.SizeOfHeaders > FileSize)
    {
        Print(L"Size of section headers is out of bounds.\r\n");

        return EFI_INVALID_PARAMETER;
    }

    // OptionalHeader.NumberOfRvaAndSizes

    for (UINT64 Index = 0; Index < NumberOfSections; Index++)
    {
        CONST EFI_IMAGE_SECTION_HEADER *SectionHeader = &SectionHeadersTable[Index];

        Print(L"Section header name: %s\r\n", SectionHeader->Name);
        Print(L"Section header virt addr: %lu\r\n", SectionHeader->VirtualAddress);
        Print(L"Section header virt size: %lu\r\n", SectionHeader->Misc.VirtualSize);
        Print(L"Section header charact: %lu\r\n\n", SectionHeader->Characteristics);

        VirtSize = (
            VirtSize > ((UINT64)(SectionHeader->VirtualAddress + SectionHeader->Misc.VirtualSize))
                ? VirtSize
                : ((UINT64)(SectionHeader->VirtualAddress + SectionHeader->Misc.VirtualSize))
        );
    }

    Print(L"-> VirtSize: %lu\r\n", VirtSize);

    if (VirtSize == 0)
    {
        Print(L"INVALID VIRT SIZE??\r\n");

        return EFI_INVALID_PARAMETER;
    }

    UINT64                  NumOfPages = EFI_SIZE_TO_PAGES(VirtSize);
    EFI_PHYSICAL_ADDRESS    AllocatedMemory = OptionalHeader.ImageBase;

    Print(L"Amount of pages to allocate: %llu\r\n", NumOfPages);

    HANDLE_STATUS(
        SystemTable->BootServices->AllocatePages(
            AllocateAnyPages, 
            EfiLoaderCode, 
            NumOfPages, 
            &AllocatedMemory),
        L"FAILED TO ALLOCATE PAGES FOR PE32+ SECTIONS\r\n");

    if (AllocatedMemory == 0)
        return EFI_NOT_FOUND;

    UINT64 OptionalHeaderSize = OptionalHeader.SizeOfHeaders;

    for (UINT64 Index = 0; Index < NumberOfSections; Index++)
    {
        CONST EFI_IMAGE_SECTION_HEADER      *SectionHeader  = &SectionHeadersTable[Index];
        UINTN                               RawDataSize     = (UINTN)SectionHeader->SizeOfRawData;
        EFI_PHYSICAL_ADDRESS                SectionAddress  = AllocatedMemory + (UINTN)SectionHeader->VirtualAddress;

        if (RawDataSize != 0)
            SectionAddress = (EFI_PHYSICAL_ADDRESS)(
                ((CONST CHAR8 *)Kernel) + SectionHeader->PointerToRawData
            );

        //
        // Apply relocation fixes, if necessary
        //
        if (
            AllocatedMemory != OptionalHeader.ImageBase &&
            OptionalHeader.NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC
        )
        {
            EFI_IMAGE_BASE_RELOCATION   *RelocationDirectoryBase;
            EFI_IMAGE_BASE_RELOCATION   *RelocTableEnd;

            RelocationDirectoryBase = (EFI_IMAGE_BASE_RELOCATION *)(
                AllocatedMemory + 
                (UINT64)OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress
            );
            RelocTableEnd = (EFI_IMAGE_BASE_RELOCATION *)(
                AllocatedMemory + 
                (UINT64)OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC].Size +
                (UINT64)OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress
            );

            // By how much we are off from the executable's intended ImageBase.
            UINT64 Delta = 0;

            if (AllocatedMemory != OptionalHeader.ImageBase)
            {
                // if (AllocatedMemory > OptionalHeader.ImageBase)
                Delta = AllocatedMemory - OptionalHeader.ImageBase;
                // else
                //     Delta = OptionalHeader.ImageBase - AllocatedMemory;
            }

            UINT64  NumRelocPerChunk;
            for (;(RelocationDirectoryBase->SizeOfBlock) && (RelocationDirectoryBase < RelocTableEnd);)
            {
                EFI_PHYSICAL_ADDRESS    Page        = AllocatedMemory + (UINT64)RelocationDirectoryBase->VirtualAddress;
                UINT16                  *DataToFix  = (UINT16 *)(
                    (UINT8 *)RelocationDirectoryBase + EFI_IMAGE_SIZEOF_BASE_RELOCATION
                );
                NumRelocPerChunk = (RelocationDirectoryBase->SizeOfBlock - EFI_IMAGE_SIZEOF_BASE_RELOCATION) / sizeof(UINT16);

                for (UINT64 NIndex = 0; NIndex < NumRelocPerChunk; NIndex++)
                {
                    if (DataToFix[NIndex] >> EFI_PAGE_SHIFT == 10)
                    {
                        // if (AllocatedMemory > OptionalHeader.ImageBase)
                        *((UINT64 *)((UINT8 *)Page + (DataToFix[NIndex] & EFI_PAGE_MASK))) += Delta;
                        // else
                        //     *((UINT64 *)((UINT8 *)Page + (DataToFix[NIndex] & EFI_PAGE_MASK))) -= Delta;
                    }

                    else
                    {
                        Status = EFI_INVALID_PARAMETER;

                        break;
                    }
                }

                RelocationDirectoryBase = (EFI_IMAGE_BASE_RELOCATION *)(
                    (UINT8 *)RelocationDirectoryBase + RelocationDirectoryBase->SizeOfBlock
                );
            }
        }
        else
            Print(L"[RELOC]: No relocation necessary!\r\n");

        if (Status == EFI_INVALID_PARAMETER)
        {
            HANDLE_STATUS(
                SystemTable->BootServices->FreePages(
                    AllocatedMemory, 
                    NumOfPages),
                L"FAILED TO FREE ALLOCATED PAGES!\r\n");

            HANDLE_STATUS(Status, L"RELOCATION FAILED!\r\n");
        }

        HeaderMemory = AllocatedMemory + (UINT64)OptionalHeader.AddressOfEntryPoint;
    }

    Print(L"ALLOCATING MEMORY POOL FOR ENTRY FUNCTION'S PARAMETERS...\r\n");

    LOADER_PARAMS *LoaderBlock;
    HANDLE_STATUS(
        SystemTable->BootServices->AllocatePool(
            EfiLoaderCode,
            sizeof(LOADER_PARAMS),
            (VOID **)&LoaderBlock),
        L"FAILED TO ALLOCATE MEMORY POOL FOR PARAMETERS\r\n");

    Print(L"ALLOCATED MEMORY POOL!\r\n");

    EFI_KERN_MEMORY_MAP     MemoryMap;

    MemoryMap = EfiKernGetMemoryMap(ImageHandle, SystemTable);

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

    typedef void (__attribute__((ms_abi)) *EntryPointFunction)(LOADER_PARAMS *LP);
    EntryPointFunction EntryPointPlaceholder = (EntryPointFunction)(HeaderMemory);
    EntryPointPlaceholder(LoaderBlock);

    // Should never reach here...
    return Status;
}