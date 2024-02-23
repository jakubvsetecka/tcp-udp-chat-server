#!/bin/bash

# Navigate to the script's directory
cd "$(dirname "$0")"

# Ensure the img directory exists
mkdir -p ./img

# Generate PNG from PlantUML, output to img directory
plantuml -tpng ./src/class_diagram.puml -o ../img
echo "PNG file has been generated in the ./img directory."

# Check if the 'open' argument is provided
if [ "$1" == "open" ]; then
    # Assuming the generated PNG has a predictable name
    PNG_FILE="./img/class_diagram.png"
    
    # Check if the file exists
    if [ -f "$PNG_FILE" ]; then
        # Open in the default browser
        xdg-open "$PNG_FILE"
    else
        echo "The PNG file does not exist."
    fi
else
    echo "To open the generated PNG in the browser, rerun with 'open' argument."
fi
