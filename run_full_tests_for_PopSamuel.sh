#!/bin/bash

# Creare fisiere test

cat > hello.py <<EOF
print("hello world")
EOF

cat > error.c <<EOF
int main() { return undeclarata; }
EOF

cat > runtime_error.py <<EOF
raise Exception("runtime error test")
EOF

cat > sleep_test.py <<EOF
import time
time.sleep(5)
print("done sleeping")
EOF

echo "=== TEST 1: Verifica conexiune server (presupune serverul pornit) ==="
nc -zv 127.0.0.1 12345 && echo "Server UP" || { echo "Server DOWN"; exit 1; }

echo -e "\n=== TEST 2: Trimite cod Python corect ==="
./client --file hello.py --output result_hello.txt
echo "Output result_hello.txt:"
cat result_hello.txt

echo -e "\n=== TEST 3: Trimite cod C cu eroare compilare ==="
./client --file error.c --output result_error.txt
echo "Output result_error.txt:"
cat result_error.txt

echo -e "\n=== TEST 4: Trimite cod Python cu eroare la execuție ==="
./client --file runtime_error.py --output result_runtime.txt
echo "Output result_runtime.txt:"
cat result_runtime.txt

echo -e "\n=== TEST 5: Trimite 4 joburi sleep simultan pentru test coada joburi ==="
./client --file sleep_test.py --output result_sleep1.txt &
./client --file sleep_test.py --output result_sleep2.txt &
./client --file sleep_test.py --output result_sleep3.txt &
./client --file sleep_test.py --output result_sleep4.txt &
wait
echo "Sleep job outputs:"
cat result_sleep1.txt result_sleep2.txt result_sleep3.txt result_sleep4.txt

echo -e "\n=== TEST 6: Shutdown server prin admin_client ==="
./admin_client --shutdown

sleep 2

echo -e "\n=== Testele s-au terminat ==="

