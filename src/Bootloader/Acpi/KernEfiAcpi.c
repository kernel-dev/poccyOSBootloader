#include "../../Common/Acpi/KernEfiAcpi.h"
#include "../../Common/Boot/Bootloader.h"

#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>

#include <IndustryStandard/Acpi.h>

#include <Guid/Acpi.h>

EFI_STATUS
EfiLocateFadtFromXsdt (
    IN  EFI_ACPI_DESCRIPTION_HEADER  *Xsdt,
    OUT EFI_ACPI_COMMON_HEADER       **Fadt)
{   
    //
    //  If the XSDT pointer is NULL,
    //  how are we going to locate the FADT?
    //
    if (Xsdt == NULL)
        return EFIERR(EFI_INVALID_PARAMETER);

    EFI_ACPI_COMMON_HEADER *Table = NULL;

    UINT32  Signature    = EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE;
    UINT64  EntryPtr     = 0;
    UINTN   BasePtr      = (UINTN)(Xsdt + 1);
    UINTN   TablePtrSize = sizeof(Xsdt);
    UINTN   Entries      = (Xsdt->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / TablePtrSize;

    for (UINTN Index = 0; Index < Entries; Index++)
    {
        EntryPtr = 0;

        CopyMem(
            &EntryPtr,
            (VOID *)(BasePtr + Index * TablePtrSize),
            TablePtrSize);

        //
        //  Hopefully we found the FADT...
        //
        Table = (EFI_ACPI_COMMON_HEADER *)((UINTN)(EntryPtr));

        if (Table != NULL && Table->Signature == Signature)
        {
            //
            //  We found it!
            //
            *Fadt = Table;

            return EFI_SUCCESS;
        }
    }

    return EFI_NOT_FOUND;
}

EFI_STATUS
EfiGetTables (
    OUT EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER **Rsdp,
    OUT EFI_ACPI_DESCRIPTION_HEADER                  **Rsdt,
    OUT EFI_ACPI_DESCRIPTION_HEADER                  **Xsdt,
    OUT EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE    **Fadt,
    OUT ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE  **Dsdt)
{
    *Rsdp = NULL;
    *Rsdt = NULL;
    *Xsdt = NULL;
    *Fadt = NULL;
    *Dsdt = NULL;

    EFI_STATUS Status;

    //
    //  Attempt to locate the RSDP.
    //
    Status = EfiGetSystemConfigurationTable(
        &gEfiAcpi20TableGuid, 
        (VOID **)Rsdp);

    HANDLE_STATUS(
        Status,
        L"FAILED TO LOCATE RSDP!\r\n");

    //
    //  The RSDP could not be found.
    //
    if (*Rsdp == NULL)
        return EFI_NOT_FOUND;

    Print(L"\n===> [ACPI]: Found RSDP? = YES\n");

    //
    //  Thankfully, the RSDP has a pointer to
    //  the base address of the XSDT.
    //
    *Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(*Rsdp)->XsdtAddress;

    Print(
        L"===> [ACPI]: Found XSDT? = %s\n",
        *Xsdt == NULL
            ? L"NO"
            : L"YES");

    //
    //  Make sure the pointer is valid,
    //  and that the signature is that of the XSDT.
    //
    if (
        *Xsdt != NULL && 
        (*Xsdt)->Signature == EFI_ACPI_2_0_EXTENDED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE
    )
    {
        Status = EfiLocateFadtFromXsdt(*Xsdt, (EFI_ACPI_COMMON_HEADER **)Fadt);

        HANDLE_STATUS(
            Status,
            L"===> [ACPI]: UNKNOWN ERROR WHILE ATTEMPTING TO FIND FADT!\r\n");

        if ((*Fadt)->Header.Signature == EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE)
            Print(L"===> [ACPI]: Found FADT? = YES\n");

        else if (*Fadt != NULL)
            Print(L"===> [ACPI]: TABLE FOUND, BUT IT'S NOT FADT!\n");

        else
            Print(L"===> [ACPI]: COULD NOT FIND FADT!\n");
    }

    if (*Fadt != NULL)
    {
        //
        //  The FADT contains a pointer to the DSDT.
        //
        *Dsdt = (ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE *)(UINTN)(*Fadt)->Dsdt;

        //
        //  If we can't find the DSDT, we can't
        //  really get the proper ACPI data about
        //  the system and its hardware.
        //
        if (*Dsdt == NULL)
        {
            Print(L"===> [ACPI]: COULD NOT FIND DSDT!\n");

            return EFI_NOT_FOUND;
        }

        else
            Print(
                L"===> [ACPI]: Found DSDT? = %s\n\n",
                (*Dsdt)->Sdt.Signature == EFI_ACPI_2_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE
                    ? L"YES"
                    : L"NO");

        //
        //  We can calculate the bytecode count
        //  of the DSDT by taking the SDT's length property
        //  and subtracting it by the size of its struct.
        //
        (*Dsdt)->BytecodeCount = (*Dsdt)->Sdt.Length - sizeof((*Dsdt)->Sdt);
    }

    if (
        *Dsdt == NULL ||
        *Fadt == NULL ||
        *Xsdt == NULL ||
        *Rsdp == NULL
    )
        return EFI_NOT_FOUND;

    return EFI_SUCCESS;
}