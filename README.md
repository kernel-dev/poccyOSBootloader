# KernelOSBootloader

A minimal, PE32+ compatible UEFI bootloader which is solely used to load my own OS.

<br />

## Usage

It is solely used to load my own OS, and has not been tested to load any other OS. It will most likely not even work, as the calling conventions used are not that of a standard UEFI bootloader. A common-purpose bootloader passes down the RSDP pointer, the system memory map, and a pointer to the `RuntimeServices` instance. However, this bootloader in particular passes down the system memory map (in a custom internal representation), a pointer to the DSDT, a pointer to the `RuntimeServices` instance, and a pointer to an internal representation of the framebuffer.

__Note__: if you want to build this project yourself, please clone the AUDK repository (listed as a submodule in this repository), and add this project to the AUDK directory. Rename it to `BootloaderPkg`. Then add `BootloaderPkg/BootloaderPkg.inf` under `Components` for both `OvmfPkg/OvmfPkgX64.dsc` and `MdeModulePkg/MdeModulePkg.dsc`. Then follow the rest of the steps for setting up EDK2 (`source edksetup.sh` and `make -C BaseTools`). Finally, just run `build.sh` with updated paths.

<br />

## Credits

- [Mhaeuser](https://github.com/mhaeuser) — for providing this project with a rich, and high level abstraction for parsing and handling PE/COFF images (PeCoffLib2).
- [Acidanthera](https://github.com/acidanthera) — for their fork of EDK2 ([AUDK](https://github.com/acidanthera/audk))
