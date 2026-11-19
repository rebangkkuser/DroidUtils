#!/system/bin/sh

# notes_cli - Simple notes manager for Android
# Licensed under the MIT License.

set -e

NOTES_CLI_DATA="/data/notes_cli"
NOTES="$NOTES_CLI_DATA/notes"
VER="0.1.0"

die() {
    echo "notes_cli: $1" >&2
    exit 1
}

init() {
    mkdir -p "$NOTES"
}

check_name() {
    [ -n "$1" ] || die "missing note name"

    case "$1" in
        *[!a-zA-Z0-9._-]*|.*|..)
            die "invalid note name: $1"
            ;;
    esac
}

get_editor() {
    if [ -n "$EDITOR" ]; then
        command -v "$EDITOR" >/dev/null 2>&1 ||
            die "EDITOR not found: $EDITOR"
        return
    fi

    if command -v nano >/dev/null 2>&1; then
        EDITOR=nano
    elif command -v vim >/dev/null 2>&1; then
        EDITOR=vim
    elif command -v vi >/dev/null 2>&1; then
        EDITOR=vi
    else
        die "no supported editor found (nano, vim or vi)"
    fi
}

create_note() {
    check_name "$2"
    init

    FILE="$NOTES/$2"

    [ ! -e "$FILE" ] || die "note already exists"

    get_editor
    "$EDITOR" "$FILE"

    [ -f "$FILE" ] || die "note was not created"

    echo "Created note: $2"
}

edit_note() {
    check_name "$2"
    init

    FILE="$NOTES/$2"

    [ -f "$FILE" ] || die "note does not exist"

    get_editor
    "$EDITOR" "$FILE"

    echo "Edited note: $2"
}

read_note() {
    check_name "$2"

    FILE="$NOTES/$2"

    [ -f "$FILE" ] || die "note does not exist"

    cat "$FILE"
}

delete_note() {
    check_name "$2"

    FILE="$NOTES/$2"

    [ -f "$FILE" ] || die "note does not exist"

    rm "$FILE"

    echo "Deleted note: $2"
}

list_notes() {
    init

    FOUND=0

    for FILE in "$NOTES"/*; do
        [ -f "$FILE" ] || continue

        echo "${FILE##*/}"
        FOUND=1
    done

    [ "$FOUND" -eq 1 ] || echo "No notes found."
}

search_notes() {
    [ -n "$2" ] || die "missing search term"
    [ -d "$NOTES" ] || die "notes directory does not exist"

    FOUND=0

    for FILE in "$NOTES"/*; do
        [ -f "$FILE" ] || continue

        if grep -n -F "$2" "$FILE"; then
            FOUND=1
        fi
    done

    [ "$FOUND" -eq 1 ] || die "no matches found"
}

rename_note() {
    check_name "$2"
    check_name "$3"
    init

    OLD="$NOTES/$2"
    NEW="$NOTES/$3"

    [ -f "$OLD" ] || die "source note does not exist"
    [ ! -e "$NEW" ] || die "destination already exists"

    mv "$OLD" "$NEW"

    echo "Renamed $2 to $3"
}

help_list() {
    cat <<EOF
notes_cli v$VER

Usage: notes_cli <command> [arguments]

Commands:
  create <name>       Create a note
  edit <name>         Edit a note
  read <name>         Read a note
  delete <name>       Delete a note
  list                List notes
  search <term>       Search notes
  rename <old> <new>  Rename a note
  help                Show help
  version             Show version
EOF
}

case "$1" in
    create) create_note "$@" ;;
    edit) edit_note "$@" ;;
    read) read_note "$@" ;;
    delete) delete_note "$@" ;;
    list) list_notes ;;
    search) search_notes "$@" ;;
    rename) rename_note "$@" ;;
    help|-h|--help) help_list ;;
    version|-v|--version) echo "notes_cli version $VER" ;;
    "") help_list; exit 1 ;;
    *) die "unknown command: $1" ;;
esac

