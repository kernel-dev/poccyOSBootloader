DIRPATH=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PARPATH=$(dirname "$DIRPATH")

echo "Moving into AUDK directory... $PARPATH"

# Move to AUDK directory
cd $PARPATH

echo "Compiling base tools..."

# Source edksetup.sh to reconfigure config
. edksetup.sh

make -C BaseTools

# cd $PARPATH/OvmfPkg

# ./build.sh

cd $DIRPATH

echo "Compiling BIOS and Bootloader..."
# Compile project
cd $DIRPATH
build -a X64 -t XCODE5 -b DEBUG -p OvmfPkg/OvmfPkgX64.dsc || exit 1
build -a X64 -t XCODE5 -b DEBUG -p MdeModulePkg/MdeModulePkg.dsc || exit 1

echo "Copying BIOS..."
# Copy BIOS
cp $PARPATH/Build/OvmfX64/DEBUG_XCODE5/FV/OVMF.fd ../KernelOSPkg/bootloader/bios.bin

echo "Copying EFI application..."
# Copy EFI binary
mkdir EFI 2> /dev/null
cp $PARPATH/Build/MdeModule/DEBUG_XCODE5/X64/BootloaderPkg/BootloaderPkg/OUTPUT/KernelOSBootloader.efi EFI/KernelOSBootloader.efi
cp ./EFI/KernelOSBootloader.efi $PARPATH/KernelOSPkg/hda-contents/EFI/BOOT/BOOTX64.efi
