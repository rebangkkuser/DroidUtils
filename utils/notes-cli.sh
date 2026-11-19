#!/system/bin/sh

set -e
# Licensed by MIT.
# Needs root and nano or vim/vi

NOTES_CLI_DATA=/data/notes_cli
NOTES=$NOTES_CLI_DATA/notes
VER=0.1.0

mkdir -p "$NOTES_CLI_DATA"
mkdir -p "$NOTES"

help_list() {
        echo "create: creates a note"
        echo "edit: edits a note"
        echo "delete: removes a note"
        echo "help: shows this message"
        echo "version: shows the notes_cli version"
}

if [ -z "$1" ]; then
	echo "notes_cli: cannot have an empty argument! showing help and exiting with code 1"
	help_list
	exit 1
fi

discovercmd() {
	command -v "$@" >/dev/null 2>&1
}

if discovercmd nano; then
	EDITOR=nano
elif discovercmd vim; then
	EDITOR=vim
elif discovercmd vi; then
	EDITOR=vi
else
	echo "notes_cli: didnt find editor! exiting"
	exit 1
fi

case "$1" in
	create)
	 "$EDITOR" "$NOTES"/"$@"
	 echo "Created note/s $@"
	  ;;
	edit)
	 "$EDITOR" "$NOTES"/"$@"
	 echo "Edited note/s $@"
	  ;;
	delete)
	 rm "$NOTES"/"$@"
	 echo "Deleted note/s $@"
	  ;;
	help)
	 help_list
	  ;;
	version)
	 echo "notes_cli version $VER"
	 ;;
	*)
	 echo "notes_cli: command not found. showing help and exiting with code 1"
	 echo
	 help_list
	 exit 1
	 ;;
esac
