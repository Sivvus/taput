#~~~~   SETTINGS   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#
CC=gcc
CFLAGS=-Os -Wall -Wextra
LDFLAGS=-s -fmerge-all-constants -fno-ident -ffunction-sections \
        -fdata-sections -Wl,--gc-sections -Wl,--build-id=none -static-libgcc
OUT=taput
OBJ=taput.o
CLN=rm -f
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#
ifeq ($(OS),Windows_NT)
	OUT:=$(OUT).exe
	CLN:=del /Q
endif
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#

.PHONY: all clean

all: $(OUT)

$(OUT): $(OBJ)
	$(info Linking to "$@")
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(info Compiling "$<" )
	$(CC) $< -c $(CFLAGS)
	
clean:
	$(info Cleaning ..)
	$(CLN) $(OUT)
	$(CLN) $(OBJ)