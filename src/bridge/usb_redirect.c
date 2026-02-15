#include "usb_redirect.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <android/log.h>

#define LOG_TAG "UsbRedirect"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define MAX_USB_DEVICES 32
#define USB_BUFFER_SIZE 65536

// المتغيرات العامة
static struct usb_redirect g_usb = {0};
static pthread_t g_usb_thread;
static pthread_mutex_t g_usb_mutex = PTHREAD_MUTEX_INITIALIZER;

// نوع الجهاز من معرفات الفئة
static usb_device_type_t get_device_type(uint8_t device_class) {
    switch (device_class) {
        case 0x00: return USB_DEVICE_TYPE_UNKNOWN;      // Use interface class
        case 0x01: return USB_DEVICE_TYPE_AUDIO;        // Audio
        case 0x02: return USB_DEVICE_TYPE_NETWORK;      // Communications
        case 0x03: return USB_DEVICE_TYPE_HID;          // HID
        case 0x05: return USB_DEVICE_TYPE_OTHER;        // Physical
        case 0x06: return USB_DEVICE_TYPE_OTHER;        // Image
        case 0x07: return USB_DEVICE_TYPE_PRINTER;      // Printer
        case 0x08: return USB_DEVICE_TYPE_STORAGE;      // Mass Storage
        case 0x09: return USB_DEVICE_TYPE_HID;          // Hub
        case 0x0A: return USB_DEVICE_TYPE_OTHER;        // CDC-Data
        case 0x0B: return USB_DEVICE_TYPE_OTHER;        // Smart Card
        case 0x0D: return USB_DEVICE_TYPE_OTHER;        // Content Security
        case 0x0E: return USB_DEVICE_TYPE_VIDEO;        // Video
        case 0x0F: return USB_DEVICE_TYPE_OTHER;        // Personal Healthcare
        case 0x10: return USB_DEVICE_TYPE_AUDIO;        // Audio/Video
        case 0x11: return USB_DEVICE_TYPE_OTHER;        // Billboard
        case 0x12: return USB_DEVICE_TYPE_OTHER;        // USB Type-C Bridge
        case 0xDC: return USB_DEVICE_TYPE_OTHER;        // Diagnostic Device
        case 0xE0: return USB_DEVICE_TYPE_OTHER;        // Wireless Controller
        case 0xEF: return USB_DEVICE_TYPE_OTHER;        // Miscellaneous
        case 0xFE: return USB_DEVICE_TYPE_OTHER;        // Application Specific
        case 0xFF: return USB_DEVICE_TYPE_OTHER;        // Vendor Specific
        default: return USB_DEVICE_TYPE_UNKNOWN;
    }
}

// دالة الخيط
static void* usb_thread_func(void *arg) {
    LOGI("USB redirect thread started");
    
    while (g_usb.running) {
        // TODO: Implement USB device polling and data transfer
        usleep(100000); // 100ms
    }
    
    LOGI("USB redirect thread stopped");
    return NULL;
}

// تهيئة توجيه USB
int usb_redirect_init(void) {
    if (g_usb.initialized) {
        LOGI("USB redirect already initialized");
        return 0;
    }
    
    memset(&g_usb, 0, sizeof(g_usb));
    
    // تخصيص قائمة الأجهزة
    g_usb.device_capacity = MAX_USB_DEVICES;
    g_usb.devices = calloc(g_usb.device_capacity, sizeof(struct usb_device_info));
    if (!g_usb.devices) {
        LOGE("Failed to allocate device list");
        return -1;
    }
    
    g_usb.device_count = 0;
    g_usb.socket_fd = -1;
    
    // إنشاء مقبس Unix للتواصل
    g_usb.socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_usb.socket_fd < 0) {
        LOGE("Failed to create socket");
        free(g_usb.devices);
        return -1;
    }
    
    strcpy(g_usb.socket_path, "/data/data/com.winland.server/usb_redirect.sock");
    
    g_usb.initialized = 1;
    LOGI("USB redirect initialized");
    return 0;
}

