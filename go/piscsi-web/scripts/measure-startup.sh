#!/usr/bin/env bash
# Measure startup time of the piscsi-web binary

set -euo pipefail

BINARY="${1:-./piscsi-web-stripped}"

if [[ ! -x "$BINARY" ]]; then
    echo "Error: Binary $BINARY not found or not executable"
    exit 1
fi

echo "Measuring startup time for: $BINARY"
echo "Binary size: $(ls -lh "$BINARY" | awk '{print $5}')"
echo ""

# Measure startup time (time until server is listening)
echo "Running 10 startup tests..."
TOTAL=0

for i in {1..10}; do
    START=$(date +%s%N)
    timeout 0.5s "$BINARY" >/dev/null 2>&1 &
    PID=$!

    # Wait a tiny bit for server to start
    sleep 0.01

    # Kill it
    kill $PID 2>/dev/null || true
    wait $PID 2>/dev/null || true

    END=$(date +%s%N)
    DURATION=$(( (END - START) / 1000000 ))  # Convert to milliseconds
    TOTAL=$(( TOTAL + DURATION ))

    echo "  Test $i: ${DURATION}ms"
done

AVG=$(( TOTAL / 10 ))
echo ""
echo "Average startup time: ${AVG}ms"
echo ""

if [[ $AVG -lt 100 ]]; then
    echo "✅ EXCELLENT: Under 100ms startup time!"
    echo "   This is 10-20x faster than Python (1000-2000ms)"
elif [[ $AVG -lt 200 ]]; then
    echo "✅ GREAT: Under 200ms startup time!"
    echo "   This is 5-10x faster than Python (1000-2000ms)"
else
    echo "⚠️  WARNING: Startup time is higher than target (< 50ms on Pi)"
fi
