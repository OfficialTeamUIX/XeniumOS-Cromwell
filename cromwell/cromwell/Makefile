CC	= ccache gcc
INCLUDE = -I$(TOPDIR)/include -I$(TOPDIR)/ -I./ -I$(TOPDIR)/fs/cdrom \
	-I$(TOPDIR)/fs/fatx -I$(TOPDIR)/lib/eeprom -I$(TOPDIR)/lib/crypt \
	-I$(TOPDIR)/drivers/video -I$(TOPDIR)/drivers/flash -I$(TOPDIR)/lib/misc \
	-I$(TOPDIR)/boot_xbe/ -I$(TOPDIR)/fs/grub -I$(TOPDIR)/lib/font -I$(TOPDIR)/lib/jpeg-6b \
	-I$(TOPDIR)/startuploader -I$(TOPDIR)/drivers/cpu

CFLAGS	= -O2 -mcpu=pentium -Werror $(INCLUDE) -Wstrict-prototypes -fomit-frame-pointer -pipe
LD      = ld
OBJCOPY = objcopy

export CC

TOPDIR  := $(shell /bin/pwd)
SUBDIRS	= boot_rom fs drivers lib xcallout boot

LDFLAGS-ROM     = -s -S -T $(TOPDIR)/scripts/ldscript-crom.ld
LDFLAGS-XBEBOOT = -s -S -T $(TOPDIR)/scripts/xbeboot.ld
LDFLAGS-ROMBOOT = -s -S -T $(TOPDIR)/boot_rom/bootrom.ld



OBJECTS-IMAGEBLD = $(TOPDIR)/bin/imagebld.o
OBJECTS-IMAGEBLD += $(TOPDIR)/bin/sha1.o
OBJECTS-IMAGEBLD += $(TOPDIR)/obj/lzari.o

OBJECTS-XBE = $(TOPDIR)/boot_xbe/xbeboot.o


OBJECTS-ROMBOOT = $(TOPDIR)/obj/2bBootStartup.o
OBJECTS-ROMBOOT += $(TOPDIR)/obj/2bPicResponseAction.o
OBJECTS-ROMBOOT += $(TOPDIR)/obj/2bBootStartBios.o
OBJECTS-ROMBOOT += $(TOPDIR)/obj/sha1.o
OBJECTS-ROMBOOT += $(TOPDIR)/obj/2bBootLibrary.o
OBJECTS-ROMBOOT += $(TOPDIR)/obj/2bdecompress.o

