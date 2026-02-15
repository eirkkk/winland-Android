#!/data/data/com.termux/files/usr/bin/bash

# سكربت إعداد Termux لـ Winland Server
# يقوم بتثبيت جميع الاعتماديات المطلوبة

set -e

# الألوان
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

# التحقق من Termux
if [ -z "$TERMUX_VERSION" ]; then
    error "This script must be run in Termux environment"
    exit 1
fi

info "Setting up Winland Server environment in Termux..."

# تحديث الحزم
info "Updating package lists..."
pkg update -y

# تثبيت الاعتماديات الأساسية
info "Installing basic dependencies..."
pkg install -y \
    git \
    cmake \
    ninja \
    clang \
    pkg-config \
    make \
    automake \
    autoconf \
    libtool \
    wget \
    curl

# تثبيت مكتبات Wayland
info "Installing Wayland libraries..."
pkg install -y \
    libwayland \
    libwayland-protocols \
    wayland-utils

# تثبيت مكتبات EGL/OpenGL
info "Installing graphics libraries..."
pkg install -y \
    mesa \
    libegl \
    libglesv2 \
    libglvnd

# تثبيت مكتبات الصوت
info "Installing audio libraries..."
pkg install -y \
    libpulseaudio \
    pulseaudio

# تثبيت مكتبات أخرى
info "Installing additional libraries..."
pkg install -y \
    libpixman \
    libxkbcommon \
    libpng \
    libjpeg-turbo \
    freetype \
    fontconfig \
    libicu

# إنشاء مجلد العمل
WORK_DIR="$HOME/winland-work"
info "Creating working directory: $WORK_DIR"
mkdir -p "$WORK_DIR"

# إعداد متغيرات البيئة
info "Setting up environment variables..."

# إنشاء ملف profile
PROFILE_FILE="$HOME/.winland_env"
cat > "$PROFILE_FILE" << 'EOF'
# Winland Server Environment Variables

# مسارات المكتبات
export LD_LIBRARY_PATH=$PREFIX/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH
export CMAKE_PREFIX_PATH=$PREFIX:$CMAKE_PREFIX_PATH

# إعدادات Wayland
export XDG_RUNTIME_DIR=$PREFIX/tmp
export WAYLAND_DISPLAY=wayland-0

# إعدادات EGL
export EGL_PLATFORM=surfaceless
export MESA_GL_VERSION_OVERRIDE=3.0
export MESA_GLSL_VERSION_OVERRIDE=130

# إعدادات الصوت
export PULSE_SERVER=127.0.0.1
export PULSE_LATENCY_MSEC=60

# مسار Winland
export WINLAND_PREFIX=$PREFIX
EOF

# إضافة إلى .bashrc إذا لم يكن موجوداً
if ! grep -q "source $PROFILE_FILE" "$HOME/.bashrc" 2>/dev/null; then
    echo "" >> "$HOME/.bashrc"
    echo "# Winland Server environment" >> "$HOME/.bashrc"
    echo "source $PROFILE_FILE" >> "$HOME/.bashrc"
fi

success "Environment setup complete!"

# إنشاء سكربت تشغيل
RUN_SCRIPT="$HOME/run-winland.sh"
cat > "$RUN_SCRIPT" << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash

# سكربت تشغيل Winland Server

source "$HOME/.winland_env"

# التحقق من وجود الخادم
if [ ! -f "./winland-server" ]; then
    echo "Error: winland-server not found"
    echo "Please build the project first"
    exit 1
fi

# إنشاء مجلد runtime
mkdir -p "$XDG_RUNTIME_DIR"

# تشغيل الخادم
echo "Starting Winland Server..."
echo "XDG_RUNTIME_DIR: $XDG_RUNTIME_DIR"
echo "WAYLAND_DISPLAY: $WAYLAND_DISPLAY"

./winland-server "$@"
EOF

chmod +x "$RUN_SCRIPT"

# إنشاء سكربت البناء
BUILD_SCRIPT="$HOME/build-winland.sh"
cat > "$BUILD_SCRIPT" << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash

# سكربت بناء Winland Server

set -e

source "$HOME/.winland_env"

PROJECT_DIR="${1:-.}"
BUILD_DIR="$PROJECT_DIR/build"

info() {
    echo "[INFO] $1"
}

error() {
    echo "[ERROR] $1"
    exit 1
}

info "Building Winland Server..."
info "Project directory: $PROJECT_DIR"

# إنشاء مجلد البناء
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# تشغيل CMake
info "Running CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    || error "CMake failed"

# البناء
info "Building..."
cmake --build . --parallel $(nproc) || error "Build failed"

success() {
    echo "[SUCCESS] $1"
}

success "Build complete!"
success "Binary location: $BUILD_DIR/winland-server"
EOF

chmod +x "$BUILD_SCRIPT"

# تعليمات إضافية
cat << 'EOF'

========================================
    Winland Server Setup Complete!
========================================

The following components have been installed:
  - Wayland libraries
  - Mesa/EGL graphics libraries
  - Audio libraries (PulseAudio)
  - Build tools (CMake, Ninja, Clang)
  - Additional dependencies

Environment variables have been configured.
Please restart Termux or run:
  source ~/.winland_env

Useful scripts created:
  - ~/build-winland.sh  : Build the project
  - ~/run-winland.sh    : Run the server

To build Winland Server:
  1. Navigate to the project directory
  2. Run: ~/build-winland.sh

To run Winland Server:
  1. Navigate to the build directory
  2. Run: ~/run-winland.sh

For Android Studio development:
  1. Open the project in Android Studio
  2. Sync Gradle files
  3. Build the project

========================================
EOF

success "Setup complete!"
