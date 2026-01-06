#!/bin/bash

#############################################################################
# PokerTH Test Runner Script
# This script builds and runs all PokerTH unit tests
#
# Usage: ./run_tests.sh [--build-only] [--test-only] [--verbose]
#############################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

VERBOSE=false
BUILD_ONLY=false
TEST_ONLY=false

# Parse arguments
for arg in "$@"; do
    case $arg in
        --build-only)
            BUILD_ONLY=true
            ;;
        --test-only)
            TEST_ONLY=true
            ;;
        --verbose)
            VERBOSE=true
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --build-only   Only build the tests, don't run them"
            echo "  --test-only    Only run existing tests, don't rebuild"
            echo "  --verbose      Show verbose output"
            echo "  --help, -h     Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                    # Build and run all tests"
            echo "  $0 --build-only       # Only build tests"
            echo "  $0 --test-only        # Only run tests (requires pre-built tests)"
            exit 0
            ;;
    esac
done

echo "================================================"
echo "PokerTH Test Runner"
echo "================================================"

cd "${BUILD_DIR}"

# Build tests
if [ "$TEST_ONLY" = false ]; then
    echo ""
    echo "Building PokerTH tests..."
    echo "================================================"
    
    if [ "$VERBOSE" = true ]; then
        cmake .. -DCMAKE_BUILD_TYPE=Release -DGUI_800_480=OFF 2>&1 | head -50
    else
        cmake .. -DCMAKE_BUILD_TYPE=Release -DGUI_800_480=OFF > /dev/null 2>&1
    fi
    
    if [ "$VERBOSE" = true ]; then
        make pokerth_tests --parallel $(nproc)
    else
        make pokerth_tests --parallel $(nproc) > /dev/null 2>&1
    fi
    
    echo "Build completed successfully!"
fi

# Run tests
if [ "$BUILD_ONLY" = false ]; then
    echo ""
    echo "Running PokerTH unit tests..."
    echo "================================================"
    
    TEST_EXE="${BUILD_DIR}/bin/pokerth_tests"
    
    if [ -f "$TEST_EXE" ]; then
        if [ "$VERBOSE" = true ]; then
            "$TEST_EXE"
        else
            "$TEST_EXE"
        fi
        
        EXIT_CODE=$?
        
        if [ $EXIT_CODE -eq 0 ]; then
            echo ""
            echo "================================================"
            echo "All tests PASSED!"
            echo "================================================"
            exit 0
        else
            echo ""
            echo "================================================"
            echo "Tests FAILED with exit code: $EXIT_CODE"
            echo "================================================"
            exit $EXIT_CODE
        fi
    else
        echo "Error: Test executable not found at $TEST_EXE"
        echo "Please run with --build-only first to build the tests"
        exit 1
    fi
fi

echo ""
echo "================================================"
echo "Build completed successfully (tests not run)"
echo "================================================"
