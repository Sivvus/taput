#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef unsigned char byte;

enum command
{
    cm_Unknown,
    cm_Add,
    cm_Insert,
    cm_Extract,
    cm_List,
    cm_Remove,
    cm_Help
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
        3, /* Code */
        { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 }, 
        0, 0, /* Length of data block */
        0x00, 0x00, /* Param1 */
        0x00, 0x80, /* Param2 */
        0, /* Parity header */
    };
    
bool AddTapeHeader = false;
bool QuietMode = false;
int SelectedBlock = 0;

byte crc(byte *start, int length)
{
    byte tcrc = 0;
    for (int i = 0; i < length; i++)
        tcrc = tcrc ^ start[i];
    return tcrc;
}

void ChkOutFile(char *FileName)
{
    if (FileName[0] == '\0')
    {
        fprintf(stderr, "Missing output file name\n");
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
       TapeHeader.HLenLo = (byte)(sizeIn & 0xff);
       TapeHeader.HLenHi = (byte)(sizeIn >> 8);
       TapeHeader.Parity1 = crc(&TapeHeader.Flag1, 18); 
       fwrite(&TapeHeader, sizeof(TapeHeader), 1, file);
    }
    byte head[3];
    head[0] = (sizeIn + 2) & 0xff;
    head[1] = (sizeIn + 2) >> 8;
    head[2] = 0xff;
    fwrite(head, 3, 1, file);
    fwrite(bufferIn, sizeIn, 1, file);
    head[0] = crc(bufferIn, sizeIn);
    head[0] = head[0] ^ 0xff;
    fwrite(head, 1, 1, file);
    
    free(bufferIn);
    fclose(file);
}

void cmdInsert(char *FileNameIn, char *FileNameOut)
{
    if (SelectedBlock == 0)
        SelectedBlock = 1;
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
        if (SelectedBlock == i)
        {
            if (AddTapeHeader)
            {
               TapeHeader.HLenLo = (byte)(sizeIn & 0xff);
               TapeHeader.HLenHi = (byte)(sizeIn >> 8);
               TapeHeader.Parity1 = crc(&TapeHeader.Flag1, 18); 
               fwrite(&TapeHeader, sizeof(TapeHeader), 1, file);
            }
            byte head[3];
            head[0] = (sizeIn + 2) & 0xff;
            head[1] = (sizeIn + 2) >> 8;
            head[2] = 0xff;
            fwrite(head, 3, 1, file);
            fwrite(bufferIn, sizeIn, 1, file);
            head[0] = crc(bufferIn, sizeIn);
            head[0] = head[0] ^ 0xff;
            fwrite(head, 1, 1, file);
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
        fprintf(stderr, "Unable to insert file \"%s\" before block %d\n", FileNameIn, SelectedBlock);
        exit(1);
    }
}

void cmdExtract(char *FileNameIn, char *FileNameOut)
{
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
        if (i == SelectedBlock)
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
        fprintf(stderr, "Block %d not found\n", SelectedBlock);
        exit(1);
    }
}

void cmdRemove(char *FileName)
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
        if (i != SelectedBlock)
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
        fprintf(stderr, "Block %d not found\n", SelectedBlock);
        exit(1);
    }
}

