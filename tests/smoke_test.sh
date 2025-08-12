#!/usr/bin/env bash
set -e

make all

# Run direttore for a brief moment to ensure setup/cleanup runs
./bin/direttore & 
PID=$!
sleep 1
kill $PID

echo "Smoke test passed"
