#!/bin/bash

# [SEQUENCE: 1] PvP Test Runner Script

echo "==================================="
echo "Running PvP System Test Scenarios"
echo "==================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# [SEQUENCE: 2] Build tests if needed
if [ "$1" == "--build" ]; then
    echo -e "${YELLOW}Building PvP tests...${NC}"
    cd ../..
    mkdir -p build
    cd build
    cmake .. -DBUILD_TESTS=ON
    make arena_test_scenarios openworld_test_scenarios pvp_integration_test
    cd ../tests/pvp
fi

# [SEQUENCE: 3] Run arena tests
echo -e "\n${YELLOW}Running Arena PvP Tests...${NC}"
if ../../build/tests/pvp/arena_test_scenarios; then
    echo -e "${GREEN}✓ Arena tests passed${NC}"
else
    echo -e "${RED}✗ Arena tests failed${NC}"
    exit 1
fi

# [SEQUENCE: 4] Run open world tests
echo -e "\n${YELLOW}Running Open World PvP Tests...${NC}"
if ../../build/tests/pvp/openworld_test_scenarios; then
    echo -e "${GREEN}✓ Open World tests passed${NC}"
else
    echo -e "${RED}✗ Open World tests failed${NC}"
    exit 1
fi

# [SEQUENCE: 5] Run integration tests
echo -e "\n${YELLOW}Running PvP Integration Tests...${NC}"
if ../../build/tests/pvp/pvp_integration_test; then
    echo -e "${GREEN}✓ Integration tests passed${NC}"
else
    echo -e "${RED}✗ Integration tests failed${NC}"
    exit 1
fi

# [SEQUENCE: 6] Summary
echo -e "\n${GREEN}==================================="
echo "All PvP tests passed successfully!"
echo "===================================${NC}"

# [SEQUENCE: 7] Optional: Run with valgrind for memory checks
if [ "$1" == "--memcheck" ]; then
    echo -e "\n${YELLOW}Running memory checks...${NC}"
    valgrind --leak-check=full --error-exitcode=1 \
        ../../build/tests/pvp/pvp_integration_test
fi