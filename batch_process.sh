#!/bin/bash

# Configuration
IN_DIR="../in"
OUT_DIR="../out"
EXECUTABLE="./spec_gen"

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: $EXECUTABLE not found. Please build it first (e.g., 'make')."
    exit 1
fi

# Ensure output directory exists
mkdir -p "$OUT_DIR"

# Loop through all files in the input directory
# We filter for files to avoid directory issues
for input_file in "$IN_DIR"/*; do
    if [ -f "$input_file" ]; then
        # Get the filename
        filename=$(basename -- "$input_file")
        
        # Only process if it has a .wav extension (or just take everything and strip extension)
        # The user mentioned "without .wav", implying .wav files.
        if [[ "$filename" == *.wav ]]; then
            name="${filename%.wav}"
        else
            name="${filename%.*}"
        fi
        
        output_file="$OUT_DIR/${name}.hmsp"
        
        echo "Processing: $filename"
        "$EXECUTABLE" -p "$input_file" "$output_file"
    fi
done

echo "Batch processing complete."
