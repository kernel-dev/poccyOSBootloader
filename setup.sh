# Move to EDK2 directory
cd /Users/kernel/Documents/edk2-master/edk2/

# Source edksetup.sh to reconfigure config
. edksetup.sh

make -C BaseTools

# Compile project
cd BootloaderPkg
build -a X64 -t XCODE5 -b RELEASE -p OvmfPkg/OvmfPkgX64.dsc || exit 1
build -a X64 -t XCODE5 -b RELEASE -p MdeModulePkg/MdeModulePkg.dsc || exit 1

# Copy BIOS
cp ../Build/OvmfX64/RELEASE_XCODE5/FV/OVMF.fd ../KernelOSPkg/bootloader/bios.bin

# Copy EFI binary
mkdir EFI 2> /dev/null
cp ../Build/MdeModule/RELEASE_XCODE5/X64/BootloaderPkg/BootloaderPkg/OUTPUT/KernelOSBootloader.efi EFI/KernelOSBootloader.efi
cp ./EFI/KernelOSBootloader.efi /Users/kernel/Documents/edk2-master/edk2/KernelOSPkg/hda-contents/EFI/BOOT/BOOTX64.efi