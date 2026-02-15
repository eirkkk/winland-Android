cmake_minimum_required(VERSION 3.18.1)
project("winland-server")

# إعدادات البناء للأندرويد
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -O2 -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -O2 -Wall -Wextra -std=c++17")

# البحث عن المكتبات المطلوبة
find_library(log-lib log)
find_library(android-lib android)
find_library(EGL-lib EGL)
find_library(GLESv2-lib GLESv2)

# مسار ملفات Wayland (سيتم تضمينها كـ prebuilt)
set(WAYLAND_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../../../../../src/protocols/wayland")
set(WAYLAND_LIB_DIR "${CMAKE_SOURCE_DIR}/../../../../../libs/arm64-v8a")

# تضمين المسارات
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${WAYLAND_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/../../../../../src/native
    ${CMAKE_SOURCE_DIR}/../../../../../src/bridge
)

# إضافة المكتبات المسبقة البناء لـ Wayland
add_library(wayland-server SHARED IMPORTED)
set_target_properties(wayland-server PROPERTIES
    IMPORTED_LOCATION ${WAYLAND_LIB_DIR}/libwayland-server.so)

add_library(wayland-client SHARED IMPORTED)
set_target_properties(wayland-client PROPERTIES
    IMPORTED_LOCATION ${WAYLAND_LIB_DIR}/libwayland-client.so)

# خيارات البناء
option(WITH_DMABUF "Enable DMA-BUF support" ON)
option(WITH_VNC "Enable VNC server" ON)
option(WITH_TILING "Enable tiling window manager" ON)
option(WITH_DEBUG_OVERLAY "Enable debug overlay" ON)
option(WITH_ROOT_DAEMON "Enable root daemon support" ON)
option(WITH_XWAYLAND "Enable XWayland support" ON)
option(WITH_PACKAGE_MANAGER "Enable package manager" ON)
option(WITH_NOTIFICATIONS "Enable notification bridge" ON)
option(WITH_KEYBOARD "Enable keyboard manager" ON)
option(WITH_POWER "Enable power manager" ON)
option(WITH_MULTI_DISPLAY "Enable multi-display support" ON)
option(WITH_FILE_SHARING "Enable file sharing" ON)
option(WITH_WEBRTC "Enable WebRTC server" ON)

# ملفات المصدر الأصلية
set(NATIVE_SOURCES
    ${CMAKE_SOURCE_DIR}/../../../../../src/native/wayland_compositor.c
    ${CMAKE_SOURCE_DIR}/../../../../../src/native/egl_display.c
    ${CMAKE_SOURCE_DIR}/../../../../../src/native/input_handler.c
    ${CMAKE_SOURCE_DIR}/../../../../../src/native/surface_manager.c
    ${CMAKE_SOURCE_DIR}/../../../../../src/native/buffer_manager.c
    ${CMAKE_SOURCE_DIR}/../../../../../src/native/output_manager.c
    ${CMAKE_SOURCE_DIR}/../../../../../src/native/seat_manager.c
    ${CMAKE_SOURCE_DIR}/../../../../../src/native/xdg_shell_impl.c
)

# إضافة ملفات DMA-BUF
if(WITH_DMABUF)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/dmabuf_handler.c)
    add_definitions(-DENABLE_DMABUF=1)
endif()

# إضافة ملفات VNC
if(WITH_VNC)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/vnc_server.c)
    add_definitions(-DENABLE_VNC=1)
endif()

# إضافة ملفات Tiling
if(WITH_TILING)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/tiling_layout.c)
    add_definitions(-DENABLE_TILING=1)
endif()

# إضافة ملفات Debug Overlay
if(WITH_DEBUG_OVERLAY)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/debug_overlay.c)
    add_definitions(-DENABLE_DEBUG_OVERLAY=1)
endif()

# إضافة ملفات Root Daemon
if(WITH_ROOT_DAEMON)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/root_daemon.c)
    add_definitions(-DENABLE_ROOT_DAEMON=1)
endif()

# إضافة ملفات XWayland
if(WITH_XWAYLAND)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/xwayland.c)
    add_definitions(-DENABLE_XWAYLAND=1)
    # البحث عن مكتبات XCB
    find_library(xcb-lib xcb)
    find_library(xcb-ewmh-lib xcb-ewmh)
    find_library(xcb-icccm-lib xcb-icccm)
    find_library(xcb-composite-lib xcb-composite)
    find_library(xcb-damage-lib xcb-damage)
endif()

# إضافة ملفات Package Manager
if(WITH_PACKAGE_MANAGER)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/package_manager.c)
    add_definitions(-DENABLE_PACKAGE_MANAGER=1)
endif()

# إضافة ملفات Keyboard Manager
if(WITH_KEYBOARD)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/keyboard_manager.c)
    add_definitions(-DENABLE_KEYBOARD_MANAGER=1)
endif()

# إضافة ملفات Power Manager
if(WITH_POWER)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/power_manager.c)
    add_definitions(-DENABLE_POWER_MANAGER=1)
endif()

# إضافة ملفات Multi-Display
if(WITH_MULTI_DISPLAY)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/multi_display.c)
    add_definitions(-DENABLE_MULTI_DISPLAY=1)
endif()

# إضافة ملفات Display Filter
if(WITH_MULTI_DISPLAY)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/display_filter.c)
    add_definitions(-DENABLE_DISPLAY_FILTER=1)
endif()

# إضافة ملفات WebRTC
if(WITH_WEBRTC)
    list(APPEND NATIVE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/native/webrtc_server.c)
    add_definitions(-DENABLE_WEBRTC=1)
endif()

# ملفات الجسر
set(BRIDGE_SOURCES
    ${CMAKE_SOURCE_DIR}/../../../../../src/bridge/audio_bridge.c
    ${CMAKE_SOURCE_DIR}/../../../../../src/bridge/usb_redirect.c
    ${CMAKE_SOURCE_DIR}/../../../../../src/bridge/clipboard_bridge.c
)

# إضافة ملفات File Sharing
if(WITH_FILE_SHARING)
    list(APPEND BRIDGE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/bridge/file_sharing.c)
    add_definitions(-DENABLE_FILE_SHARING=1)
endif()

# إضافة ملفات Notification Bridge
if(WITH_NOTIFICATIONS)
    list(APPEND BRIDGE_SOURCES ${CMAKE_SOURCE_DIR}/../../../../../src/bridge/notification_bridge.c)
    add_definitions(-DENABLE_NOTIFICATIONS=1)
endif()

# المكتبة الرئيسية
add_library(
    winland-native
    SHARED
    native-lib.cpp
    ${NATIVE_SOURCES}
    ${BRIDGE_SOURCES}
)

# ربط المكتبات
set(LINK_LIBRARIES
    winland-native
    ${log-lib}
    ${android-lib}
    ${EGL-lib}
    ${GLESv2-lib}
    wayland-server
    wayland-client
)

# إضافة مكتبات XCB لـ XWayland
if(WITH_XWAYLAND)
    list(APPEND LINK_LIBRARIES
        ${xcb-lib}
        ${xcb-ewmh-lib}
        ${xcb-icccm-lib}
        ${xcb-composite-lib}
        ${xcb-damage-lib}
    )
endif()

target_link_libraries(${LINK_LIBRARIES})

# تجميع بروتوكولات Wayland
add_custom_target(wayland-protocols
    COMMAND ${CMAKE_COMMAND} -E echo "Generating Wayland protocols..."
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../../../../../src/protocols
)

add_dependencies(winland-native wayland-protocols)
