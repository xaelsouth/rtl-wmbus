RM=rm
MKDIR=mkdir
CC=gcc
STRIP=strip

OUTDIR?=build
OUTFILE="$(OUTDIR)/rtl_wmbus"
CFLAGS?=-Iinclude -std=gnu99
CFLAGS_WARNINGS?=-Wall -W -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual -Wchar-subscripts -Wcomment -Wno-float-equal -Winline -Wmain -Wmissing-noreturn -Wno-missing-prototypes -Wparentheses -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow -Wsign-compare -Wstrict-prototypes -Wswitch -Wunreachable-code -Wno-unused -Wuninitialized
LIB?=-lm
SRC=rtl_wmbus.c

all: $(OUTDIR) release

$(OUTDIR):
	$(MKDIR) -p "$(OUTDIR)"

release: $(OUTDIR)
	$(CC) -DNDEBUG -O3                  $(CFLAGS) $(CFLAGS_WARNINGS) -o $(OUTFILE) $(SRC) $(LIB)

debug: $(OUTDIR)
	$(CC) -DDEBUG  -O0 -g3 -ggdb -p -pg $(CFLAGS) $(CFLAGS_WARNINGS) -o $(OUTFILE) $(SRC) $(LIB)

# Will build on Raspberry Pi 1 only
pi1:
	$(CC) -DNDEBUG -O3 -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -ffast-math $(CFLAGS) $(CFLAGS_WARNINGS) -o $(OUTFILE) $(SRC) $(LIB)

rebuild: clean all

install: release
	cp $(OUTFILE) /usr/bin

clean:
	$(RM) -rf "$(OUTDIR)"