OBJECTS-CROM = $(TOPDIR)/obj/BootStartup.o
OBJECTS-CROM += $(TOPDIR)/obj/BootResetAction.o
OBJECTS-CROM += $(TOPDIR)/obj/BootPerformPicChallengeResponseAction.o
OBJECTS-CROM += $(TOPDIR)/obj/BootPciPeripheralInitialization.o
OBJECTS-CROM += $(TOPDIR)/obj/BootVgaInitialization.o
OBJECTS-CROM += $(TOPDIR)/obj/VideoInitialization.o
OBJECTS-CROM += $(TOPDIR)/obj/BootIde.o
OBJECTS-CROM += $(TOPDIR)/obj/BootHddKey.o
OBJECTS-CROM += $(TOPDIR)/obj/rc4.o
OBJECTS-CROM += $(TOPDIR)/obj/sha1.o
OBJECTS-CROM += $(TOPDIR)/obj/BootVideoHelpers.o
OBJECTS-CROM += $(TOPDIR)/obj/vsprintf.o
OBJECTS-CROM += $(TOPDIR)/obj/BootStartBios.o
OBJECTS-CROM += $(TOPDIR)/obj/setup.o
OBJECTS-CROM += $(TOPDIR)/obj/BootFilesystemIso9660.o
OBJECTS-CROM += $(TOPDIR)/obj/BootLibrary.o
OBJECTS-CROM += $(TOPDIR)/obj/cputools.o
OBJECTS-CROM += $(TOPDIR)/obj/microcode.o
OBJECTS-CROM += $(TOPDIR)/obj/ioapic.o
OBJECTS-CROM += $(TOPDIR)/obj/BootInterrupts.o
OBJECTS-CROM += $(TOPDIR)/obj/fsys_reiserfs.o
OBJECTS-CROM += $(TOPDIR)/obj/char_io.o
OBJECTS-CROM += $(TOPDIR)/obj/disk_io.o
OBJECTS-CROM += $(TOPDIR)/obj/jdapimin.o
OBJECTS-CROM += $(TOPDIR)/obj/jdapistd.o
OBJECTS-CROM += $(TOPDIR)/obj/jdtrans.o
OBJECTS-CROM += $(TOPDIR)/obj/jdatasrc.o
OBJECTS-CROM += $(TOPDIR)/obj/jdmaster.o
OBJECTS-CROM += $(TOPDIR)/obj/jdinput.o
OBJECTS-CROM += $(TOPDIR)/obj/jdmarker.o
OBJECTS-CROM += $(TOPDIR)/obj/jdhuff.o
OBJECTS-CROM += $(TOPDIR)/obj/jdphuff.o
OBJECTS-CROM += $(TOPDIR)/obj/jdmainct.o
OBJECTS-CROM += $(TOPDIR)/obj/jdcoefct.o
OBJECTS-CROM += $(TOPDIR)/obj/jdpostct.o
OBJECTS-CROM += $(TOPDIR)/obj/jddctmgr.o
OBJECTS-CROM += $(TOPDIR)/obj/jidctfst.o
OBJECTS-CROM += $(TOPDIR)/obj/jidctflt.o
OBJECTS-CROM += $(TOPDIR)/obj/jidctint.o
OBJECTS-CROM += $(TOPDIR)/obj/jidctred.o
OBJECTS-CROM += $(TOPDIR)/obj/jdsample.o
OBJECTS-CROM += $(TOPDIR)/obj/jdcolor.o
OBJECTS-CROM += $(TOPDIR)/obj/jquant1.o
OBJECTS-CROM += $(TOPDIR)/obj/jquant2.o
OBJECTS-CROM += $(TOPDIR)/obj/jdmerge.o
OBJECTS-CROM += $(TOPDIR)/obj/jmemnobs.o
OBJECTS-CROM += $(TOPDIR)/obj/jmemmgr.o
OBJECTS-CROM += $(TOPDIR)/obj/jcomapi.o
OBJECTS-CROM += $(TOPDIR)/obj/jutils.o
OBJECTS-CROM += $(TOPDIR)/obj/jerror.o
OBJECTS-CROM += $(TOPDIR)/obj/BootAudio.o
OBJECTS-CROM += $(TOPDIR)/obj/BootFlash.o
OBJECTS-CROM += $(TOPDIR)/obj/BootFlashUi.o
OBJECTS-CROM += $(TOPDIR)/obj/BootEEPROM.o
OBJECTS-CROM += $(TOPDIR)/obj/BootParser.o
OBJECTS-CROM += $(TOPDIR)/obj/BootFATX.o
#USB
OBJECTS-CROM += $(TOPDIR)/obj/config.o
OBJECTS-CROM += $(TOPDIR)/obj/hcd-pci.o
OBJECTS-CROM += $(TOPDIR)/obj/hcd.o
OBJECTS-CROM += $(TOPDIR)/obj/hub.o
OBJECTS-CROM += $(TOPDIR)/obj/message.o
OBJECTS-CROM += $(TOPDIR)/obj/ohci-hcd.o
OBJECTS-CROM += $(TOPDIR)/obj/buffer_simple.o
OBJECTS-CROM += $(TOPDIR)/obj/urb.o
OBJECTS-CROM += $(TOPDIR)/obj/usb-debug.o
OBJECTS-CROM += $(TOPDIR)/obj/usb.o
OBJECTS-CROM += $(TOPDIR)/obj/BootUSB.o
OBJECTS-CROM += $(TOPDIR)/obj/usbwrapper.o
OBJECTS-CROM += $(TOPDIR)/obj/linuxwrapper.o
OBJECTS-CROM += $(TOPDIR)/obj/xpad.o
OBJECTS-CROM += $(TOPDIR)/obj/xremote.o
OBJECTS-CROM += $(TOPDIR)/obj/usbkey.o
OBJECTS-CROM += $(TOPDIR)/obj/risefall.o

OBJECTS-CROM += $(TOPDIR)/obj/xcallout.o

RESOURCES = $(TOPDIR)/obj/backdrop.elf
#RESOURCES += $(TOPDIR)/obj/backdrop_focus.elf


