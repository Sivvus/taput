# TAPe UTility

Command line tool for processing TAP files.

## Usage

```
TAPUT command [options] FileIn [FileOut]
commands:
    add           adds a file at the end of the "tap" image
    insert        inserts a file at the beginning of the image,
                  with the -b <n> option inserts a file before block <n>
    extract       extracts a block of data to a file
                  (requires an option -b <n>)
    remove        remove block of data from the image (requires -b <n>)
    list          list image content
    help          display this help and exit
options:
    -o <addr>     implies creating a block header,
                  <addr> sets the origin address
    -n <name>     implies creating a block header,
                  <name> sets a block name
    -b <number>   sets the block number
```