void cmdList(char *FileNameIn)
{
    byte *buffer;
    byte *endbuf;
    byte *pos;
    char blockname[11];
    struct tapeheader *Header;
    
    size_t size = LoadFile(&buffer, FileNameIn);
    
    endbuf = buffer + size;
    pos = buffer;
    for (int i = 1; pos < endbuf; i++)
    {
        size_t blocksize = pos[0] | (pos[1] << 8);
        switch (pos[2])
        {
            case 0x00:
                Header = (struct tapeheader*)pos;
                printf("#%-4.2d <HEAD>   %-8d %-6.4X", i, (int)(blocksize - 2), (unsigned int)(pos - buffer));
                for (int i = 0; i < 10; i++) 
                    blockname[i] = Header->HName[i];
                blockname[10] = '\0';
                switch (Header->HType)
                {
                    case 0:
                        printf(" Program: %s LINE %d\n", blockname,
                            Header->HStartLo | (Header->HStartHi << 8));
                        break;
                    case 3:
                        printf(" Bytes:   %s CODE %d,%d\n", blockname,
                            Header->HStartLo | (Header->HStartHi << 8),
                            Header->HLenLo | (Header->HLenHi << 8));
                        break;
                    case 1:
                        printf("Number array: %s\n", blockname);
                        break;
                    case 2:
                        printf("Character array: %s\n", blockname);
                        break;
                    default:
                        printf("Unknown data type\n");
                        break;
                }
                break;
            case 0xff:
                printf("#%-4.2d <DATA>   %-8d %-6.4X\n", i, (int)(blocksize - 2), (unsigned int)(pos - buffer));
                break;
            default:
                break;
        }
        pos = pos + blocksize + 2;
    }
    free(buffer);
}

void showUsage(void)
{
    printf("TAPe UTility 1.0 by Sivvus\n");
    printf("Usage: TAPUT command [options] FileIn [FileOut]\n");
    printf("commands:\n");
    printf("    add           adds a file at the end of the \"tap\" image\n");
    printf("    insert        inserts a file at the beginning of the image,\n");
    printf("                  with the -b <n> option inserts a file before block <n>\n");
    printf("    extract       extracts a block of data to a file\n");
    printf("                  (requires an option -b <n>)\n");
    printf("    remove        remove block of data from the image (requires -b <n>)\n");
    printf("    list          list image content\n");
    printf("    help          display this help and exit\n");
    printf("options:\n");
    printf("    -o <addr>     implies creating a block header,\n");
    printf("                  <addr> sets the origin address\n");
    printf("    -n <name>     implies creating a block header,\n");
    printf("                  <name> sets a block name\n");
    printf("    -b <number>   sets the block number\n");
    printf("\n");
}

int main(int argc, char** argv)
{
    char FileNameIn[256] = "\0";
    char FileNameOut[256] = "\0";
    bool AllOk = true;

    for (int i = 1; i < argc && AllOk; i++)
    {
        if (argv[i][0] == '-')
            switch (tolower(argv[i][1]))
            {
                case 'n':
                    if (strlen(argv[i + 1]) > 10)
                    {
                        fprintf(stderr, "Blockname [-n] too long \"%s\"\n", argv[i + 1]);
                        exit(1);
                    }
                    AddTapeHeader = true;
                    strncpy(TapeHeader.HName, argv[i + 1], strlen(argv[i + 1]));
                    i++;
                    break;
                case 'o':
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
                    break;
                case 'b':
                    SelectedBlock = atoi(argv[i + 1]);
                    i++;
                    break;
                case 'q':
                    QuietMode = true;
                    break;
            }
        else if (!Command)
        {
            if (strcasecmp(argv[i], "add") == 0)
                Command = cm_Add;
            if (strcasecmp(argv[i], "insert") == 0)
                Command = cm_Insert;
            if (strcasecmp(argv[i], "extract") == 0)
                Command = cm_Extract;
            if (strcasecmp(argv[i], "list") == 0)
                Command = cm_List;
            if (strcasecmp(argv[i], "remove") == 0)
                Command = cm_Remove;
            if (strcasecmp(argv[i], "help") == 0)
                Command = cm_Help;
        }
        else if (FileNameIn[0] == '\0')
            strcpy(FileNameIn, argv[i]);
        else if (FileNameOut[0] == '\0')
            strcpy(FileNameOut, argv[i]);
        else if (Command != cm_Help)
            AllOk = false;
    }

    if (FileNameIn[0] == '\0' && Command != cm_Help)
        AllOk = false;

    if (!AllOk)
    {
        if (!Command)
            fprintf(stderr, "Unknown command\n");   
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
        case cm_Help:
            showUsage();
            break;
        default:
            break;
    }

    return 0;
}