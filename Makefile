DESC=TAPe UTility 1.0
CC=gcc
CFLAGS=-Os -Wall -Wextra
LDFLAGS=-s -fmerge-all-constants -fno-ident -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--build-id=none -static-libgcc
OUT=taput
OBJ=taput.o

ifeq ($(OS),Windows_NT)
	OUT:=$(OUT).exe
endif

.PHONY: all clean test install

all: $(OUT)

$(OUT): $(OBJ)
	$(info Linking to "$@")
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(info Compiling "$<" )
	$(CC) $< -c $(CFLAGS)
	
clean:
	$(info Cleaning ..)
	-rm -f $(OUT) $(OBJ)