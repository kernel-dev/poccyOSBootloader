BDIRPATH=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
BPARPATH=$(dirname "$BDIRPATH")

echo "Moving into AUDK directory... $BPARPATH"

# Move to AUDK directory
cd $BPARPATH

echo "Compiling base tools..."

# Source edksetup.sh to reconfigure config
. edksetup.sh

make -C BaseTools

# cd $BPARPATH/OvmfPkg

# ./build.sh

cd $BDIRPATH

# Compile project
echo "Compiling BIOS and Bootloader..."
cd $BDIRPATH
build -a X64 -t XCODE5 -b DEBUG -p OvmfPkg/OvmfPkgX64.dsc || exit 1
build -a X64 -t XCODE5 -b DEBUG -p MdeModulePkg/MdeModulePkg.dsc || exit 1

# Copy BIOS
echo "Copying BIOS..."
cp $BPARPATH/Build/OvmfX64/DEBUG_XCODE5/FV/OVMF.fd ../KernelOSPkg/bootloader/bios.bin

# Copy EFI binary
echo "Copying EFI application..."
mkdir EFI 2>/dev/null
cp $BPARPATH/Build/MdeModule/DEBUG_XCODE5/X64/BootloaderPkg/BootloaderPkg/OUTPUT/KernelOSBootloader.efi EFI/KernelOSBootloader.efi
cp ./EFI/KernelOSBootloader.efi $BPARPATH/KernelOSPkg/hda-contents/EFI/BOOT/BOOTX64.efi
