# TAPe UTility

Command line tool for processing TAP files.

## Usage

```
TAPUT command [options] FileIn [FileOut]
commands:
    add             adds a file at the end of the "tap" image
                    if the image does not exist, it will be created
    insert          inserts a file at the beginning of the image,
                    with the -s <n> option inserts a file before block <n>
                    if several blocks are selected, inserts the file before each block
    extract         extracts a block of data to a file
                    (requires an option -s <n>)
    remove          remove blocks of data from the image (requires -s <n>[,<n>]..)
                    up to 10 blocks can be selected
    list            list image content
options:
    -o <addr>       implies creating a block header,
                    <addr> sets the origin address
    -n <name>       implies creating a block header,
                    <name> sets a block name
    -s <n>[,<n>]..  selects the block numbers (up to 10)
    --help          display this help and exit
```