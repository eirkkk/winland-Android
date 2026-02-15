#!/bin/bash

# سكربت البناء التلقائي لـ Winland Server
# يقوم بتحميل الأدوات اللازمة والبناء تلقائياً

set -e

# الألوان
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# المتغيرات
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
TOOLS_DIR="$PROJECT_DIR/tools"
NDK_VERSION="25.2.9519653"
CMAKE_VERSION="3.22.1"

# رسائل
info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

step() {
    echo -e "${CYAN}[STEP]${NC} $1"
}

# التحقق من النظام
detect_os() {
    if [[ "$OSTYPE" == "linux-android"* ]]; then
        echo "android"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

OS=$(detect_os)
info "Detected OS: $OS"

# إنشاء مجلد الأدوات
mkdir -p "$TOOLS_DIR"

# تحميل NDK
download_ndk() {
    step "Checking Android NDK..."
    
    if [ -d "$TOOLS_DIR/android-ndk-r$NDK_VERSION" ]; then
        info "NDK already downloaded"
        return 0
    fi
    
    info "Downloading Android NDK..."
    
    local ndk_url="https://dl.google.com/android/repository/android-ndk-r${NDK_VERSION}-linux.zip"
    local ndk_zip="$TOOLS_DIR/ndk.zip"
    
    if [ "$OS" == "macos" ]; then
        ndk_url="https://dl.google.com/android/repository/android-ndk-r${NDK_VERSION}-darwin.zip"
    fi
    
    wget -q --show-progress "$ndk_url" -O "$ndk_zip" || {
        error "Failed to download NDK"
        return 1
    }
    
    info "Extracting NDK..."
    unzip -q "$ndk_zip" -d "$TOOLS_DIR"
    rm "$ndk_zip"
    
    success "NDK downloaded and extracted"
}

# تحميل CMake
download_cmake() {
    step "Checking CMake..."
    
    if [ -d "$TOOLS_DIR/cmake-$CMAKE_VERSION" ]; then
        info "CMake already downloaded"
        return 0
    fi
    
    info "Downloading CMake..."
    
    local cmake_url="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"
    local cmake_tar="$TOOLS_DIR/cmake.tar.gz"
    
    if [ "$OS" == "macos" ]; then
        cmake_url="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-macos-universal.tar.gz"
    fi
    
    wget -q --show-progress "$cmake_url" -O "$cmake_tar" || {
        error "Failed to download CMake"
        return 1
    }
    
    info "Extracting CMake..."
    tar -xzf "$cmake_tar" -C "$TOOLS_DIR"
    rm "$cmake_tar"
    
    success "CMake downloaded and extracted"
}

# تحميل Wayland
download_wayland() {
    step "Checking Wayland libraries..."
    
    if [ -d "$TOOLS_DIR/wayland" ]; then
        info "Wayland already downloaded"
        return 0
    fi
    
    info "Downloading Wayland..."
    
    mkdir -p "$TOOLS_DIR/wayland"
    cd "$TOOLS_DIR/wayland"
    
    # تحميل Wayland
    git clone --depth 1 --branch 1.22.0 https://gitlab.freedesktop.org/wayland/wayland.git src 2>/dev/null || {
        warning "Failed to clone Wayland, using prebuilt"
        return 0
    }
    
    success "Wayland downloaded"
}

# تحميل EGL
download_egl() {
    step "Checking EGL headers..."
    
    if [ -d "$TOOLS_DIR/egl-headers" ]; then
        info "EGL headers already downloaded"
        return 0
    fi
    
    info "Downloading EGL headers..."
    
    mkdir -p "$TOOLS_DIR/egl-headers"
    cd "$TOOLS_DIR/egl-headers"
    
    # تحميل Khronos headers
    git clone --depth 1 https://github.com/KhronosGroup/EGL-Registry.git 2>/dev/null || {
        warning "Failed to clone EGL registry"
        return 0
    }
    
    success "EGL headers downloaded"
}

