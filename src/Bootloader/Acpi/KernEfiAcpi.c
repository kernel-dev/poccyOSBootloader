#include "../../Common/Acpi/KernEfiAcpi.h"
#include "../../Common/Boot/Bootloader.h"

#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>

#include <IndustryStandard/Acpi.h>

#include <Guid/Acpi.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
EFI_STATUS
EfiLocateFadtFromXsdtOrRsdt (
  IN  EFI_ACPI_DESCRIPTION_HEADER  *Sdt,
  OUT EFI_ACPI_COMMON_HEADER       **Fadt
  )
{
  //
  //  If the XSDT/RSDT pointer is NULL,
  //  how are we going to locate the FADT?
  //
  if (Sdt == NULL) {
    return EFIERR (EFI_INVALID_PARAMETER);
  }

  EFI_ACPI_COMMON_HEADER  *Table = NULL;

  UINT32  Signature    = SIGNATURE_32 ('F', 'A', 'C', 'P');
  UINT64  EntryPtr     = 0;
  UINTN   BasePtr      = (UINTN)(Sdt + 1);
  UINTN   TablePtrSize = sizeof (Sdt);
  UINTN   Entries      = (Sdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / TablePtrSize;

  for (UINTN Index = 0; Index < Entries; Index++) {
    EntryPtr = 0;

    CopyMem (
      &EntryPtr,
      (VOID *)(BasePtr + Index * TablePtrSize),
      TablePtrSize
      );

    //
    //  Hopefully we found the FADT...
    //
    Table = (EFI_ACPI_COMMON_HEADER *)((UINTN)(EntryPtr));

    if ((Table != NULL) && (Table->Signature == Signature)) {
      //
      //  We found it!
      //
      *Fadt = Table;

      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}
#pragma GCC diagnostic pop

EFI_STATUS
EfiGetTables (
  OUT EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  **Rsdp,
  OUT EFI_ACPI_DESCRIPTION_HEADER                   **Rsdt,
  OUT EFI_ACPI_DESCRIPTION_HEADER                   **Xsdt,
  OUT EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE     **Fadt,
  OUT ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE   **Dsdt
  )
{
  EFI_STATUS  Status;

  //
  //  Attempt to locate the RSDP.
  //
  Status = EfiGetSystemConfigurationTable (
             &gEfiAcpi20TableGuid,
             (VOID **)Rsdp
             );

  HANDLE_STATUS (
    Status,
    L"FAILED TO LOCATE RSDP!\r\n"
    );

  //
  //  The RSDP could not be found.
  //
  if (*Rsdp == NULL) {
    Print (L"Rsdp hasn't been found!\r\n");

    return EFI_NOT_FOUND;
  }

  Print (L"\n===> [ACPI]: Found RSDP? = YES\n");

  *Rsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)(*Rsdp)->RsdtAddress;

  //
  //  Thankfully, the RSDP has a pointer to
  //  the base address of the XSDT.
  //
  *Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(*Rsdp)->XsdtAddress;

  Print (
    L"===> [ACPI]: Found RSDT? = %s\n",
    *Rsdt == NULL
            ? L"NO"
            : L"YES"
    );

  Print (
    L"===> [ACPI]: Found XSDT? = %s\n",
    *Xsdt == NULL
            ? L"NO"
            : L"YES"
    );

  if ((*Rsdt == NULL) && (*Xsdt == NULL)) {
    Print (L"FAILED TO LOCATE RSDT _AND_ XSDT!\r\n");

    return EFI_NOT_FOUND;
  }

  //
  //  Make sure the pointer is valid,
  //  and that the signature is that of the XSDT.
  //
  if (
      (
       (*Rsdt != NULL) &&
       ((*Rsdt)->Signature == EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_TABLE_SIGNATURE)
      ) ||
      (
       (*Xsdt != NULL) &&
       ((*Xsdt)->Signature == EFI_ACPI_2_0_EXTENDED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE)
      )
      )
  {
    Status = EfiLocateFadtFromXsdtOrRsdt (*Xsdt, (EFI_ACPI_COMMON_HEADER **)Fadt);

    HANDLE_STATUS (
      Status,
      L"===> [ACPI]: UNKNOWN ERROR WHILE ATTEMPTING TO FIND FADT!\r\n"
      );

    if ((*Fadt)->Header.Signature == EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE) {
      Print (L"===> [ACPI]: Found FADT? = YES\n");
    } else if (*Fadt != NULL) {
      Print (L"===> [ACPI]: TABLE FOUND, BUT IT'S NOT FADT!\n");
    } else {
      Print (L"===> [ACPI]: COULD NOT FIND FADT!\n");
    }
  }

  if (*Fadt != NULL) {
    //
    //  The FADT contains a pointer to the DSDT.
    //
    *Dsdt = (ACPI_DIFFERENTIATED_SYSTEM_DESCRIPTOR_TABLE *)(UINTN)(*Fadt)->Dsdt;

    //
    //  If we can't find the DSDT, we can't
    //  really get the proper ACPI data about
    //  the system and its hardware.
    //
    if (*Dsdt == NULL) {
      Print (L"===> [ACPI]: COULD NOT FIND DSDT!\n");

      return EFI_NOT_FOUND;
    } else {
      Print (
        L"===> [ACPI]: Found DSDT? = %s\n\n",
        (*Dsdt)->Sdt.Signature == EFI_ACPI_2_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE
                    ? L"YES"
                    : L"NO"
        );
    }

    //
    //  We can calculate the bytecode count
    //  of the DSDT by taking the SDT's length property
    //  and subtracting it by the size of its struct.
    //
    (*Dsdt)->BytecodeCount = (*Dsdt)->Sdt.Length - sizeof ((*Dsdt)->Sdt);
  }

  if (
      (*Dsdt == NULL) ||
      (*Fadt == NULL) ||
      (*Rsdp == NULL)
      )
  {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}
