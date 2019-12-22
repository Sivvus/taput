#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_SELECTED_BLOCKS 10
#define MAX_PATH 260
#define APP_VERSION "1.05"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define MAX_SELECTED_BLOCKS_S STR(MAX_SELECTED_BLOCKS)

#define DT_BASIC     0
#define DT_NUMARRAY  1
#define DT_CHARARRAY 2
#define DT_CODE      3

typedef unsigned char byte;

enum command
{
    cm_Unknown,
    cm_Add,
    cm_Insert,
    cm_Extract,
    cm_List,
    cm_Remove,
    cm_Fix
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
        19, 0, /* Len header */
        0, /* Flag header */
        DT_CODE,  
        { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 }, 
        0, 0, /* Length of data block */
        0x00, 0x00, /* Param1 */
        0x00, 0x80, /* Param2 */
        0, /* Parity header */
    };
    
struct blockrange
{
    int start;
    int end;
};

struct selectedblocks
{
    int count;
    struct blockrange block[MAX_SELECTED_BLOCKS];
} SelectedBlocks;
    
bool AddTapeHeader = false;
bool QuietMode = false;

void showUsage(void) 
{
    puts("TAPe UTility " APP_VERSION " by Sivvus\n"
         "Usage: TAPUT command [options] FileIn [FileOut]\n"
         "Commands:\n"
         "    add              Add a file at the end of the \"tap\" image\n"
         "                     if the image does not exist, it will be created\n"
         "    insert           Insert a file at the beginning of the image,\n"
         "                     with the -s <n> option inserts a file before block n\n"
         "    extract          Extract a block of data to a file\n"
         "                     (requires an option -s <n>)\n"
         "    remove           Remove blocks of data from the image (requires -s (<n>|<n>-<m>))\n"
         "                     up to " MAX_SELECTED_BLOCKS_S " blocks can be selected\n"
         "    fix              Remove blocks with zero size from the image\n"
         "    list             List image content\n"
         "Options:\n"
         "    -b               Creates a BASIC program header block\n"
         "    -o <addr>        Implies creating a block header,\n"
         "                     <addr> sets the origin address or the BASIC autostart line number\n"
         "    -n <name>        Implies creating a block header,\n"
         "                     <name> sets a block name\n"
         "    -s (<n>|<n>-<m>)[,(<n>|<n>-<m>)]..\n"
         "                     Selects the block numbers and/or block ranges (up to " MAX_SELECTED_BLOCKS_S ")\n"
         "    -h, --help       Display this help and exit\n"
         "Examples:\n"
         "    taput add -o 32768 -n BlockName file.bin image.tap\n"
         "    taput insert -s 3 -o 32768 -n BlockName file.bin image.tap\n"
         "    taput extract -s 2 image.tap file.bin\n"
         "    taput remove -s 1-4,8,12 image.tap\n"
         "    taput fix image.tap\n"
         "    taput list image.tap\n"
    );
}

byte crc(byte *start, int length)
{
    byte tcrc = 0;
    for (int i = 0; i < length; i++)
        tcrc = tcrc ^ start[i];
    return tcrc;
}

void PrepareTapeHeader(size_t size)
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
    TapeHeader.HLenLo = (byte)(size & 0xff);
    TapeHeader.HLenHi = (byte)(size >> 8);
    TapeHeader.Parity1 = crc(&TapeHeader.Flag1, 18); 
}

void ChkOutFile(char *FileName)
{
    if (FileName[0] == '\0')
    {
        fprintf(stderr, "No output file name was given\n");
        if (!QuietMode) 
            showUsage();
        exit(1);
    }
}

size_t LoadFile(byte **Dest, char *FileName)
{
    FILE *file;
    size_t size;
    file = fopen(FileName, "rb");
    if (!file)
        return 0;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    *Dest = (byte *)malloc(size + 1);
    if (!*Dest)
    {
        fprintf(stderr, "Out of memory\n");
        fclose(file);
        exit(1);        
    }
    fread(*Dest, size, 1, file);
    fclose(file);
    
    return size;
}

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

