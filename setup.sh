# Move to EDK2 directory
cd /Users/user/Documents/audk/

# Source edksetup.sh to reconfigure config
. edksetup.sh

make -C BaseTools

# Compile project
cd BootloaderPkg
build -a X64 -t XCODE5 -b RELEASE -p OvmfPkg/OvmfPkgX64.dsc || exit 1
build -a X64 -t XCODE5 -b RELEASE -p MdeModulePkg/MdeModulePkg.dsc || exit 1

# Copy BIOS
cp /Users/user/Documents/audk/Build/OvmfX64/RELEASE_XCODE5/FV/OVMF.fd ../KernelOSPkg/bootloader/bios.bin

# Copy EFI binary
mkdir EFI 2> /dev/null
cp /Users/user/Documents/audk/Build/MdeModule/RELEASE_XCODE5/X64/BootloaderPkg/BootloaderPkg/OUTPUT/KernelOSBootloader.efi EFI/KernelOSBootloader.efi
cp ./EFI/KernelOSBootloader.efi /Users/user/Documents/audk/KernelOSPkg/hda-contents/EFI/BOOT/BOOTX64.efi
