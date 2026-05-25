#!/bin/bash
# Build script for Winland Server on ARM64

set -e

# Load Rust environment
source "$HOME/.cargo/env"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}🔨 Winland Server - ARM64 Build${NC}"
echo "==========================================="

# Check Rust installation
echo -e "${YELLOW}✓ Checking Rust environment...${NC}"
rustc --version
cargo --version

# Check Android targets
echo -e "${YELLOW}✓ Checking ARM64 target...${NC}"
if ! rustup target list | grep -q "aarch64-linux-android (installed)"; then
    echo -e "${RED}✗ ARM64 target not installed${NC}"
    echo "Installing ARM64 target..."
    rustup target add aarch64-linux-android
fi

cd "$(dirname "$0")"

export CC_aarch64_linux_android=/opt/android/ndk/r29/toolchains/llvm/prebuilt/linux-aarch64/bin/aarch64-linux-android31-clang
export CXX_aarch64_linux_android=/opt/android/ndk/r29/toolchains/llvm/prebuilt/linux-aarch64/bin/aarch64-linux-android31-clang++

# Build Rust native library
echo -e "${YELLOW}📦 Building Rust native library (ARM64)...${NC}"
cd native

cargo build \
    --release \
    --lib \
    --target aarch64-linux-android \
    --features smithay_android \
    --jobs 4

echo -e "${GREEN}✓ Rust build completed!${NC}"

# Copy native library to JNI directory
echo -e "${YELLOW}📦 Copying native library to JNI...${NC}"
mkdir -p ../app/src/main/jniLibs/arm64-v8a
rm -f ../app/src/main/jniLibs/arm64-v8a/libwinland_core.so
cp target/aarch64-linux-android/release/libuniffi_winland_core.so  ../app/src/main/jniLibs/arm64-v8a/

echo -e "${GREEN}✓ Native library copied!${NC}"

# Build Android APK
echo -e "${YELLOW}🔨 Building Android APK...${NC}"
cd ..
./gradlew clean assembleDebug

echo -e "${GREEN}✓ Build completed successfully!${NC}"
echo "==========================================="
echo -e "${GREEN}APK location: ${NC}app/build/outputs/apk/"
cp -r /root/winland-Android/app/build/outputs/apk/debug/*  /sdcard
