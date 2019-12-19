# TAPe UTility

Command line tool for processing TAP files.

## Usage

```
TAPUT command [options] FileIn [FileOut]
    taput add -o 32768 -n BlockName file.bin image.tap
    taput insert -s 3 -o 32768 -n BlockName file.bin image.tap
    taput extract -s 2 image.tap file.bin
    taput remove -s 1-4,8,12 image.tap
    taput list image.tap
Commands:
    add              Adds a file at the end of the "tap" image
                     if the image does not exist, it will be created
    insert           Inserts a file at the beginning of the image,
                     with the -s <n> option inserts a file before block <n>
    extract          Extracts a block of data to a file
                     (requires an option -s <n>)
    remove           Remove blocks of data from the image (requires -s (<n>|<n>-<m>))
                     up to 10 blocks can be selected
    list             List image content
Options:
    -b               Creates a BASIC program header block
    -o <addr>        Implies creating a block header,
                     <addr> sets the origin address or the BASIC autostart line number
    -n <name>        Implies creating a block header,
                     <name> sets a block name
    -s (<n>|<n>-<m>)[,(<n>|<n>-<m>)]..
                     Selects the block numbers and/or block ranges (up to 10)
    -h, --help       Display this help and exit
```
