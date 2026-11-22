CC ?= cc

ifeq ($(V),1)
    Q :=
else
    Q := @
endif

.PHONY: build

build:
	$(Q)$(CC) -c utils/getmodules.c -o utils/getmodules.o
	$(Q)$(CC) -c utils/kmesg.c -o utils/kmesg.o
	$(Q)$(CC) -c utils/notes_cli.c -o utils/notes_cli.o
	$(Q)$(CC) -c utils/notes_cli_termux.c -o utils/notes_cli_termux.o
	$(Q)$(CC) -c utils/sysinfo.c -o utils/sysinfo.o
	$(Q)echo "Finished compiling binaries. Linking..."
	$(Q)$(CC) utils/getmodules.o -o utils/getmodules
	$(Q)$(CC) utils/kmesg.o -o utils/kmesg
	$(Q)$(CC) utils/notes_cli.o -o utils/notes_cli
	$(Q)$(CC) utils/notes_cli_termux.o -o utils/notes_cli_termux
	$(Q)$(CC) utils/sysinfo.o -o utils/sysinfo
	$(Q)echo "Finished building. Binaries on utils/"

clean:
	$(Q)rm utils/kmesg
	$(Q)rm utils/notes_cli
	$(Q)rm utils/notes_cli_termux
	$(Q)rm utils/sysinfo
	$(Q)rm utils/getmodules
	$(Q)rm utils/*.o
