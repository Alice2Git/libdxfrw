#!/bin/bash
#
# Build script for libdxfrw
# This script is designed to run inside Docker containers
# and creates a distributable tar.gz archive
#

# Robust error handling
# -e: Exit on error
# -u: Exit on undefined variable
# -o pipefail: Fail on pipe errors
set -euo pipefail

echo "=========================================="
echo "libdxfrw Build Script"
echo "=========================================="

# Configuration
PREFIX="/opt/dxfrw"
OUTPUT_DIR="/work"
OUTPUT_FILE="dxfrw.tar.gz"

# Start timer
START_TIME=$(date +%s)

# Step 1: Configure build (CMake)
echo ""
echo "[1/6] Configuring build..."
rm -rf build-docker
cmake -S . -B build-docker -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${PREFIX}"

# Step 2: Build library and tools
echo ""
echo "[2/6] Building library..."
cmake --build build-docker -j"$(nproc)"

# Step 3: Run unit tests
echo ""
echo "[3/6] Running tests..."
ctest --test-dir build-docker --output-on-failure

# Step 4: Install to prefix
echo ""
echo "[4/6] Installing to ${PREFIX}..."
cmake --install build-docker

# Step 5: Copy additional binaries if they exist
echo ""
echo "[5/6] Copying additional binaries..."
if [ -d "bin" ] && [ "$(ls -A bin 2>/dev/null)" ]; then
    mkdir -p "${PREFIX}/bin"
    cp -v bin/* "${PREFIX}/bin/" || true
else
    echo "No additional binaries found in bin/ directory"
fi

# Step 6: Create distributable archive
echo ""
echo "[6/6] Creating distributable archive..."
cd /opt
tar czf "${OUTPUT_DIR}/${OUTPUT_FILE}" dxfrw

# Set permissions to allow host user access when running in Docker
# 666 (rw-rw-rw-) ensures the file can be accessed even if Docker runs as different UID
chmod 666 "${OUTPUT_DIR}/${OUTPUT_FILE}"

# Calculate elapsed time
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

# Show summary
echo ""
echo "=========================================="
echo "✓ Build completed successfully!"
echo "=========================================="
echo "Output file: ${OUTPUT_DIR}/${OUTPUT_FILE}"
echo "File size:   $(du -h "${OUTPUT_DIR}/${OUTPUT_FILE}" | cut -f1)"
echo "Build time:  ${ELAPSED} seconds"
echo "=========================================="
echo ""
echo "To extract the archive:"
echo "  tar xzf ${OUTPUT_FILE} -C /opt"
echo ""

# Clean up build artifacts
echo "Cleaning up build artifacts..."
cd /work
rm -rf build-docker
