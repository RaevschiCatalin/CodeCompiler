
import socket
import os
import sys
import argparse

class CodeCompilerClient:
    def __init__(self, host="127.0.0.1", port=12345):
        self.host = host
        self.port = port
        self.sock = None
    
    def connect(self):
        """Connect to the server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            print("Connected to server!")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from server"""
        if self.sock:
            self.sock.close()
            self.sock = None
            print("Disconnected from server")
    
    def send_file_and_run(self, filepath, outputpath):
        """Send file to server and get compilation/execution results"""
        # Check if file exists
        if not os.path.exists(filepath):
            print(f"Error: File {filepath} does not exist")
            return False
        
        # Get file info
        filesize = os.path.getsize(filepath)
        filename = os.path.basename(filepath)
        
        print(f"Sending file: {filename} ({filesize} bytes)")
        
        try:
            # Send UPLOAD header
            header = f"UPLOAD {filename} {filesize}\n"
            self.sock.send(header.encode())
            
            # Send file content
            with open(filepath, 'rb') as f:
                bytes_sent = 0
                while bytes_sent < filesize:
                    chunk = f.read(4096)
                    if not chunk:
                        break
                    self.sock.send(chunk)
                    bytes_sent += len(chunk)
            
            print("[>] File sent.")
            
            # Send RUN command
            self.sock.send(b"RUN\n")
            print("[>] RUN command sent.")
            
            # Receive result size
            size_line = b""
            while True:
                byte = self.sock.recv(1)
                if not byte:
                    print("Error: Connection lost while receiving size")
                    return False
                if byte == b'\n':
                    break
                size_line += byte
            
            result_size = int(size_line.decode().strip())
            print(f"[<] Result file size: {result_size}")
            
            # Receive result content
            with open(outputpath, 'wb') as outf:
                bytes_received = 0
                while bytes_received < result_size:
                    remaining = result_size - bytes_received
                    chunk_size = min(4096, remaining)
                    chunk = self.sock.recv(chunk_size)
                    if not chunk:
                        print("Error: Connection lost while receiving result")
                        return False
                    outf.write(chunk)
                    bytes_received += len(chunk)
            
            print(f"[<] Result file saved to {outputpath}")
            return True
            
        except Exception as e:
            print(f"Error during file transfer: {e}")
            return False

def main():
    parser = argparse.ArgumentParser(description='Code Compiler Client')
    parser.add_argument('--file', required=True, help='Path to source code file')
    parser.add_argument('--output', required=True, help='Path to save compilation/execution results')
    parser.add_argument('--host', default='127.0.0.1', help='Server host (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=12345, help='Server port (default: 12345)')
    
    args = parser.parse_args()
    
    # Create client
    client = CodeCompilerClient(args.host, args.port)
    
    # Connect to server
    if not client.connect():
        sys.exit(1)
    
    # Send file and get results
    success = client.send_file_and_run(args.file, args.output)
    
    # Disconnect
    client.disconnect()
    
    if success:
        print("Operation completed successfully!")
        sys.exit(0)
    else:
        print("Operation failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()