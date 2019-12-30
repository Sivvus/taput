# TAPe UTility

Command line tool for processing TAP files.

## Usage

```
TAPUT command [options] FileIn [FileOut]
Commands:
    add              Add a file at the end of the "tap" image
                     if the image does not exist, it will be created
    extract          Extract a block of data to a file
    fix-0            Remove empty blocks from the image
    fix-crc          Correct the checksum of the selected blocks
    insert           Insert a file at the beginning of the image,
                     with the -s <n> option inserts a file before block n
    list             List image content
    remove           Remove blocks of data from the image (requires -s (<n>|<n>-<m>))
                     up to 10 blocks can be selected
    replace          Replace the data block with the contents of the file
Options:
    -b               Creates a BASIC program header block
    -h, --help       Display this help and exit
    -n <name>        Implies creating a block header,
                     <name> sets a block name
    -o <addr>        Implies creating a block header,
                     <addr> sets the origin address or the BASIC autostart line number
    -r, --raw        Treats input/output files as raw data,
                     refrains from creating a flag and checksum
    -s (<n>|<n>-<m>)[,(<n>|<n>-<m>)]..
                     Selects the block numbers and/or block ranges (up to 10)
Examples:
    taput add -o 32768 -n BlockName file.bin image.tap
    taput extract -s 2 image.tap file.bin
    taput fix-0 image.tap
    taput fix-crc -s 3 image.tap
    taput insert -s 3 -o 32768 -n BlockName file.bin image.tap
    taput list image.tap
    taput remove -s 1-4,8,12 image.tap
    taput replace -s 2 file.bin image.tap
```
