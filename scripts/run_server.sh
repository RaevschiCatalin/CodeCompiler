#!/bin/bash

echo "Starting servers..."

# Start main server in background
echo "Starting main server on port 12345..."
bin/server &
SERVER_PID=$!

# Start admin server in background  
echo "Starting admin server on unix socket..."
bin/admin_server &
ADMIN_PID=$!

echo "Both servers started!"
echo "Main server PID: $SERVER_PID"
echo "Admin server PID: $ADMIN_PID"
echo "Press Ctrl+C to stop both servers"

# Wait for Ctrl+C
trap 'kill $SERVER_PID $ADMIN_PID; exit' INT
wait

# chmod +x run_server.sh
# ./run_servers.sh