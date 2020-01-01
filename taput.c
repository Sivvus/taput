#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_SELECTED_BLOCKS 10
#define MAX_PATH 260
#define MAX_FILE_SIZE 0xffff - 2
#define APP_VERSION "1.06"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define MAX_SELECTED_BLOCKS_S STR(MAX_SELECTED_BLOCKS)

#define DT_BASIC     0
#define DT_NUMARRAY  1
#define DT_CHARARRAY 2
#define DT_CODE      3

typedef unsigned char byte;

//~~~~ Global variables ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

enum command
{
    cm_Unknown,
    cm_Add,
    cm_Insert,
    cm_Extract,
    cm_List,
    cm_Remove,
    cm_Replace,
    cm_Fix0,
    cm_FixCrc
} Command
    = cm_Unknown;

struct tapeheader
{
    byte LenLo1;
    byte LenHi1;
    byte Flag1;
    byte HType;
    char HName[10];
    byte HLenLo;
    byte HLenHi;
    byte HStartLo;
    byte HStartHi;
    byte HParam2Lo;
    byte HParam2Hi;
    byte Parity1;
} TapeHeader = {
        19, 0,                                        // len
        0,                                            // flag
        DT_CODE,                                      // datatype
        { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 },   // name
        0, 0,                                         // length of data block
        0x00, 0x00,                                   // param1 : origin
        0x00, 0x80,                                   // param2
        0,                                            // checksum
    };

struct selectedblocks
{
    int count;
    struct
    {
        int start;
        int end;
    } block[MAX_SELECTED_BLOCKS];
} SelectedBlocks;