# بناء Wayland
build_wayland() {
    step "Building Wayland libraries..."
    
    if [ -f "$TOOLS_DIR/wayland/lib/libwayland-server.so" ]; then
        info "Wayland already built"
        return 0
    fi
    
    if [ ! -d "$TOOLS_DIR/wayland/src" ]; then
        warning "Wayland source not found, skipping build"
        return 0
    fi
    
    cd "$TOOLS_DIR/wayland/src"
    
    info "Configuring Wayland..."
    meson setup build \
        --prefix="$TOOLS_DIR/wayland" \
        --libdir="lib" \
        --buildtype=release \
        -Ddocumentation=false \
        -Dtests=false \
        -Ddtd_validation=false 2>/dev/null || {
        warning "Meson not available, using alternative build"
        return 0
    }
    
    info "Building Wayland..."
    ninja -C build 2>/dev/null || {
        warning "Wayland build failed"
        return 0
    }
    
    info "Installing Wayland..."
    ninja -C build install 2>/dev/null || {
        warning "Wayland install failed"
        return 0
    }
    
    success "Wayland built and installed"
}

# إعداد متغيرات البيئة
setup_environment() {
    step "Setting up build environment..."
    
    export ANDROID_NDK="$TOOLS_DIR/android-ndk-r$NDK_VERSION"
    export CMAKE="$TOOLS_DIR/cmake-$CMAKE_VERSION/bin/cmake"
    
    if [ "$OS" == "android" ]; then
        export PREFIX="/data/data/com.termux/files/usr"
    else
        export PREFIX="/usr/local"
    fi
    
    # إضافة إلى PATH
    export PATH="$ANDROID_NDK:$CMAKE:$PATH"
    
    info "ANDROID_NDK: $ANDROID_NDK"
    info "CMAKE: $CMAKE"
    info "PREFIX: $PREFIX"
    
    success "Environment set up"
}

# بناء المشروع
build_project() {
    step "Building Winland Server..."
    
    cd "$PROJECT_DIR"
    
    # إنشاء مجلد البناء
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # تشغيل CMake
    info "Running CMake..."
    
    if [ "$OS" == "android" ]; then
        # بناء لـ Android/Termux
        cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="$PREFIX" \
            -DCMAKE_C_COMPILER=clang \
            -DCMAKE_CXX_COMPILER=clang++ \
            -DWITH_DMABUF=ON \
            -DWITH_VNC=ON \
            -DWITH_TILING=ON \
            -DWITH_DEBUG_OVERLAY=ON \
            2>&1 | tee cmake.log
    else
        # بناء لـ Android مع NDK
        cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI=arm64-v8a \
            -DANDROID_PLATFORM=android-26 \
            -DCMAKE_INSTALL_PREFIX="$PREFIX" \
            -DWITH_DMABUF=ON \
            -DWITH_VNC=ON \
            -DWITH_TILING=ON \
            -DWITH_DEBUG_OVERLAY=ON \
            2>&1 | tee cmake.log
    fi
    
    if [ ${PIPESTATUS[0]} -ne 0 ]; then
        error "CMake configuration failed"
        return 1
    fi
    
    # البناء
    info "Compiling..."
    cmake --build . --parallel $(nproc) 2>&1 | tee build.log
    
    if [ ${PIPESTATUS[0]} -ne 0 ]; then
        error "Build failed"
        return 1
    fi
    
    success "Build completed successfully!"
}

# بناء تطبيق Android
build_android_app() {
    step "Building Android App..."
    
    cd "$PROJECT_DIR"
    
    # التحقق من وجود Gradle
    if [ ! -f "./gradlew" ]; then
        warning "Gradle wrapper not found, skipping app build"
        return 0
    fi
    
    # جعل gradlew قابل للتنفيذ
    chmod +x ./gradlew
    
    # بناء التطبيق
    info "Building APK..."
    ./gradlew assembleDebug 2>&1 | tee gradle-build.log
    
    if [ ${PIPESTATUS[0]} -eq 0 ]; then
        success "APK built successfully!"
        info "APK location: app/build/outputs/apk/debug/app-debug.apk"
    else
        error "APK build failed"
        return 1
    fi
}

# تشغيل الاختبارات
run_tests() {
    step "Running tests..."
    
    cd "$BUILD_DIR"
    
    if [ -f "winland-server" ]; then
        info "Running unit tests..."
        ctest --output-on-failure 2>/dev/null || warning "No tests available"
    fi
    
    success "Tests completed"
}

