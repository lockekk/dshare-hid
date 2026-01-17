#!/bin/bash
set -e

# Version of OpenSSL to build
OPENSSL_VERSION="3.0.12"
OPENSSL_NAME="openssl-${OPENSSL_VERSION}"
OPENSSL_URL="https://www.openssl.org/source/${OPENSSL_NAME}.tar.gz"

# Directories
BASE_DIR="$(pwd)/deps/openssl-universal"
SRC_DIR="${BASE_DIR}/src"
BUILD_ARM64="${BASE_DIR}/arm64"
BUILD_X86_64="${BASE_DIR}/x86_64"
OUTPUT_DIR="${BASE_DIR}/universal"

# Create directories
mkdir -p "${SRC_DIR}" "${BUILD_ARM64}" "${BUILD_X86_64}" "${OUTPUT_DIR}/lib" "${OUTPUT_DIR}/include"

# 1. Download OpenSSL
if [ ! -f "${SRC_DIR}/${OPENSSL_NAME}.tar.gz" ]; then
    echo "Downloading OpenSSL ${OPENSSL_VERSION}..."
    curl -L "${OPENSSL_URL}" -o "${SRC_DIR}/${OPENSSL_NAME}.tar.gz"
fi

# 2. Extract
if [ ! -d "${SRC_DIR}/${OPENSSL_NAME}" ]; then
    echo "Extracting OpenSSL..."
    tar -xzf "${SRC_DIR}/${OPENSSL_NAME}.tar.gz" -C "${SRC_DIR}"
fi

# 3. Build for ARM64
if [ ! -f "${BUILD_ARM64}/lib/libssl.dylib" ]; then
    echo "Building OpenSSL for ARM64..."
    cd "${SRC_DIR}/${OPENSSL_NAME}"
    make distclean >/dev/null 2>&1 || true

    ./Configure darwin64-arm64-cc shared -DOPENSSL_NO_APPLE_CRYPTO_API_RANDOM --prefix="${BUILD_ARM64}" --openssldir="${BUILD_ARM64}"
    make -j$(sysctl -n hw.ncpu)
    make install_sw
fi

# 4. Build for x86_64
if [ ! -f "${BUILD_X86_64}/lib/libssl.dylib" ]; then
    echo "Building OpenSSL for x86_64..."
    cd "${SRC_DIR}/${OPENSSL_NAME}"
    make distclean >/dev/null 2>&1 || true

    ./Configure darwin64-x86_64-cc shared -DOPENSSL_NO_APPLE_CRYPTO_API_RANDOM --prefix="${BUILD_X86_64}" --openssldir="${BUILD_X86_64}"
    make -j$(sysctl -n hw.ncpu)
    make install_sw
fi

# 5. Create Universal Libraries (Lipo)
echo "Creating Universal Libraries..."
# Merge dylibs
lipo -create "${BUILD_ARM64}/lib/libssl.3.dylib" "${BUILD_X86_64}/lib/libssl.3.dylib" -output "${OUTPUT_DIR}/lib/libssl.3.dylib"
lipo -create "${BUILD_ARM64}/lib/libcrypto.3.dylib" "${BUILD_X86_64}/lib/libcrypto.3.dylib" -output "${OUTPUT_DIR}/lib/libcrypto.3.dylib"

# Create symlinks for versionless dylibs (optional but good for linking)
ln -sf libssl.3.dylib "${OUTPUT_DIR}/lib/libssl.dylib"
ln -sf libcrypto.3.dylib "${OUTPUT_DIR}/lib/libcrypto.dylib"

# Merge static libs as well (optional, but good to have)
lipo -create "${BUILD_ARM64}/lib/libssl.a" "${BUILD_X86_64}/lib/libssl.a" -output "${OUTPUT_DIR}/lib/libssl.a"
lipo -create "${BUILD_ARM64}/lib/libcrypto.a" "${BUILD_X86_64}/lib/libcrypto.a" -output "${OUTPUT_DIR}/lib/libcrypto.a"

# 6. Copy Includes (They are usually identical for headers, taking arm64)
cp -r "${BUILD_ARM64}/include" "${OUTPUT_DIR}/"

echo "Universal OpenSSL created at: ${OUTPUT_DIR}"