void cmdAdd(char *FileNameIn, char *FileNameOut)
{
    byte *bufferOut, *bufferIn, *pos, *endbuf;
    size_t sizeIn  = LoadFile(&bufferIn, FileNameIn);
    if (sizeIn > 0xffff)
    {
        fprintf(stderr, "The file \"%s\" is too large\n", FileNameIn);
        free(bufferIn);
        exit(1);
    }
    if (sizeIn == 0)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameIn);
        exit(1);
    }         
        
    size_t sizeOut = LoadFile(&bufferOut, FileNameOut);
    endbuf = bufferOut + sizeOut;
    pos = bufferOut;
    FILE *file;
    file = fopen(FileNameOut, "wb");
    if (!file)
    {
        fprintf(stderr, "Unable to create file \"%s\"\n", FileNameOut);
        free(bufferOut);
        free(bufferIn);
        exit(1);              
    }
    
    if (sizeOut > 0)
    {
        for (int i = 1; pos < endbuf; i++)
        {
            size_t blocksize = pos[0] | (pos[1] << 8);
            fwrite(pos, blocksize + 2, 1, file);
            pos = pos + blocksize + 2;   
        }
        free(bufferOut);
    }    

    if (AddTapeHeader)
    {
        PrepareTapeHeader(sizeIn);
        fwrite(&TapeHeader, sizeof(TapeHeader), 1, file);
    }
    byte Head[3];
    Head[0] = (sizeIn + 2) & 0xff;
    Head[1] = (sizeIn + 2) >> 8;
    Head[2] = 0xff;
    fwrite(Head, sizeof(Head), 1, file);
    fwrite(bufferIn, sizeIn, 1, file);
    byte Parity = crc(bufferIn, sizeIn) ^ Head[2];
    fwrite(&Parity, sizeof(Parity), 1, file);
    
    free(bufferIn);
    fclose(file);
}

void cmdInsert(char *FileNameIn, char *FileNameOut)
{
    if (SelectedBlocks.count == 0)
    {
        SelectedBlocks.count = 1;
        SelectedBlocks.block[0].start = 1;        
        SelectedBlocks.block[0].end = 1;        
    }
    
    if (SelectedBlocks.count != 1)
    {
        fprintf(stderr, "No block selected\n");
        exit(1);
    }
    
    if (SelectedBlocks.block[0].start != SelectedBlocks.block[0].end)
    {
        fprintf(stderr, "Not allowed to use block range in this command\n");
        exit(1);
    }
    
    bool isDone = false;
    byte *bufferOut, *bufferIn, *pos, *endbuf;

    size_t sizeIn  = LoadFile(&bufferIn, FileNameIn);
    if (sizeIn > 0xffff)
    {
        fprintf(stderr, "The file \"%s\" is too large\n", FileNameIn);
        free(bufferIn);
        exit(1);
    }
    if (sizeIn == 0)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameIn);
        exit(1);
    }   
    
    size_t sizeOut = LoadFile(&bufferOut, FileNameOut);
    if (sizeOut == 0)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameOut);
        exit(1);
    } 
    
    endbuf = bufferOut + sizeOut;
    pos = bufferOut;
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
            Head[0] = (sizeIn + 2) & 0xff;
            Head[1] = (sizeIn + 2) >> 8;
            Head[2] = 0xff;
            fwrite(Head, sizeof(Head), 1, file);
            fwrite(bufferIn, sizeIn, 1, file);
            byte Parity = crc(bufferIn, sizeIn) ^ Head[2];
            fwrite(&Parity, sizeof(Parity), 1, file);
            isDone = true;
        }
        size_t blocksize = pos[0] | (pos[1] << 8);
        fwrite(pos, blocksize + 2, 1, file);
        pos = pos + blocksize + 2;   
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

void cmdExtract(char *FileNameIn, char *FileNameOut)
{
    if (SelectedBlocks.count != 1)
    {
        fprintf(stderr, "No block selected\n");
        exit(1);
    }
    
    if (SelectedBlocks.block[0].start != SelectedBlocks.block[0].end)
    {
        fprintf(stderr, "Not allowed to use block range in this command\n");
        exit(1);
    }
    
    bool isDone = false;
    byte *buffer;
    byte *pos;
    size_t size = LoadFile(&buffer, FileNameIn);
    if (size == 0)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileNameIn);
        exit(1);
    }
    byte *endbuf = buffer + size;
    pos = buffer;
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
          fwrite(pos + 3, blocksize - 2, 1, file);
          fclose(file);
          isDone = true;
        }    
        pos = pos + blocksize + 2;
    }
    free(buffer);
    
    if (!isDone)
    {
        fprintf(stderr, "Selected block was not found\n");
        exit(1);
    }
}

void cmdRemove(char *FileName)
{
    if (SelectedBlocks.count == 0)
    {
        fprintf(stderr, "No block selected\n");
        exit(1);
    }

    bool isDone = false;
    byte *buffer, *pos, *endbuf;
    size_t size = LoadFile(&buffer, FileName);
    if (size == 0)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileName);
        exit(1);
    }
    endbuf = buffer + size;
    pos = buffer;
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
            fwrite(pos, blocksize + 2, 1, file);
        } 
        else isDone = true;
        pos = pos + blocksize + 2;        
    }
    fclose(file);
    free(buffer);

    if (!isDone)
    {
        fprintf(stderr, "Selected blocks were not found\n");
        exit(1);
    }
}

