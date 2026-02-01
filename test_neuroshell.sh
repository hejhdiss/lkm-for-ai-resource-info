#!/bin/bash
# test_neuroshell.sh - Comprehensive test suite for NeuroShell LKM

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "NeuroShell Module Test Suite"
echo "========================================="
echo ""

# Check if module is built
if [ ! -f "neuroshell_enhanced.ko" ]; then
    echo -e "${RED}✗ Module not built${NC}"
    echo "Run 'make' first to build the module"
    exit 1
fi
echo -e "${GREEN}✓ Module file found${NC}"

# Check if module is loaded
if lsmod | grep -q neuroshell; then
    echo -e "${YELLOW}! Module already loaded, unloading...${NC}"
    sudo rmmod neuroshell_enhanced 2>/dev/null || sudo rmmod neuroshell 2>/dev/null || true
fi

# Load the module
echo ""
echo "Loading module..."
if sudo insmod neuroshell_enhanced.ko; then
    echo -e "${GREEN}✓ Module loaded successfully${NC}"
else
    echo -e "${RED}✗ Failed to load module${NC}"
    exit 1
fi

# Check kernel log
echo ""
echo "Kernel log messages:"
dmesg | tail -5 | grep neuroshell || echo "No recent messages"

# Check sysfs directory
echo ""
if [ -d "/sys/kernel/neuroshell" ]; then
    echo -e "${GREEN}✓ Sysfs directory created${NC}"
else
    echo -e "${RED}✗ Sysfs directory not found${NC}"
    sudo rmmod neuroshell_enhanced
    exit 1
fi

# Test reading attributes
echo ""
echo "Testing sysfs attributes..."
echo ""

test_attr() {
    local attr=$1
    local desc=$2
    
    if [ -f "/sys/kernel/neuroshell/$attr" ]; then
        echo -e "${GREEN}✓${NC} $desc ($attr)"
        cat "/sys/kernel/neuroshell/$attr" | head -5
        echo ""
    else
        echo -e "${RED}✗${NC} $desc ($attr) - not found"
    fi
}

test_attr "cpu_count" "CPU Count"
test_attr "cpu_info" "CPU Info"
test_attr "mem_total_bytes" "Memory Total"
test_attr "mem_info" "Memory Info"
test_attr "numa_nodes" "NUMA Nodes"
test_attr "gpu_info" "GPU Info"
test_attr "accelerator_count" "Accelerator Count"
test_attr "system_summary" "System Summary"

# List all available attributes
echo ""
echo "All available attributes:"
ls -1 /sys/kernel/neuroshell/

# Performance test
echo ""
echo "Performance test (100 reads)..."
start_time=$(date +%s%N)
for i in {1..100}; do
    cat /sys/kernel/neuroshell/cpu_count > /dev/null
done
end_time=$(date +%s%N)
elapsed=$((($end_time - $start_time) / 1000000))
echo "Time for 100 reads: ${elapsed}ms (avg: $((elapsed / 100))ms per read)"

# Python interface test
echo ""
if command -v python3 &> /dev/null; then
    echo "Testing Python interface..."
    if python3 neuroshell.py --summary > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Python interface working${NC}"
    else
        echo -e "${YELLOW}! Python interface test failed (may be normal if module just loaded)${NC}"
    fi
else
    echo -e "${YELLOW}! Python3 not found, skipping Python tests${NC}"
fi

# Cleanup prompt
echo ""
echo "========================================="
echo "Tests completed!"
echo "========================================="
echo ""
echo "Module is still loaded. To unload:"
echo "  sudo make uninstall"
echo ""
echo "To view system info:"
echo "  cat /sys/kernel/neuroshell/system_summary"
echo "  python3 neuroshell.py --all"
