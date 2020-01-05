#~~~~   SETTINGS   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#
CC=gcc
CFLAGS=-O3 -Wall -Wextra
LDFLAGS=-s -fmerge-all-constants -fno-ident -ffunction-sections \
        -fdata-sections -Wl,--gc-sections -Wl,--build-id=none -static-libgcc
OUT=taput
OBJ=taput.o
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#
ifeq ($(OS),Windows_NT)
	OUT:=$(OUT).exe
	BUILD_TS=$(shell echo %date:~8,2%%date:~3,2%%date:~0,2%%time:~0,2%%time:~3,2%)
	CLN=del /Q
else
	BUILD_TS=$(shell date +%y%m%d%H%M)
	CLN=rm -f
endif
CFLAGS+=-DBUILD_TS="$(BUILD_TS)"
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