// إنهاء توجيه USB
void usb_redirect_terminate(void) {
    if (!g_usb.initialized) return;
    
    usb_redirect_stop();
    
    // إغلاق المقبس
    if (g_usb.socket_fd >= 0) {
        close(g_usb.socket_fd);
        g_usb.socket_fd = -1;
    }
    
    // تحرير قائمة الأجهزة
    if (g_usb.devices) {
        free(g_usb.devices);
        g_usb.devices = NULL;
    }
    
    g_usb.device_count = 0;
    g_usb.initialized = 0;
    
    LOGI("USB redirect terminated");
}

// بدء توجيه USB
int usb_redirect_start(void) {
    if (!g_usb.initialized) {
        LOGE("USB redirect not initialized");
        return -1;
    }
    
    if (g_usb.running) {
        LOGI("USB redirect already running");
        return 0;
    }
    
    g_usb.running = 1;
    
    // إنشاء الخيط
    if (pthread_create(&g_usb_thread, NULL, usb_thread_func, NULL) != 0) {
        LOGE("Failed to create USB thread");
        g_usb.running = 0;
        return -1;
    }
    
    LOGI("USB redirect started");
    return 0;
}

// إيقاف توجيه USB
void usb_redirect_stop(void) {
    if (!g_usb.running) return;
    
    g_usb.running = 0;
    
    // انتظار انتهاء الخيط
    pthread_join(g_usb_thread, NULL);
    
    LOGI("USB redirect stopped");
}

// إضافة جهاز
int usb_redirect_add_device(struct usb_device_info *device) {
    if (!g_usb.initialized || !device) return -1;
    
    pthread_mutex_lock(&g_usb_mutex);
    
    // التحقق من السعة
    if (g_usb.device_count >= g_usb.device_capacity) {
        LOGE("Device list full");
        pthread_mutex_unlock(&g_usb_mutex);
        return -1;
    }
    
    // التحقق من عدم التكرار
    for (int i = 0; i < g_usb.device_count; i++) {
        if (g_usb.devices[i].vendor_id == device->vendor_id &&
            g_usb.devices[i].product_id == device->product_id) {
            LOGI("Device already exists");
            pthread_mutex_unlock(&g_usb_mutex);
            return 0;
        }
    }
    
    // إضافة الجهاز
    memcpy(&g_usb.devices[g_usb.device_count], device, sizeof(struct usb_device_info));
    g_usb.devices[g_usb.device_count].type = get_device_type(device->device_class);
    g_usb.devices[g_usb.device_count].state = USB_DEVICE_STATE_DISCONNECTED;
    g_usb.device_count++;
    
    LOGI("Device added: %04X:%04X (%s)", 
         device->vendor_id, device->product_id, device->product);
    
    // استدعاء معاودة الاتصال
    if (g_usb.on_device_connected) {
        g_usb.on_device_connected(&g_usb.devices[g_usb.device_count - 1]);
    }
    
    pthread_mutex_unlock(&g_usb_mutex);
    return 0;
}

// إزالة جهاز
int usb_redirect_remove_device(uint16_t vendor_id, uint16_t product_id) {
    if (!g_usb.initialized) return -1;
    
    pthread_mutex_lock(&g_usb_mutex);
    
    int found = -1;
    for (int i = 0; i < g_usb.device_count; i++) {
        if (g_usb.devices[i].vendor_id == vendor_id &&
            g_usb.devices[i].product_id == product_id) {
            found = i;
            break;
        }
    }
    
    if (found < 0) {
        pthread_mutex_unlock(&g_usb_mutex);
        return -1;
    }
    
    // استدعاء معاودة الاتصال
    if (g_usb.on_device_disconnected) {
        g_usb.on_device_disconnected(&g_usb.devices[found]);
    }
    
    // إزالة الجهاز بنقل العناصر
    for (int i = found; i < g_usb.device_count - 1; i++) {
        memcpy(&g_usb.devices[i], &g_usb.devices[i + 1], sizeof(struct usb_device_info));
    }
    
    g_usb.device_count--;
    memset(&g_usb.devices[g_usb.device_count], 0, sizeof(struct usb_device_info));
    
    LOGI("Device removed: %04X:%04X", vendor_id, product_id);
    
    pthread_mutex_unlock(&g_usb_mutex);
    return 0;
}

// البحث عن جهاز
struct usb_device_info* usb_redirect_find_device(uint16_t vendor_id, uint16_t product_id) {
    if (!g_usb.initialized) return NULL;
    
