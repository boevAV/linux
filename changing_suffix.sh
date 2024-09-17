#!/bin/bash

# Function to display help
display_help() {
    echo "Usage: $0 [OPTIONS] <directory> <source suffix> <target suffix>"
    echo
    echo "Description: This script recursively changes the suffix of files in the specified directory."
    echo
    echo "Options:"
    echo "  -h  Display this help message"
    echo "  -r  Process files in subdirectories as well (recursive mode)"
    echo "  -v  Verbose mode, display detailed output"
    echo
    echo "Example:"
    echo
    echo "  $0 -r -v /path/to/directory jpg png"
}

# Function to change suffix
change_suffix() {
    local file="$1"
    local source_suffix="$2"
    local target_suffix="$3"
    local verbose_mode="$4"
    local new_file="${file%.$source_suffix}.$target_suffix"
    if [ $verbose_mode -eq 1 ]; then
        echo "DEBUG: File: $file, New file: $new_file"
    fi
    mv "$file" "$new_file"
    if [ $verbose_mode -eq 1 ]; then
        echo "Changed suffix of file: $file -> $new_file"
    fi
}

process_directory() {
    local dir="$1"
    local source_suffix="$2"
    local target_suffix="$3"
    local verbose_mode="$4"

    for file in "$dir"/*; do
        if [ -f "$file" ] && [[ "$file" == *.$source_suffix ]]; then
            change_suffix "$file" "$source_suffix" "$target_suffix" "$verbose_mode"
        elif [ -d "$file" ]; then
            process_directory "$file" "$source_suffix" "$target_suffix" "$verbose_mode"
        fi
    done
}

# Parsing options
while getopts "hrv" opt; do
    case $opt in
        h)
            display_help
            exit 0
            ;;
        r)
            recursive_mode=1
            ;;
        v)
            verbose_mode=1
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            exit 1
            ;;
    esac
done
shift $((OPTIND - 1))

# Check for the correct number of arguments after parsing options
if [ "$#" -ne 3 ]; then
    display_help
    exit 1
fi

# Extracting arguments
directory="$1"
source_suffix="$2"
target_suffix="$3"
recursive_mode=${recursive_mode:-0}
verbose_mode=${verbose_mode:-0}

if [ $recursive_mode -eq 1 ]; then
    if ! [ -d "$directory" ]; then
        echo "Ошибка: не удается получить доступ к указанному каталогу $directory"
        exit 1
    fi
    process_directory "$directory" "$source_suffix" "$target_suffix" "$verbose_mode" 
else
    if ! [ -d "$directory" ]; then
        echo "Ошибка: не удается получить доступ к указанному каталогу $directory"
        exit 1
    fi
    for file in "$directory"/*."$source_suffix"; do
        if ! [ -e "$file" ]; then
            echo "Ошибка: не удается получить доступ к указанному файлу $file"
            exit 1
        fi
        change_suffix "$file" "$source_suffix" "$target_suffix" "$verbose_mode"
    done
fi