void cmdFix(char *FileName)
{
    bool isDone = false;
    byte *buffer, *pos, *endbuf;
    size_t size = LoadFile(&buffer, FileName);
    if (size == 0)
    {
        fprintf(stderr, "Unable to open file \"%s\"\n", FileName);
        exit(1);
    }
    endbuf = buffer + size;
    pos = buffer;
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
            fwrite(pos, blocksize + 2, 1, file);
        } 
        else
            isDone = true;
        pos = pos + blocksize + 2;        
    }
    fclose(file);
    free(buffer);

    if (!isDone)
    {
        fprintf(stderr, "No zero size blocks found\n");
        exit(1);
    }
}

void cmdList(char *FileNameIn)
{
    byte *buffer;
    byte *endbuf;
    byte *pos;
    size_t size = LoadFile(&buffer, FileNameIn);
    
    endbuf = buffer + size;
    pos = buffer;
    for (int i = 1; pos < endbuf; i++)
    {
        size_t blocksize = pos[0] | (pos[1] << 8);
        int datasize = blocksize - 2;
        if (datasize < 0)
            datasize = 0;
        // check crc
        char crcStr[8];
        int crcCalc = crc(&pos[2], datasize + 1);
        if (pos[blocksize + 1] == crcCalc) 
            sprintf(crcStr, "%.2X",crcCalc);
        else
            sprintf(crcStr, "%.2X(%.2X)", (int)pos[blocksize + 1], crcCalc);
        // blocktype
        char typeStr[7];
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
        // zero lenght block
        if (!datasize)
        {
            strcpy(crcStr, "--");
            strcpy(typeStr, "<---->");
        }
        
        printf("#%d\t%s\t%5d\t%s\t%.6X\t", i, typeStr, datasize, crcStr, (unsigned int)(pos - buffer));
        
        if (!blocksize)
        {
            puts("zero-length block");
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
}

int main(int argc, char **argv)
{
    char FileNameIn[MAX_PATH + 1] = "\0";
    char FileNameOut[MAX_PATH + 1] = "\0";
    bool AllOk = true;
    memset(&SelectedBlocks, 0, sizeof(SelectedBlocks));
    
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
                        if (strlen(argv[i + 1]) > 10)
                        {
                            fprintf(stderr, "Blockname [-n] too long \"%s\"\n", argv[i + 1]);
                            exit(1);
                        }
                        AddTapeHeader = true;
                        strncpy(TapeHeader.HName, argv[i + 1], strlen(argv[i + 1]));
                        i++;
                    }
                    else
                        AllOk = false;
                    break;
                case 'o':
                    if ((i + 1) < argc)
                    {
                        int adr = atoi(argv[i + 1]);
                        if (adr > 0xffff || adr < 0)
                        {
                            fprintf(stderr, "Origin [-o] address not in range 0-65535\n");
                            exit(1);
                        }
                        AddTapeHeader = true;
                        TapeHeader.HStartLo = (byte)(adr & 0xff);
                        TapeHeader.HStartHi = (byte)(adr >> 8);
                        i++;
                    }
                    else
                        AllOk = false;
                    break;
                case 'q':
                    QuietMode = true;
                    break;
                case 's':
                    if ((i + 1) < argc)
                    {
                        int tmp = 0;
                        SelectedBlocks.count++;
                        for (char *c = argv[i + 1]; *c != '\0' && SelectedBlocks.count <= MAX_SELECTED_BLOCKS; c++)
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
                    }
                    else
                        AllOk = false;
                    break;
                case '-':
                    if (strcasecmp(&argv[i][2], "help") == 0)
                    {
                        showUsage();
                        return 0;
                    }
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
            else if (strcasecmp(argv[i], "fix") == 0)
                Command = cm_Fix;
        }
        else if (FileNameIn[0] == '\0')
            strncpy(FileNameIn, argv[i], MAX_PATH);
        else if (FileNameOut[0] == '\0')
            strncpy(FileNameOut, argv[i], MAX_PATH);
        else 
            AllOk = false;
    }

    if (!Command)
    {
        fprintf(stderr, "Unknown command\n"); 
        AllOk = false;
    }

    if (FileNameIn[0] == '\0')
    {
        fprintf(stderr, "No input file name was provided\n");   
        AllOk = false;
    }

    if (!AllOk)
    {
        if (!QuietMode) 
            showUsage();
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
        case cm_List:
            cmdList(FileNameIn);
            break;
        case cm_Fix:
            cmdFix(FileNameIn);
            break;
        default:
            break;
    }

    return 0;
}