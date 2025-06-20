#!/bin/bash

echo "=== Code Compiler Clients ==="
echo "1. Run C client"
echo "2. Run Python client"
echo "3. Run Admin client"
echo "4. Exit"
echo ""

read -p "Choose option (1-3): " choice

case $choice in
    1)
        echo "Running C client..."
        read -p "Enter source file path: " file
        read -p "Enter output file path: " output
        bin/client --file "$file" --output "$output"
        ;;
    2)
        echo "Running Python client..."
        read -p "Enter source file path: " file
        read -p "Enter output file path: " output
        python3 src/client.py --file "$file" --output "$output"
        ;;
    3)
        echo "Running Admin client..."
        bin/admin_client
        ;;
    4)
        echo "Goodbye!"
        exit 0
        ;;
    *)
        echo "Invalid option!"
        ;;
esac


# chmod +x run_clients.sh
# ./run_clients.sh