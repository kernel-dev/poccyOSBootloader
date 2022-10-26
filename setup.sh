# Move to EDK2 directory
cd /Users/kernel/Documents/edk2-oofer/edk2/

# Source edksetup.sh to reconfigure config
. edksetup.sh

make -C BaseTools

# Compile project
cd BootloaderPkg
build -a X64 -t XCODE5 -b DEBUG -p OvmfPkg/OvmfPkgX64.dsc -D SOURCE_DEBUG_ENABLE=TRUE -D DEBUG_ON_SERIAL_PORT || exit 1
build -a X64 -t XCODE5 -b DEBUG -p MdeModulePkg/MdeModulePkg.dsc || exit 1

# Copy BIOS
cp /Users/kernel/Documents/edk2-master/edk2/Build/OvmfX64/RELEASE_XCODE5/FV/OVMF.fd ../KernelOSPkg/bootloader/bios.bin

# Copy EFI binary
mkdir EFI 2> /dev/null
cp /Users/kernel/Documents/edk2-oofer/edk2/Build/MdeModule/DEBUG_XCODE5/X64/BootloaderPkg/BootloaderPkg/OUTPUT/KernelOSBootloader.efi EFI/KernelOSBootloader.efi
cp ./EFI/KernelOSBootloader.efi /Users/kernel/Documents/edk2-oofer/edk2/KernelOSPkg/hda-contents/EFI/BOOT/BOOTX64.efi