# XC001 Bootloader / Application build layout

XC001 now follows the same operating model as Chaos:

- `make bootloader` builds the permanent bootloader HEX.
- `make app` or `make dfu` builds the normal application HEX and BIN.
- First-time programming burns bootloader HEX + application HEX.
- Web firmware upgrade uploads the application `.bin` only.

## Build commands

From the XC001 project root:

```powershell
$env:PATH = "<STM32 GNU toolchain bin>;$env:PATH"
make bootloader
make app
```

Chaos-compatible aliases:

```powershell
make dfu        # same as make app
make factory    # build bootloader + app + one-shot factory HEX
make all        # build bootloader + app
```

## Output files

Bootloader build:

- `Bootloader/build/XC001_Bootloader.hex`
- `Bootloader/build/XC001_Bootloader_<version>.hex`
- Internally this is Stage0 + Recovery merged into one HEX:
  - Stage0 at `0x08000000`
  - Recovery at `0x081E0000`

Application build:

- `Debug/XC001.hex`
- `Debug/XC001.bin`
- `Debug/XC001_<version>.hex`
- `Debug/XC001_<version>.bin`
- Application is linked at `0x08020000`.

Factory build:

- `Bootloader/build/XC001_Factory.hex`
- `Bootloader/build/XC001_Factory_<version>.hex`

The factory HEX contains bootloader + application + recovery + applied metadata. It is convenient for production, but the Chaos-style two-file first programming flow is still supported and recommended for clarity.

## First-time programming of a new board

Use STM32CubeProgrammer or your production programmer to program both HEX files:

1. Program `Bootloader/build/XC001_Bootloader_<version>.hex`.
2. Program `Debug/XC001_<version>.hex`.
3. Reset the board.

Do not use full-chip erase after the bootloader has been installed unless you intend to reprogram both files again.

## Web upgrade

The web page and Chaos-compatible `/firmware_upload` endpoint accept the application BIN only:

- Upload `Debug/XC001_<version>.bin`.
- Do not upload bootloader HEX.
- Do not upload factory HEX.

The web upgrade writes the application to the staging area, verifies the vector table and CRC, records it as pending, and then reboots to install it.