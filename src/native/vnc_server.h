#ifndef VNC_SERVER_H
#define VNC_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// إعدادات VNC
#define VNC_DEFAULT_PORT 5900
#define VNC_MAX_CLIENTS 8
#define VNC_BUFFER_SIZE 65536
#define VNC_UPDATE_INTERVAL 16  // ~60 FPS

// تنسيقات البكسل VNC
typedef enum {
    VNC_FORMAT_RAW,
    VNC_FORMAT_COPYRECT,
    VNC_FORMAT_RRE,
    VNC_FORMAT_HEXTILE,
    VNC_FORMAT_ZRLE,
    VNC_FORMAT_TIGHT,
    VNC_FORMAT_JPEG
} vnc_encoding_t;

// حالات العميل
typedef enum {
    VNC_CLIENT_DISCONNECTED,
    VNC_CLIENT_CONNECTING,
    VNC_CLIENT_HANDSHAKE,
    VNC_CLIENT_AUTHENTICATING,
    VNC_CLIENT_CONNECTED,
    VNC_CLIENT_ERROR
} vnc_client_state_t;

// هيكل عميل VNC
struct vnc_client {
    int socket_fd;
    vnc_client_state_t state;
    
    // معلومات العميل
    char name[256];
    uint32_t ip_address;
    
    // إعدادات
    uint16_t preferred_encoding;
    bool supports_cursor;
    bool supports_desktop_resize;
    bool supports_extended_desktop_size;
    
    // المنطقة المعروضة
    int16_t x, y;
    uint16_t width, height;
    
    // المخزن المؤقت
    uint8_t* framebuffer;
    size_t framebuffer_size;
    
    // الحالة
    bool update_pending;
    bool cursor_update_pending;
};

// هيكل خادم VNC
struct vnc_server {
    int initialized;
    int running;
    
    // إعدادات
    uint16_t port;
    char password[256];
    bool require_password;
    
    // المقبس
    int listen_fd;
    
    // العملاء
    struct vnc_client* clients[VNC_MAX_CLIENTS];
    int client_count;
    
    // الأبعاد
    uint16_t width;
    uint16_t height;
    uint8_t bits_per_pixel;
    uint8_t depth;
    
    // المخزن المؤقت للشاشة
    uint8_t* screen_buffer;
    size_t screen_buffer_size;
    
    // معاودة الاتصال للحصول على الإطار
    int (*get_frame_callback)(uint8_t** buffer, uint16_t* width, uint16_t* height);
    
    // معاودة الاتصال لأحداث المدخلات
    void (*pointer_event_callback)(int x, int y, uint8_t button_mask);
    void (*key_event_callback)(uint32_t key, bool down);
};

// دوال الخادم
int vnc_server_init(uint16_t port, const char* password);
void vnc_server_terminate(void);
int vnc_server_start(void);
void vnc_server_stop(void);

// إعدادات
void vnc_server_set_resolution(uint16_t width, uint16_t height);
void vnc_server_set_password(const char* password);
void vnc_server_set_frame_callback(int (*callback)(uint8_t** buffer, uint16_t* width, uint16_t* height));
void vnc_server_set_input_callbacks(
    void (*pointer_callback)(int x, int y, uint8_t button_mask),
    void (*key_callback)(uint32_t key, bool down)
);

// إدارة العملاء
int vnc_server_get_client_count(void);
void vnc_server_disconnect_client(int client_id);
void vnc_server_disconnect_all_clients(void);

// تحديثات الشاشة
void vnc_server_request_update(void);
void vnc_server_send_framebuffer_update(int client_id);
void vnc_server_send_cursor_update(int client_id);
void vnc_server_send_desktop_size_update(void);

// معالجة المدخلات
void vnc_server_handle_pointer_event(int client_id, int x, int y, uint8_t button_mask);
void vnc_server_handle_key_event(int client_id, uint32_t key, bool down);

// معلومات
bool vnc_server_is_running(void);
uint16_t vnc_server_get_port(void);
const char* vnc_server_get_address(void);

#ifdef __cplusplus
}
#endif

#endif // VNC_SERVER_H
