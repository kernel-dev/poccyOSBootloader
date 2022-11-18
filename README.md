# KernelOSBootloader

A minimal, PE32+ compatible UEFI bootloader which is solely used to load my own OS.

<br />

## Usage

It is solely used to load my own OS, and has not been tested to load any other OS. It will most likely not even work, as the calling conventions used are not that of a standard UEFI bootloader. A common-purpose bootloader passes down the RSDP pointer, the system memory map, and a pointer to the `RuntimeServices` instance. However, this bootloader in particular passes down the system memory map (in a custom internal representation), a pointer to the DSDT, a pointer to the `RuntimeServices` instance, and a pointer to an internal representation of the framebuffer.

<br />

## Credits

- [Mhaeuser](https://github.com/mhaeuser) — for providing this project with a rich, and high level abstraction for parsing and handling PE/COFF images (PeCoffLib2).
