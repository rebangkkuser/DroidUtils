CC ?= cc
LD ?= ld

ifeq ($(V),1)
    Q :=
else
    Q := @
endif

.PHONY: build

build:
	$(Q)$(CC) utils/kmesg.c -o utils/kmesg.o
	$(Q)$(CC) utils/notes_cli.c -o utils/notes_cli.o
	$(Q)$(CC) utils/notes_cli_termux.c -o utils/notes_cli_termux.o
	$(Q)$(CC) utils/sysinfo.c -o utils/sysinfo.o
	$(Q)echo "Finished compiling binaries. Linking..."
	$(Q)$(LD) utils/kmesg.o -o utils/kmesg
	$(Q)$(LD) utils/notes_cli.o -o utils/notes_cli
	$(Q)$(LD) utils/notes_cli_termux.o -o utils/notes_cli_termux
	$(Q)$(LD) utils/sysinfo.o -o utils/sysinfo
	$(Q)echo "Finished building. Binaries on utils/"

clean:
	$(Q)rm utils/kmesg
	$(Q)rm utils/notes_cli
	$(Q)rm utils/notes_cli_termux
	$(Q)rm utils/sysinfo
	$(Q)rm utils/*.o