bool AddTapeHeader = false;
bool RawData = false;
bool QuietMode = false;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void showUsage(void)
{
    puts(
    "TAPe UTility " APP_VERSION " by Sivvus\n"
    "Usage: TAPUT command [options] FileIn [FileOut]\n"
    "Commands:\n"
    "    add              Add a file at the end of the \"tap\" image\n"
    "                     if the image does not exist, it will be created\n"
    "    extract          Extract a block of data to a file\n"
    "    fix-0            Remove empty blocks from the image\n"
    "    fix-crc          Correct the checksum of the selected blocks\n"
    "    insert           Insert a file at the beginning of the image,\n"
    "                     with the -s <n> option inserts a file before block n\n"
    "    list             List image content\n"
    "    remove           Remove blocks of data from the image (requires -s (<n>|<n>-<m>))\n"
    "                     up to " MAX_SELECTED_BLOCKS_S " blocks can be selected\n"
    "    replace          Replace the data block with the contents of the file\n"
    "Options:\n"
    "    -b               Creates a BASIC program header block\n"
    "    -h, --help       Display this help and exit\n"
    "    -n <name>        Implies creating a block header,\n"
    "                     <name> sets a block name\n"
    "    -o <addr>        Implies creating a block header,\n"
    "                     <addr> sets the origin address or the BASIC autostart line number\n"
    "    -r, --raw        Treats input/output files as raw data,\n"
    "                     refrains from creating a flag and checksum\n"
    "    -s (<n>|<n>-<m>)[,(<n>|<n>-<m>)]..\n"
    "                     Selects the block numbers and/or block ranges (up to " MAX_SELECTED_BLOCKS_S ")\n"
    "Examples:\n"
    "    taput add -o 32768 -n \"Block name\" file.bin image.tap\n"
    "    taput extract -s 2 image.tap file.bin\n"
    "    taput fix-0 image.tap\n"
    "    taput fix-crc -s 3 image.tap\n"
    "    taput insert -s 3 -o 32768 -n \"Block name\" file.bin image.tap\n"
    "    taput list image.tap\n"
    "    taput remove -s 1-4,8,12 image.tap\n"
    "    taput replace -s 2 file.bin image.tap"
    );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

byte crc(byte *start, int length)
{
    byte tcrc = 0;
    for (int i = 0; i < length; i++)
        tcrc = tcrc ^ start[i];
    return tcrc;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void PrepareTapeHeader(int size)
{
    switch (TapeHeader.HType)
    {
        case DT_BASIC:
            TapeHeader.HParam2Lo = (byte)(size & 0xff);
            TapeHeader.HParam2Hi = (byte)(size >> 8);
            break;
        case DT_CODE:
            TapeHeader.HParam2Lo = 0x00;
            TapeHeader.HParam2Hi = 0x80;
            break;
    }
    if (RawData)
        size = size - 2;
    if (size < 0) size = 0;
    TapeHeader.HLenLo = (byte)(size & 0xff);
    TapeHeader.HLenHi = (byte)(size >> 8);
    TapeHeader.Parity1 = crc(&TapeHeader.Flag1, 18);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void ChkOutFile(char *FileName)
{
    if (FileName[0] == '\0')
    {
        fprintf(stderr, "No output file name was given\n");
        exit(1);
    }
}

//~~~~ LoadFile ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// load file to memory 
// return pionter and size - size of file
// if fail return NULL

byte *LoadFile(size_t *size, const char *FileName)
{
    FILE *file;
    file = fopen(FileName, "rb");
    if (!file)
    {
        *size = 0;
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    byte *Dest = (byte*)malloc(*size + 1);
    if (!Dest)
    {
        fprintf(stderr, "Memory allocation error\n");
        fclose(file);
        exit(1);
    }
    if (*size)
        if (!fread(Dest, *size, 1, file))
        {
            *size = 0;
            free(Dest);
            Dest = NULL;
        }
    fclose(file);

    return Dest;
}

//~~~~ isSelected ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// check if given block numbrer is selected

bool isSelected(int BlockNo)
{
    for (int i = 0; i < SelectedBlocks.count; i++)
        if (
                SelectedBlocks.block[i].start <= SelectedBlocks.block[i].end &&
                BlockNo >= SelectedBlocks.block[i].start &&
                BlockNo <= SelectedBlocks.block[i].end
            )
            return true;
    return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void cmdAdd(char *FileNameIn, char *FileNameOut)
{
    size_t sizeIn;
    byte *bufferIn = LoadFile(&sizeIn, FileNameIn);
    if (!bufferIn)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameIn);
        exit(1);
    }
    if (sizeIn > MAX_FILE_SIZE)
    {
        fprintf(stderr, "The file \"%s\" is too large\n", FileNameIn);
        free(bufferIn);
        exit(1);
    }

    size_t sizeOut;
    byte *bufferOut = LoadFile(&sizeOut, FileNameOut);
    byte *endbuf = bufferOut + sizeOut;
    byte *pos = bufferOut;

    FILE *file;
    file = fopen(FileNameOut, "wb");
    if (!file)
    {
        fprintf(stderr, "Unable to create file \"%s\"\n", FileNameOut);
        free(bufferOut);
        free(bufferIn);
        exit(1);
    }

    // re-write old file
    if (bufferOut)
    {
        for (int i = 1; pos < endbuf; i++)
        {
            size_t blocksize = pos[0] | (pos[1] << 8);

            if ((pos + blocksize + 1) < endbuf)
                fwrite(pos, blocksize + 2, 1, file);
            else
           {
                fwrite(pos, endbuf - pos, 1, file);
                fclose(file);
                free(bufferOut);
                free(bufferIn);
                fprintf(stderr, "Warning: image \"%s\" is corrupted", FileNameOut);
                fprintf(stderr, ", saving data cannot be done\n");
                exit(1);
            }

            pos += blocksize + 2;
        }
        free(bufferOut);
    }

    // add new part
    if (AddTapeHeader)
    {
        PrepareTapeHeader(sizeIn);
        fwrite(&TapeHeader, sizeof(TapeHeader), 1, file);
    }
    byte Head[3];
    if (RawData)
    {
        Head[0] = sizeIn & 0xff;
        Head[1] = sizeIn >> 8;
        fwrite(Head, 2, 1, file);
        fwrite(bufferIn, sizeIn, 1, file);
    }
    else
    {
        Head[0] = (sizeIn + 2) & 0xff;
        Head[1] = (sizeIn + 2) >> 8;
        Head[2] = 0xff;
        fwrite(Head, sizeof(Head), 1, file);
        fwrite(bufferIn, sizeIn, 1, file);
        byte Parity = crc(bufferIn, sizeIn) ^ Head[2];
        fwrite(&Parity, sizeof(Parity), 1, file);
    }
    free(bufferIn);
    fclose(file);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void cmdInsert(char *FileNameIn, char *FileNameOut)
{
    if (SelectedBlocks.count == 0)
    {
        SelectedBlocks.count = 1;
        SelectedBlocks.block[0].start = 1;
        SelectedBlocks.block[0].end = 1;
    }
    if (SelectedBlocks.count > 1 || SelectedBlocks.block[0].start != SelectedBlocks.block[0].end)
    {
        fprintf(stderr, "Exactly one block should be selected\n");
        exit(1);
    }
    bool isDone = false;
    
    // Load and check input file
    size_t sizeIn;
    byte *bufferIn = LoadFile(&sizeIn, FileNameIn);
    if (!bufferIn)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameIn);
        exit(1);
    }
    if (sizeIn > MAX_FILE_SIZE)
    {
        fprintf(stderr, "The file \"%s\" is too large\n", FileNameIn);
        free(bufferIn);
        exit(1);
    }

    // Load output file
    size_t sizeOut;
    byte *bufferOut = LoadFile(&sizeOut, FileNameOut);
    if (!bufferOut)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameOut);
        free(bufferIn);
        exit(1);
    }

    byte *endbuf = bufferOut + sizeOut;
    byte *pos = bufferOut;
    FILE *file;
    file = fopen(FileNameOut, "wb");
    if (!file)
    {
        fprintf(stderr, "Unable to create file \"%s\"\n", FileNameOut);
        free(bufferOut);
        free(bufferIn);
        exit(1);
    }
    for (int i = 1; pos < endbuf; i++)
    {
        if (isSelected(i))
        {
            if (AddTapeHeader)
            {
                PrepareTapeHeader(sizeIn);
                fwrite(&TapeHeader, sizeof(TapeHeader), 1, file);
            }
            byte Head[3];
            if (RawData)
            {
                Head[0] = sizeIn & 0xff;
                Head[1] = sizeIn >> 8;
                fwrite(Head, 2, 1, file);
                fwrite(bufferIn, sizeIn, 1, file);
            } else {
                Head[0] = (sizeIn + 2) & 0xff;
                Head[1] = (sizeIn + 2) >> 8;
                Head[2] = 0xff;
                fwrite(Head, sizeof(Head), 1, file);
                fwrite(bufferIn, sizeIn, 1, file);
                byte Parity = crc(bufferIn, sizeIn) ^ Head[2];
                fwrite(&Parity, sizeof(Parity), 1, file);
            }
            isDone = true;
        }
        size_t blocksize = pos[0] | (pos[1] << 8);
        if ((pos + blocksize + 1) < endbuf)
            fwrite(pos, blocksize + 2, 1, file);
        else
            fwrite(pos, endbuf - pos, 1, file);
        pos += blocksize + 2;
    }
    fclose(file);
    free(bufferOut);
    free(bufferIn);

    if (!isDone)
    {
        fprintf(stderr, "Selected block was not found\n");
        exit(1);
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void cmdReplace(char *FileNameIn, char *FileNameOut)
{
    if (SelectedBlocks.count != 1)
    {
        fprintf(stderr, "Exactly one block or range of blocks should be selected\n");
        exit(1);
    }

    bool isDone = false;

    size_t sizeIn;
    byte *bufferIn = LoadFile(&sizeIn, FileNameIn);
    if (!bufferIn)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameIn);
        exit(1);
    }
    if (sizeIn > MAX_FILE_SIZE)
    {
        fprintf(stderr, "The file \"%s\" is too large\n", FileNameIn);
        free(bufferIn);
        exit(1);
    }

    size_t sizeOut;
    byte *bufferOut = LoadFile(&sizeOut, FileNameOut);
    if (!bufferOut)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameOut);
        free(bufferIn);
        exit(1);
    }

    byte *endbuf = bufferOut + sizeOut;
    byte *pos = bufferOut;
    FILE *file;
    file = fopen(FileNameOut, "wb");
    if (!file)
    {
        fprintf(stderr, "Unable to create file \"%s\"\n", FileNameOut);
        free(bufferOut);
        free(bufferIn);
        exit(1);
    }
    for (int i = 1; pos < endbuf; i++)
    {
        size_t blocksize = pos[0] | (pos[1] << 8);
        if (isSelected(i))
        {
            if (!isDone)
            {
                if (AddTapeHeader)
                {
                    PrepareTapeHeader(sizeIn);
                    fwrite(&TapeHeader, sizeof(TapeHeader), 1, file);
                }
                byte Head[3];
                if (RawData)
                {
                    Head[0] = sizeIn & 0xff;
                    Head[1] = sizeIn >> 8;
                    fwrite(Head, 2, 1, file);
                    fwrite(bufferIn, sizeIn, 1, file);
                } else {
                    Head[0] = (sizeIn + 2) & 0xff;
                    Head[1] = (sizeIn + 2) >> 8;
                    Head[2] = 0xff;
                    fwrite(Head, sizeof(Head), 1, file);
                    fwrite(bufferIn, sizeIn, 1, file);
                    byte Parity = crc(bufferIn, sizeIn) ^ Head[2];
                    fwrite(&Parity, sizeof(Parity), 1, file);
                }
                isDone = true;
            }
        }
        else
        {
            if ((pos + blocksize + 1) < endbuf)
                fwrite(pos, blocksize + 2, 1, file);
            else
                fwrite(pos, endbuf - pos, 1, file);
        }
        pos += blocksize + 2;
    }

    fclose(file);
    free(bufferOut);
    free(bufferIn);

    if (!isDone)
    {
        fprintf(stderr, "Selected blocks were not found\n");
        exit(1);
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void cmdExtract(char *FileNameIn, char *FileNameOut)
{
    if (SelectedBlocks.count != 1 || SelectedBlocks.block[0].start != SelectedBlocks.block[0].end)
    {
        fprintf(stderr, "Exactly one block should be selected\n");
        exit(1);
    }

    bool isDone = false;
    bool isComplete = true;

    size_t size;
    byte *buffer = LoadFile(&size, FileNameIn);
    if (!buffer)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameIn);
        exit(1);
    }
    byte *endbuf = buffer + size;
    byte *pos = buffer;
    for (int i = 1; pos < endbuf; i++)
    {
        size_t blocksize = pos[0] | (pos[1] << 8);
        if (isSelected(i))
        {
            FILE *file;
            file = fopen(FileNameOut, "wb");
            if (!file)
            {
                fprintf(stderr, "Unable to create file \"%s\"\n", FileNameOut);
                free(buffer);
                exit(1);
            }
            if (RawData)
            {
                int len = endbuf - pos - 2;
                if (len >= (int)blocksize)
                    len = blocksize;
                else
                    isComplete = false;
                fwrite(pos + 2, len, 1 , file);
            } else {
                int len = endbuf - pos - 3;
                if (len >= (int)(blocksize - 2))
                    len = blocksize - 2;
                else
                    isComplete = false;
                fwrite(pos + 3, len, 1 , file);
            }
            fclose(file);
            isDone = true;
        }
        pos += blocksize + 2;
    }
    free(buffer);

    if (!isDone)
    {
        fprintf(stderr, "Selected block was not found\n");
        exit(1);
    }

    if (!isComplete)
    {
        fprintf(stderr, "Warning: image \"%s\" is corrupted", FileNameIn);
        fprintf(stderr, ", file \"%s\" has not been saved completely.\n",
            FileNameOut);
        exit(1);
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void cmdRemove(char *FileName)
{
    if (SelectedBlocks.count == 0)
    {
        fprintf(stderr, "No block selected\n");
        exit(1);
    }

    bool isDone = false;

    size_t size;
    byte *buffer = LoadFile(&size, FileName);
    if (!buffer)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileName);
        exit(1);
    }
    byte *endbuf = buffer + size;
    byte *pos = buffer;
    FILE *file;
    file = fopen(FileName, "wb");
    if (!file)
    {
        fprintf(stderr, "Unable to create file \"%s\"\n", FileName);
        free(buffer);
        exit(1);
    }
    for (int i = 1; pos < endbuf; i++)
    {
        size_t blocksize = pos[0] | (pos[1] << 8);
        if (!isSelected(i))
        {
            if ((pos + blocksize + 1) < endbuf)
                fwrite(pos, blocksize + 2, 1, file);
            else
                fwrite(pos, endbuf - pos, 1, file);
        }
        else isDone = true;
        pos += blocksize + 2;
    }
    fclose(file);
    free(buffer);

    if (!isDone)
    {
        fprintf(stderr, "Selected blocks were not found\n");
        exit(1);
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void cmdFix0(char *FileName)
{
    bool isDone = false;
    size_t size;
    byte *buffer = LoadFile(&size, FileName);
    if (!buffer)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileName);
        exit(1);
    }
    byte *endbuf = buffer + size;
    byte *pos = buffer;
    FILE *file;
    file = fopen(FileName, "wb");
    if (!file)
    {
        fprintf(stderr, "Unable to create file \"%s\"\n", FileName);
        free(buffer);
        exit(1);
    }
    for (int i = 1; pos < endbuf; i++)
    {
        size_t blocksize = pos[0] | (pos[1] << 8);
        if (blocksize)
        {

            if ((pos + blocksize + 1) < endbuf)
                fwrite(pos, blocksize + 2, 1, file);
            else {
                fwrite(pos, endbuf - pos, 1, file);
                byte fill = 0;
                for (int cnt = (blocksize + 2)-(endbuf - pos); cnt > 0; cnt--)
                    fwrite(&fill, 1, 1, file);
                isDone = true;
            }
        }
        else
            isDone = true;
        pos += blocksize + 2;
    }
    fclose(file);
    free(buffer);

    if (!isDone)
    {
        fprintf(stderr, "No zero size blocks found\n");
        exit(1);
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void cmdFixCrc(char *FileName)
{
    if (SelectedBlocks.count != 1)
    {
        fprintf(stderr, "No block selected\n");
        exit(1);
    }

    bool isDone = false;
    size_t size;
    byte *buffer = LoadFile(&size, FileName);
    if (!buffer)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileName);
        exit(1);
    }
    byte *endbuf = buffer + size;
    byte *pos = buffer;
    FILE *file;
    file = fopen(FileName, "wb");
    if (!file)
    {
        fprintf(stderr, "Unable to create file \"%s\"\n", FileName);
        free(buffer);
        exit(1);
    }
    for (int i = 1; pos < endbuf; i++)
    {
        size_t blocksize = pos[0] | (pos[1] << 8);
            if ((pos + blocksize + 1) < endbuf)
            {
                if (isSelected(i) && blocksize > 1)
                {
                    pos[blocksize + 1] = crc(&pos[2], blocksize - 1);
                    isDone = true;
                }
                fwrite(pos, blocksize + 2, 1, file);
            } else 
                fwrite(pos, endbuf - pos, 1, file);
            
        pos += blocksize + 2;
    }
    fclose(file);
    free(buffer);

    if (!isDone)
    {
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

void cmdList(char *FileNameIn)
{
    size_t size;
    byte *buffer = LoadFile(&size, FileNameIn);
    if (buffer == NULL)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameIn);
        exit(1);
    }
    byte *endbuf = buffer + size;
    byte *pos = buffer;
    for (int i = 1; pos < endbuf; i++)
    {
        size_t blocksize = pos[0] | (pos[1] << 8);
        int datasize = blocksize - 2;
        if (datasize < 0)
            datasize = 0;
        // check crc
        char crcStr[8];
        int crcCalc;
        if (blocksize > 1 && (pos + blocksize + 1) < endbuf)
        {
            crcCalc = crc(&pos[2], datasize + 1);
            if (pos[blocksize + 1] == crcCalc)
                sprintf(crcStr, "<%.2X>",crcCalc);
            else
                sprintf(crcStr, "<%.2X:%.2X>", (int)pos[blocksize + 1], crcCalc);
        } else
            strcpy(crcStr,"<-->");
        // blocktype
        char typeStr[7];
        if (blocksize && &pos[2] < endbuf)
        {
            switch (pos[2])
            {
                case 0x00:
                    if (blocksize == 19)
                        strcpy(typeStr, "<HEAD>");
                    else
                        sprintf(typeStr, "<0x%.2X>", (int)pos[2]);
                    break;
                case 0xff:
                    strcpy(typeStr, "<DATA>");
                    break;
                default:
                    sprintf(typeStr, "<0x%.2X>", (int)pos[2]);
                    break;
            };
        } else
            strcpy(typeStr, "<---->");

        printf("#%d\t%s\t%5d\t%s\t%.6X\t", i, typeStr, datasize, crcStr,
            (unsigned int)(pos - buffer));

        if (!blocksize)
        {
            puts("empty block");
            pos += 2;
            continue;
        }
        else if (pos[2] == 0x00 && blocksize == 19)
        {
            struct tapeheader *Header = (struct tapeheader*)pos;
            char blockname[11];
            for (int j = 0; j < 10; j++)
                if (isprint(Header->HName[j]))
                    blockname[j] = Header->HName[j];
                else
                    blockname[j] = '?';
            blockname[10] = '\0';
            int startadr = Header->HStartLo | (Header->HStartHi << 8);
            int length = Header->HLenLo | (Header->HLenHi << 8);
            int basic = Header->HParam2Lo | (Header->HParam2Hi << 8);
            int variab = length - basic;
            switch (Header->HType)
            {
                case DT_BASIC:
                    printf("Program: %s\t", blockname);
                    if (startadr != 0x8000)
                        printf("LINE %d,", startadr);
                    printf("%d", length);
                    if (variab > 0)
                        printf("(B:%d,V:%d)\n", basic, variab);
                    else
                        printf("\n");
                    break;
                case DT_NUMARRAY:
                    printf("Number array: %s\n", blockname);
                    break;
                case DT_CHARARRAY:
                    printf("Character array: %s\n", blockname);
                    break;
                case DT_CODE:
                    printf("Bytes:   %s\tCODE %d,%d\n", blockname, startadr, length);
                    break;
                default:
                    printf("Unknown data type [0x%.2X]\n", Header->HType);
                    break;
            }
        }
        else
            puts("");
        pos += blocksize + 2;
    }
    if (pos != endbuf)
    {
        fprintf(stderr, "Warning: image \"%s\" is corrupted", FileNameIn);
        printf(", missing bytes: %d\n", (int)(pos - endbuf));
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

int main(int argc, char **argv)
{
    char FileNameIn[MAX_PATH + 1] = "\0";
    char FileNameOut[MAX_PATH + 1] = "\0";
    bool AllOk = true;
    memset(&SelectedBlocks, 0, sizeof(SelectedBlocks));

    // parsing command-line arguments
    for (int i = 1; i < argc && AllOk; i++)
    {
        if (argv[i][0] == '-')
            switch (tolower(argv[i][1]))
            {
                case 'b':
                    AddTapeHeader = true;
                    TapeHeader.HType = DT_BASIC;
                    break;
                case 'h':
                    showUsage();
                    return 0;
                    break;
                case 'n':
                    if ((i + 1) < argc)
                    {
                        AddTapeHeader = true;
                        i++;
                        for(int j = 0; argv[i][j] != '\0' && j < 10; j++)
                            TapeHeader.HName[j] = argv[i][j];
                    } else {
                        fprintf(stderr, "Missing parameter");
                        AllOk = false;
                    }
                    break;
                case 'o':
                    if ((i + 1) < argc)
                    {
                        int adr = atoi(argv[i + 1]);
                        if (adr > 0xffff || adr < 0)
                        {
                            fprintf(stderr, "Origin address not in range 0-65535");
                            AllOk = false;
                        }
                        AddTapeHeader = true;
                        TapeHeader.HStartLo = (byte)(adr & 0xff);
                        TapeHeader.HStartHi = (byte)(adr >> 8);
                        i++;
                    } else {
                        fprintf(stderr, "Missing parameter");
                        AllOk = false;
                    }
                    break;
                case 'q':
                    QuietMode = true;
                    break;
                case 'r':
                    RawData = true;
                    break;
                case 's':
                    if ((i + 1) < argc)
                    {
                        int tmp = 0;
                        SelectedBlocks.count++;
                        for (char *c = argv[i + 1];
                             *c != '\0' && SelectedBlocks.count <= MAX_SELECTED_BLOCKS;
                             c++)
                        {
                            if (*c >= '0' && *c <= '9')
                                tmp = tmp * 10 + (*c - '0');
                            else if (*c == '-' && tmp != 0)
                            {
                                SelectedBlocks.block[SelectedBlocks.count - 1].start = tmp;
                                tmp = 0;
                            }
                            else if (*c == ',' && tmp != 0)
                            {
                                if (SelectedBlocks.block[SelectedBlocks.count - 1].start == 0)
                                    SelectedBlocks.block[SelectedBlocks.count - 1].start = tmp;
                                SelectedBlocks.block[SelectedBlocks.count - 1].end = tmp;
                                tmp = 0;
                                SelectedBlocks.count++;
                            }
                            else break;
                        }
                        if (tmp == 0)
                            SelectedBlocks.count--;
                        else
                        {
                            if (SelectedBlocks.block[SelectedBlocks.count - 1].start == 0)
                                SelectedBlocks.block[SelectedBlocks.count - 1].start = tmp;
                            SelectedBlocks.block[SelectedBlocks.count - 1].end = tmp;
                        }
                        i++;
                    } else {
                        fprintf(stderr, "Missing parameter");
                        AllOk = false;
                    }
                    break;
                case '-':
                    if (strcasecmp(&argv[i][2], "help") == 0)
                    {
                        showUsage();
                        return 0;
                    }
                    else if (strcasecmp(&argv[i][2], "raw") == 0)
                        RawData = true;
                    else {
                        fprintf(stderr, "Unknown option");
                        AllOk = false;
                    }
                    break;
                default:
                    fprintf(stderr, "Unknown option");
                    AllOk = false;
                    break;
            }
        else if (!Command)
        {
            if (strcasecmp(argv[i], "add") == 0)
                Command = cm_Add;
            else if (strcasecmp(argv[i], "insert") == 0)
                Command = cm_Insert;
            else if (strcasecmp(argv[i], "extract") == 0)
                Command = cm_Extract;
            else if (strcasecmp(argv[i], "list") == 0)
                Command = cm_List;
            else if (strcasecmp(argv[i], "remove") == 0)
                Command = cm_Remove;
            else if (strcasecmp(argv[i], "replace") == 0)
                Command = cm_Replace;
            else if (strcasecmp(argv[i], "fix-0") == 0)
                Command = cm_Fix0;
            else if (strcasecmp(argv[i], "fix-crc") == 0)
                Command = cm_FixCrc;
        }
        else if (FileNameIn[0] == '\0')
            strncpy(FileNameIn, argv[i], MAX_PATH);
        else if (FileNameOut[0] == '\0')
            strncpy(FileNameOut, argv[i], MAX_PATH);
    }

    if (!Command && AllOk)
    {
        fprintf(stderr, "Unknown command");
        AllOk = false;
    }

    if (FileNameIn[0] == '\0' && AllOk)
    {
        fprintf(stderr, "No input file name was provided");
        AllOk = false;
    }

    if (!AllOk)
    {
        fprintf(stderr, ", try --help\n");
        exit(1);
    }

    switch (Command)
    {
        case cm_Add:
            ChkOutFile(FileNameOut);
            cmdAdd(FileNameIn, FileNameOut);
            break;
        case cm_Insert:
            ChkOutFile(FileNameOut);
            cmdInsert(FileNameIn, FileNameOut);
            break;
        case cm_Extract:
            ChkOutFile(FileNameOut);
            cmdExtract(FileNameIn, FileNameOut);
            break;
        case cm_Remove:
            cmdRemove(FileNameIn);
            break;
        case cm_Replace:
            ChkOutFile(FileNameOut);
            cmdReplace(FileNameIn, FileNameOut);
            break;
        case cm_List:
            cmdList(FileNameIn);
            break;
        case cm_Fix0:
            cmdFix0(FileNameIn);
            break;
        case cm_FixCrc:
            cmdFixCrc(FileNameIn);
            break;
        default:
            break;
    }

    return 0;
}
