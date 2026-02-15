#ifndef USB_REDIRECT_H
#define USB_REDIRECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// أنواع أجهزة USB
typedef enum {
    USB_DEVICE_TYPE_UNKNOWN,
    USB_DEVICE_TYPE_STORAGE,
    USB_DEVICE_TYPE_HID,
    USB_DEVICE_TYPE_PRINTER,
    USB_DEVICE_TYPE_AUDIO,
    USB_DEVICE_TYPE_VIDEO,
    USB_DEVICE_TYPE_SERIAL,
    USB_DEVICE_TYPE_NETWORK,
    USB_DEVICE_TYPE_OTHER
} usb_device_type_t;

// حالة الجهاز
typedef enum {
    USB_DEVICE_STATE_DISCONNECTED,
    USB_DEVICE_STATE_CONNECTING,
    USB_DEVICE_STATE_CONNECTED,
    USB_DEVICE_STATE_ERROR
} usb_device_state_t;

// معلومات جهاز USB
struct usb_device_info {
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t bcd_device;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    char manufacturer[256];
    char product[256];
    char serial[256];
    usb_device_type_t type;
    usb_device_state_t state;
};

// هيكل توجيه USB
struct usb_redirect {
    int initialized;
    int running;
    
    // قائمة الأجهزة
    struct usb_device_info *devices;
    int device_count;
    int device_capacity;
    
    // مقبس التوجيه
    int socket_fd;
    char socket_path[256];
    
    // معاودة الاتصال
    void (*on_device_connected)(struct usb_device_info *device);
    void (*on_device_disconnected)(struct usb_device_info *device);
    void (*on_data_received)(uint8_t *data, size_t size);
};

// دوال توجيه USB
int usb_redirect_init(void);
void usb_redirect_terminate(void);
int usb_redirect_start(void);
void usb_redirect_stop(void);

// إدارة الأجهزة
int usb_redirect_add_device(struct usb_device_info *device);
int usb_redirect_remove_device(uint16_t vendor_id, uint16_t product_id);
struct usb_device_info* usb_redirect_find_device(uint16_t vendor_id, uint16_t product_id);
int usb_redirect_get_device_list(struct usb_device_info **devices, int *count);

// نقل البيانات
int usb_redirect_send_data(uint16_t vendor_id, uint16_t product_id, const uint8_t *data, size_t size);
int usb_redirect_receive_data(uint16_t vendor_id, uint16_t product_id, uint8_t *data, size_t max_size, size_t *received);

// التحكم في الجهاز
int usb_redirect_connect_device(uint16_t vendor_id, uint16_t product_id);
int usb_redirect_disconnect_device(uint16_t vendor_id, uint16_t product_id);
int usb_redirect_reset_device(uint16_t vendor_id, uint16_t product_id);

// إعدادات
void usb_redirect_set_callbacks(
    void (*on_device_connected)(struct usb_device_info *device),
    void (*on_device_disconnected)(struct usb_device_info *device),
    void (*on_data_received)(uint8_t *data, size_t size)
);

#ifdef __cplusplus
}
#endif

#endif // USB_REDIRECT_H