export INCLUDE
export TOPDIR

all: resources cromsubdirs image-crom.bin default.xbe image.bin imagecompress

cromsubdirs: $(patsubst %, _dir_%, $(SUBDIRS))
$(patsubst %, _dir_%, $(SUBDIRS)) : dummy
	$(MAKE) CFLAGS="$(CFLAGS)" -C $(patsubst _dir_%, %, $@)

dummy:

resources:
	# Background
	${LD} -r --oformat elf32-i386 -o $(TOPDIR)/obj/backdrop.elf -T $(TOPDIR)/scripts/backdrop.ld -b binary $(TOPDIR)/pics/backdrop.jpg
#	${LD} -r --oformat elf32-i386 -o $(TOPDIR)/obj/backdrop_focus.elf -T $(TOPDIR)/scripts/backdrop_focus.ld -b binary $(TOPDIR)/pics/focus.jpg

install:
	lmilk -f -p $(TOPDIR)/image/image.bin
	lmilk -f -a c0000 -p $(TOPDIR)/image/image.bin -q
clean:
	find . \( -name '*.[oas]' -o -name core -o -name '.*.flags' \) -type f -print \
		| grep -v lxdialog/ | xargs rm -f
	rm -f $(TOPDIR)/obj/*.bin
	rm -f $(TOPDIR)/obj/*.elf
	rm -f $(TOPDIR)/image/*.bin
	rm -f $(TOPDIR)/image/*.xbe
	rm -f $(TOPDIR)/xbe/*.xbe $(TOPDIR)/xbe/*.bin
	rm -f $(TOPDIR)/xbe/*.elf
	rm -f $(TOPDIR)/image/*.bin
	rm -f $(TOPDIR)/bin/imagebld*
	rm -f $(TOPDIR)/boot_vml/disk/vmlboot
	mkdir $(TOPDIR)/xbe -p
	mkdir $(TOPDIR)/image -p
	mkdir $(TOPDIR)/obj -p
	mkdir $(TOPDIR)/bin -p

#rombios.bin:
#	${GCC295} -E $< > _rombios_.c
#	${BCC} -o rombios.s -c -D__i86__ -0 _rombios_.c
#	sed -e 's/^\.text//' -e 's/^\.data//' rombios.s > _rombios_.s
#	as86 _rombios_.s -b rombios.bin -u- -w- -g -0 -j -O -l rombios.txt
#	ls -l rombios.bin
#
#rombios.elf : rombios.bin
#	${LD} -r --oformat elf32-i386 -o $@ -T rombios.ld -b binary rombios.bin


image-crom.bin:
	${LD} -o $(TOPDIR)/obj/image-crom.elf ${OBJECTS-CROM} ${RESOURCES} ${LDFLAGS-ROM}
	${OBJCOPY} --output-target=binary --strip-all $(TOPDIR)/obj/image-crom.elf $(TOPDIR)/obj/$@

default.xbe: ${OBJECTS-XBE}
	${LD} -o $(TOPDIR)/obj/default.elf ${OBJECTS-XBE} ${LDFLAGS-XBEBOOT}
	${OBJCOPY} --output-target=binary --strip-all $(TOPDIR)/obj/default.elf $(TOPDIR)/xbe/$@

image.bin:
	${LD} -o $(TOPDIR)/obj/2lbimage.elf ${OBJECTS-ROMBOOT} ${LDFLAGS-ROMBOOT}
	${OBJCOPY} --output-target=binary --strip-all $(TOPDIR)/obj/2lbimage.elf $(TOPDIR)/obj/2blimage.bin

imagecompress:
	gcc $(OBJECTS-IMAGEBLD) -o $(TOPDIR)/bin/imagebld $(INCLUDE)
	$(TOPDIR)/bin/imagebld -rom $(TOPDIR)/obj/2blimage.bin $(TOPDIR)/obj/image-crom.bin $(TOPDIR)/image/image.bin $(TOPDIR)/image/image_1024.bin
	$(TOPDIR)/bin/imagebld -xbe $(TOPDIR)/xbe/default.xbe $(TOPDIR)/obj/image-crom.bin
	$(TOPDIR)/bin/imagebld -vml $(TOPDIR)/boot_vml/disk/vmlboot $(TOPDIR)/obj/image-crom.bin
