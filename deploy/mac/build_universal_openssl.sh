#!/bin/bash
set -e

# Configuration
OPENSSL_VERSION="3.6.0"
WORK_DIR="build_openssl_universal"
OUTPUT_DIR="rel_openssl_universal"

# Clean up previous
rm -rf "$WORK_DIR" "$OUTPUT_DIR"
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

echo "Downloading OpenSSL ${OPENSSL_VERSION}..."
curl -LO "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"

tar xzf "openssl-${OPENSSL_VERSION}.tar.gz"

build_arch() {
    ARCH=$1
    TARGET=$2
    PREFIX="$(pwd)/../$OUTPUT_DIR/$ARCH"

    echo "--- Building for $ARCH ($TARGET) ---"

    cp -r "openssl-${OPENSSL_VERSION}" "openssl-$ARCH"
    pushd "openssl-$ARCH"

    ./Configure "$TARGET" no-shared --prefix="$PREFIX" --openssldir="$PREFIX/ssl"
    make -j$(sysctl -n hw.ncpu)
    make install_sw

    popd
}

# Build for both architectures
build_arch "arm64" "darwin64-arm64-cc"
build_arch "x86_64" "darwin64-x86_64-cc"

# Create Universal Binary
UNIVERSAL_DIR="$(pwd)/../$OUTPUT_DIR/universal"
mkdir -p "$UNIVERSAL_DIR/lib" "$UNIVERSAL_DIR/include"

echo "--- Creating Universal Binaries ---"

# Copy includes (strictly speaking identical for these archs usually, but take arm64)
cp -r "$(pwd)/../$OUTPUT_DIR/arm64/include/" "$UNIVERSAL_DIR/include/"

# Lipo libraries
lipo -create \
    "$(pwd)/../$OUTPUT_DIR/arm64/lib/libssl.a" \
    "$(pwd)/../$OUTPUT_DIR/x86_64/lib/libssl.a" \
    -output "$UNIVERSAL_DIR/lib/libssl.a"

lipo -create \
    "$(pwd)/../$OUTPUT_DIR/arm64/lib/libcrypto.a" \
    "$(pwd)/../$OUTPUT_DIR/x86_64/lib/libcrypto.a" \
    -output "$UNIVERSAL_DIR/lib/libcrypto.a"

# Create CMake config helper
cat <<EOF > "$UNIVERSAL_DIR/openssl-config.cmake"
set(OPENSSL_ROOT_DIR "\${CMAKE_CURRENT_LIST_DIR}")
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(OPENSSL_INCLUDE_DIR "\${OPENSSL_ROOT_DIR}/include")
set(OPENSSL_CRYPTO_LIBRARY "\${OPENSSL_ROOT_DIR}/lib/libcrypto.a")
set(OPENSSL_SSL_LIBRARY "\${OPENSSL_ROOT_DIR}/lib/libssl.a")
EOF

echo "Done! Universal OpenSSL available at: $UNIVERSAL_DIR"