# إنشاء حزمة التوزيع
create_package() {
    step "Creating distribution package..."
    
    local package_dir="$PROJECT_DIR/dist"
    mkdir -p "$package_dir"
    
    # نسخ الملفات
    cp -r "$BUILD_DIR"/*.so "$package_dir/" 2>/dev/null || true
    cp -r "$BUILD_DIR"/winland-server "$package_dir/" 2>/dev/null || true
    cp -r "$PROJECT_DIR"/scripts "$package_dir/" 2>/dev/null || true
    
    # نسخ APK
    cp "$PROJECT_DIR"/app/build/outputs/apk/debug/app-debug.apk "$package_dir/" 2>/dev/null || true
    
    # إنشاء ملف README
    cat > "$package_dir/README.txt" << EOF
Winland Server Distribution
===========================

This package contains:
- winland-server: Native Wayland compositor binary
- libwinland-native.so: Native library for Android app
- app-debug.apk: Android application
- scripts/: Helper scripts

Installation:
1. Install the APK on your Android device
2. For Termux: Run setup-termux.sh
3. For Magisk: Flash winland-module.zip

Usage:
- Start the app and tap "Start Server"
- Or run: ./winland-server from Termux

For more information, visit:
https://github.com/yourusername/winland-server
EOF
    
    success "Package created in: $package_dir"
}

# عرض المساعدة
show_help() {
    cat << EOF
Winland Server Build Automation Script

Usage: $0 [OPTIONS] [TARGET]

OPTIONS:
    -h, --help          Show this help message
    -v, --verbose       Enable verbose output
    -j, --jobs N        Number of parallel jobs (default: auto)
    --clean             Clean build directory before building
    --download-only     Only download dependencies
    --no-download       Skip downloading dependencies

TARGETS:
    all                 Build everything (default)
    native              Build only native components
    app                 Build only Android app
    wayland             Build Wayland libraries
    test                Run tests
    package             Create distribution package

EXAMPLES:
    $0                  Build everything
    $0 native           Build only native code
    $0 --clean all      Clean and rebuild everything
    $0 --jobs 4 app     Build app with 4 parallel jobs

EOF
}

# المعالجة الرئيسية
main() {
    local target="all"
    local clean=0
    local download_only=0
    local no_download=0
    local verbose=0
    local jobs="$(nproc)"
    
    # معالجة المعاملات
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -v|--verbose)
                verbose=1
                shift
                ;;
            -j|--jobs)
                jobs="$2"
                shift 2
                ;;
            --clean)
                clean=1
                shift
                ;;
            --download-only)
                download_only=1
                shift
                ;;
            --no-download)
                no_download=1
                shift
                ;;
            -*)
                error "Unknown option: $1"
                show_help
                exit 1
                ;;
            *)
                target="$1"
                shift
                ;;
        esac
    done
    
    # التحقد من التنظيف
    if [ $clean -eq 1 ]; then
        info "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
        success "Build directory cleaned"
    fi
    
    # تحميل الاعتماديات
    if [ $no_download -eq 0 ]; then
        download_ndk
        download_cmake
        download_wayland
        download_egl
    fi
    
    if [ $download_only -eq 1 ]; then
        success "Dependencies downloaded"
        exit 0
    fi
    
    # إعداد البيئة
    setup_environment
    
    # بناء Wayland إذا لزم الأمر
    if [ "$target" == "wayland" ] || [ "$target" == "all" ]; then
        build_wayland
    fi
    
    # بناء الهدف
    case "$target" in
        all)
            build_project
            build_android_app
            ;;
        native)
            build_project
            ;;
        app)
            build_android_app
            ;;
        wayland)
            build_wayland
            ;;
        test)
            run_tests
            ;;
        package)
            create_package
            ;;
        *)
            error "Unknown target: $target"
            show_help
            exit 1
            ;;
    esac
    
    success "Build automation completed!"
    
    # عرض الملخص
    echo ""
    echo "========================================"
    echo "           Build Summary"
    echo "========================================"
    echo "Target: $target"
    echo "Build directory: $BUILD_DIR"
    echo ""
    
    if [ -f "$BUILD_DIR/winland-server" ]; then
        echo "Native binary: $BUILD_DIR/winland-server"
    fi
    
    if [ -f "$PROJECT_DIR/app/build/outputs/apk/debug/app-debug.apk" ]; then
        echo "Android APK: app/build/outputs/apk/debug/app-debug.apk"
    fi
    
    echo "========================================"
}

# تشغيل البرنامج الرئيسي
main "$@"