    pthread_mutex_lock(&g_usb_mutex);
    
    for (int i = 0; i < g_usb.device_count; i++) {
        if (g_usb.devices[i].vendor_id == vendor_id &&
            g_usb.devices[i].product_id == product_id) {
            pthread_mutex_unlock(&g_usb_mutex);
            return &g_usb.devices[i];
        }
    }
    
    pthread_mutex_unlock(&g_usb_mutex);
    return NULL;
}

// الحصول على قائمة الأجهزة
int usb_redirect_get_device_list(struct usb_device_info **devices, int *count) {
    if (!g_usb.initialized || !devices || !count) return -1;
    
    pthread_mutex_lock(&g_usb_mutex);
    
    *count = g_usb.device_count;
    *devices = malloc(g_usb.device_count * sizeof(struct usb_device_info));
    if (!*devices) {
        pthread_mutex_unlock(&g_usb_mutex);
        return -1;
    }
    
    memcpy(*devices, g_usb.devices, g_usb.device_count * sizeof(struct usb_device_info));
    
    pthread_mutex_unlock(&g_usb_mutex);
    return 0;
}

// إرسال بيانات
int usb_redirect_send_data(uint16_t vendor_id, uint16_t product_id, const uint8_t *data, size_t size) {
    if (!g_usb.initialized || !data || size == 0) return -1;
    
    // TODO: Implement data sending
    LOGI("Sending %zu bytes to device %04X:%04X", size, vendor_id, product_id);
    
    return 0;
}

// استقبال بيانات
int usb_redirect_receive_data(uint16_t vendor_id, uint16_t product_id, uint8_t *data, size_t max_size, size_t *received) {
    if (!g_usb.initialized || !data || !received) return -1;
    
    // TODO: Implement data receiving
    *received = 0;
    
    return 0;
}

// الاتصال بالجهاز
int usb_redirect_connect_device(uint16_t vendor_id, uint16_t product_id) {
    struct usb_device_info *device = usb_redirect_find_device(vendor_id, product_id);
    if (!device) {
        LOGE("Device not found: %04X:%04X", vendor_id, product_id);
        return -1;
    }
    
    if (device->state == USB_DEVICE_STATE_CONNECTED) {
        LOGI("Device already connected");
        return 0;
    }
    
    device->state = USB_DEVICE_STATE_CONNECTING;
    
    // TODO: Implement actual USB connection
    
    device->state = USB_DEVICE_STATE_CONNECTED;
    LOGI("Device connected: %04X:%04X", vendor_id, product_id);
    
    return 0;
}

// قطع الاتصال بالجهاز
int usb_redirect_disconnect_device(uint16_t vendor_id, uint16_t product_id) {
    struct usb_device_info *device = usb_redirect_find_device(vendor_id, product_id);
    if (!device) {
        LOGE("Device not found: %04X:%04X", vendor_id, product_id);
        return -1;
    }
    
    if (device->state != USB_DEVICE_STATE_CONNECTED) {
        LOGI("Device not connected");
        return 0;
    }
    
    // TODO: Implement actual USB disconnection
    
    device->state = USB_DEVICE_STATE_DISCONNECTED;
    LOGI("Device disconnected: %04X:%04X", vendor_id, product_id);
    
    return 0;
}

// إعادة تعيين الجهاز
int usb_redirect_reset_device(uint16_t vendor_id, uint16_t product_id) {
    struct usb_device_info *device = usb_redirect_find_device(vendor_id, product_id);
    if (!device) {
        LOGE("Device not found: %04X:%04X", vendor_id, product_id);
        return -1;
    }
    
    LOGI("Resetting device: %04X:%04X", vendor_id, product_id);
    
    // TODO: Implement device reset
    
    return 0;
}

// تعيين معاودات الاتصال
void usb_redirect_set_callbacks(
    void (*on_device_connected)(struct usb_device_info *device),
    void (*on_device_disconnected)(struct usb_device_info *device),
    void (*on_data_received)(uint8_t *data, size_t size)
) {
    g_usb.on_device_connected = on_device_connected;
    g_usb.on_device_disconnected = on_device_disconnected;
    g_usb.on_data_received = on_data_received;
}
