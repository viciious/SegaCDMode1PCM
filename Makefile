ifdef $(GENDEV)
ROOTDIR = $(GENDEV)
else
ROOTDIR = /opt/toolchains/sega
endif

CC = $(ROOTDIR)/m68k-elf/bin/m68k-elf-gcc
AS = $(ROOTDIR)/m68k-elf/bin/m68k-elf-as
OBJC = $(ROOTDIR)/m68k-elf/bin/m68k-elf-objcopy
RM = rm -f

OPTION =
INCS = -I. -I$(ROOTDIR)/m68k-elf/include -I$(ROOTDIR)/m68k-elf/m68k-elf/include
CCFLAGS = $(ROOTDIR) -m68000 -Wall -O2 -c -fomit-frame-pointer -ffunction-sections -fdata-sections -flto
HWCCFLAGS = $(ROOTDIR) -m68000 -Wall -O1 -c -fomit-frame-pointer -fno-lto
ASFLAGS = -m68000 --register-prefix-optional
LIBS = -L$(ROOTDIR)/m68k-elf/lib -L$(ROOTDIR)/m68k-elf/lib/gcc/m68k-elf/12.1.0 -L$(ROOTDIR)/m68k-elf/m68k-elf/lib -lc -lgcc -lnosys
LINKFLAGS = -T $(ROOTDIR)/ldscripts/md.ld -Wl,-Map=output.map -nostdlib --specs=nosys.specs -Wl,--gc-sections -flto

OBJS = crt0.o main.o kos.o cd.o hw_md.o font.o data.o scd_pcm.o

all: cd.bin Mode1PCM.bin

Mode1PCM.bin: Mode1PCM.elf
	$(OBJC) -O binary $< temp.bin
	dd if=temp.bin of=$@ bs=512K conv=sync
	$(RM) temp.bin

Mode1PCM.elf: $(OBJS)
	$(CC) $(LINKFLAGS) $(OBJS) $(LIBS) -o Mode1PCM.elf

%.o: %.c
	$(CC) $(CCFLAGS) $(INCLUDE) -c $< -o $@

cd.bin:
	make -C cd

cd.o: cd.s cd.bin

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	$(RM) *.o Mode1PCM.bin temp.bin cdmain.bin *.elf *.map
	make -C cd clean
