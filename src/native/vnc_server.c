#include "vnc_server.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <android/log.h>
#include <zlib.h>

#define LOG_TAG "VNCServer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// بروتوكول VNC
#define RFB_PROTOCOL_VERSION "RFB 003.008\n"
#define RFB_SECURITY_NONE 1
#define RFB_SECURITY_VNC_AUTH 2
#define RFB_SECURITY_OK 0

// أنواع الرسائل
#define RFB_FRAMEBUFFER_UPDATE 0
#define RFB_SET_COLOR_MAP_ENTRIES 1
#define RFB_BELL 2
#define RFB_SERVER_CUT_TEXT 3

// أنواع رسائل العميل
#define RFB_SET_PIXEL_FORMAT 0
#define RFB_SET_ENCODINGS 2
#define RFB_FRAMEBUFFER_UPDATE_REQUEST 3
#define RFB_KEY_EVENT 4
#define RFB_POINTER_EVENT 5
#define RFB_CLIENT_CUT_TEXT 6

// المتغير العام
static struct vnc_server g_server = {0};
static pthread_t g_server_thread;
static pthread_mutex_t g_server_mutex = PTHREAD_MUTEX_INITIALIZER;

// دالة الخادم
static void* vnc_server_thread(void* arg);
static void* vnc_client_thread(void* arg);

// دوال البروتوكول
static int vnc_handle_protocol_version(struct vnc_client* client);
static int vnc_handle_security(struct vnc_client* client);
static int vnc_handle_client_init(struct vnc_client* client);
static int vnc_handle_message(struct vnc_client* client);

// دوال المساعدة
static int vnc_send_framebuffer_update_raw(struct vnc_client* client, int x, int y, int w, int h);
static int vnc_send_framebuffer_update_tight(struct vnc_client* client, int x, int y, int w, int h);

// تهيئة خادم VNC
int vnc_server_init(uint16_t port, const char* password) {
    if (g_server.initialized) {
        LOGI("VNC server already initialized");
        return 0;
    }
    
    memset(&g_server, 0, sizeof(g_server));
    
    g_server.port = port ? port : VNC_DEFAULT_PORT;
    g_server.width = 1920;
    g_server.height = 1080;
    g_server.bits_per_pixel = 32;
    g_server.depth = 24;
    
    if (password && strlen(password) > 0) {
        strncpy(g_server.password, password, sizeof(g_server.password) - 1);
        g_server.require_password = true;
    }
    
    // تخصيص مخزن الشاشة
    g_server.screen_buffer_size = g_server.width * g_server.height * 4;
    g_server.screen_buffer = malloc(g_server.screen_buffer_size);
    if (!g_server.screen_buffer) {
        LOGE("Failed to allocate screen buffer");
        return -1;
    }
    
    g_server.initialized = 1;
    LOGI("VNC server initialized on port %d", g_server.port);
    return 0;
}

// إنهاء خادم VNC
void vnc_server_terminate(void) {
    if (!g_server.initialized) return;
    
    vnc_server_stop();
    
    // تحرير الموارد
    if (g_server.screen_buffer) {
        free(g_server.screen_buffer);
        g_server.screen_buffer = NULL;
    }
    
    g_server.initialized = 0;
    LOGI("VNC server terminated");
}

// بدء خادم VNC
int vnc_server_start(void) {
    if (!g_server.initialized) {
        LOGE("VNC server not initialized");
        return -1;
    }
    
    if (g_server.running) {
        LOGI("VNC server already running");
        return 0;
    }
    
    // إنشاء مقبس الاستماع
    g_server.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server.listen_fd < 0) {
        LOGE("Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    // السماح بإعادة استخدام العنوان
    int opt = 1;
    if (setsockopt(g_server.listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOGE("Failed to set socket options: %s", strerror(errno));
        close(g_server.listen_fd);
        return -1;
    }
    
    // ربط المقبس
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(g_server.port);
    
    if (bind(g_server.listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOGE("Failed to bind socket: %s", strerror(errno));
        close(g_server.listen_fd);
        return -1;
    }
    
    // الاستماع
    if (listen(g_server.listen_fd, VNC_MAX_CLIENTS) < 0) {
        LOGE("Failed to listen on socket: %s", strerror(errno));
        close(g_server.listen_fd);
        return -1;
    }
    
    g_server.running = 1;
    
    // إنشاء خيط الخادم
    if (pthread_create(&g_server_thread, NULL, vnc_server_thread, NULL) != 0) {
        LOGE("Failed to create server thread");
        g_server.running = 0;
        close(g_server.listen_fd);
        return -1;
    }
    
    LOGI("VNC server started on port %d", g_server.port);
    return 0;
}

// إيقاف خادم VNC
void vnc_server_stop(void) {
    if (!g_server.running) return;
    
    g_server.running = 0;
    
    // إغلاق جميع العملاء
    vnc_server_disconnect_all_clients();
    
    // إغلاق مقبس الاستماع
    if (g_server.listen_fd >= 0) {
        close(g_server.listen_fd);
        g_server.listen_fd = -1;
    }
    
    // انتظار انتهاء الخيط
    pthread_join(g_server_thread, NULL);
    
    LOGI("VNC server stopped");
}

// دالة خيط الخادم
static void* vnc_server_thread(void* arg) {
    LOGI("VNC server thread started");
    
    fd_set read_fds;
    struct timeval tv;
    
    while (g_server.running) {
        FD_ZERO(&read_fds);
        FD_SET(g_server.listen_fd, &read_fds);
        
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout
        
        int ret = select(g_server.listen_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            LOGE("Select error: %s", strerror(errno));
            break;
        }
        
        if (ret > 0 && FD_ISSET(g_server.listen_fd, &read_fds)) {
            // قبول اتصال جديد
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            int client_fd = accept(g_server.listen_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd < 0) {
                LOGE("Failed to accept connection: %s", strerror(errno));
                continue;
            }
            
            // تعيين خيارات المقبس
            int opt = 1;
            setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
            
            // البحث عن خانة عميل فارغة
            int slot = -1;
            for (int i = 0; i < VNC_MAX_CLIENTS; i++) {
                if (g_server.clients[i] == NULL) {
                    slot = i;
                    break;
                }
            }
            
            if (slot < 0) {
                LOGI("Max clients reached, rejecting connection");
                close(client_fd);
                continue;
            }
            
            // إنشاء هيكل العميل
            struct vnc_client* client = calloc(1, sizeof(*client));
            if (!client) {
                LOGE("Failed to allocate client");
                close(client_fd);
                continue;
            }
            
            client->socket_fd = client_fd;
            client->state = VNC_CLIENT_CONNECTING;
            client->ip_address = client_addr.sin_addr.s_addr;
            
            g_server.clients[slot] = client;
            g_server.client_count++;
            
            // إنشاء خيط للعميل
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, vnc_client_thread, client);
            pthread_detach(client_thread);
            
            LOGI("New VNC client connected from %s", inet_ntoa(client_addr.sin_addr));
        }
    }
    
    LOGI("VNC server thread stopped");
    return NULL;
}

// دالة خيط العميل
static void* vnc_client_thread(void* arg) {
    struct vnc_client* client = (struct vnc_client*)arg;
    
    // معالجة البروتوكول
    if (vnc_handle_protocol_version(client) < 0) goto disconnect;
    if (vnc_handle_security(client) < 0) goto disconnect;
    if (vnc_handle_client_init(client) < 0) goto disconnect;
    
    client->state = VNC_CLIENT_CONNECTED;
    LOGI("VNC client authenticated and ready");
    
    // حلقة الرسائل الرئيسية
    while (g_server.running && client->state == VNC_CLIENT_CONNECTED) {
        if (vnc_handle_message(client) < 0) break;
    }
    
disconnect:
    LOGI("VNC client disconnected");
    
    // إزالة العميل
    for (int i = 0; i < VNC_MAX_CLIENTS; i++) {
        if (g_server.clients[i] == client) {
            g_server.clients[i] = NULL;
            g_server.client_count--;
            break;
        }
    }
    
    // تحرير الموارد
    if (client->framebuffer) free(client->framebuffer);
    close(client->socket_fd);
    free(client);
    
    return NULL;
}

// معالجة إصدار البروتوكول
static int vnc_handle_protocol_version(struct vnc_client* client) {
    // إرسال إصدار البروتوكول
    if (send(client->socket_fd, RFB_PROTOCOL_VERSION, strlen(RFB_PROTOCOL_VERSION), 0) < 0) {
        return -1;
    }
    
    // استقبال إصدار العميل
    char version[13];
    if (recv(client->socket_fd, version, 12, 0) != 12) {
        return -1;
    }
    version[12] = '\0';
    
    LOGI("Client protocol version: %s", version);
    return 0;
}

// معالجة الأمان
static int vnc_handle_security(struct vnc_client* client) {
    // إرسال أنواع الأمان المدعومة
    uint8_t num_types = g_server.require_password ? 2 : 1;
    send(client->socket_fd, &num_types, 1, 0);
    
    if (g_server.require_password) {
        uint8_t types[] = {RFB_SECURITY_NONE, RFB_SECURITY_VNC_AUTH};
        send(client->socket_fd, types, 2, 0);
    } else {
        uint8_t type = RFB_SECURITY_NONE;
        send(client->socket_fd, &type, 1, 0);
    }
    
    // استقبال اختيار العميل
    uint8_t selected_type;
    if (recv(client->socket_fd, &selected_type, 1, 0) != 1) {
        return -1;
    }
    
    if (selected_type == RFB_SECURITY_NONE) {
        // لا أمان مطلوب
        uint32_t result = 0; // OK
        send(client->socket_fd, &result, 4, 0);
    } else if (selected_type == RFB_SECURITY_VNC_AUTH) {
        // TODO: Implement VNC authentication
        uint32_t result = 0; // OK for now
        send(client->socket_fd, &result, 4, 0);
    } else {
        return -1;
    }
    
    return 0;
}

// معالجة تهيئة العميل
static int vnc_handle_client_init(struct vnc_client* client) {
    // استقبال مشاركة سطح المكتب
    uint8_t shared;
    if (recv(client->socket_fd, &shared, 1, 0) != 1) {
        return -1;
    }
    
    // إرسال تهيئة الخادم
    uint16_t width = htons(g_server.width);
    uint16_t height = htons(g_server.height);
    
    // تنسيق البكسل
    uint8_t pixel_format[16];
    pixel_format[0] = g_server.bits_per_pixel;  // bits-per-pixel
    pixel_format[1] = g_server.depth;           // depth
    pixel_format[2] = 0;                        // big-endian-flag
    pixel_format[3] = 1;                        // true-color-flag
    pixel_format[4] = 0; pixel_format[5] = 255; // red-max
    pixel_format[6] = 0; pixel_format[7] = 255; // green-max
    pixel_format[8] = 0; pixel_format[9] = 255; // blue-max
    pixel_format[10] = 16;                      // red-shift
    pixel_format[11] = 8;                       // green-shift
    pixel_format[12] = 0;                       // blue-shift
    pixel_format[13] = 0; pixel_format[14] = 0; pixel_format[15] = 0; // padding
    
    // اسم سطح المكتب
    const char* name = "Winland Server";
    uint32_t name_len = htonl(strlen(name));
    
    // إرسال البيانات
    send(client->socket_fd, &width, 2, 0);
    send(client->socket_fd, &height, 2, 0);
    send(client->socket_fd, pixel_format, 16, 0);
    send(client->socket_fd, &name_len, 4, 0);
    send(client->socket_fd, name, strlen(name), 0);
    
    return 0;
}

// معالجة رسالة العميل
static int vnc_handle_message(struct vnc_client* client) {
    uint8_t msg_type;
    
    // استقبال نوع الرسالة
    int ret = recv(client->socket_fd, &msg_type, 1, MSG_DONTWAIT);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            usleep(1000);
            return 0;
        }
        return -1;
    }
    if (ret == 0) {
        return -1; // Connection closed
    }
    
    switch (msg_type) {
        case RFB_SET_PIXEL_FORMAT:
            // TODO: Handle set pixel format
            break;
            
        case RFB_SET_ENCODINGS:
            // TODO: Handle set encodings
            break;
            
        case RFB_FRAMEBUFFER_UPDATE_REQUEST:
            // TODO: Handle framebuffer update request
            break;
            
        case RFB_KEY_EVENT:
            // TODO: Handle key event
            break;
            
        case RFB_POINTER_EVENT:
            // TODO: Handle pointer event
            break;
            
        default:
            LOGI("Unknown message type: %d", msg_type);
            break;
    }
    
    return 0;
}

// تعيين الدقة
void vnc_server_set_resolution(uint16_t width, uint16_t height) {
    pthread_mutex_lock(&g_server_mutex);
    g_server.width = width;
    g_server.height = height;
    pthread_mutex_unlock(&g_server_mutex);
}

// تعيين كلمة المرور
void vnc_server_set_password(const char* password) {
    pthread_mutex_lock(&g_server_mutex);
    if (password && strlen(password) > 0) {
        strncpy(g_server.password, password, sizeof(g_server.password) - 1);
        g_server.require_password = true;
    } else {
        g_server.password[0] = '\0';
        g_server.require_password = false;
    }
    pthread_mutex_unlock(&g_server_mutex);
}

// تعيين معاودة الاتصال للإطار
void vnc_server_set_frame_callback(int (*callback)(uint8_t** buffer, uint16_t* width, uint16_t* height)) {
    pthread_mutex_lock(&g_server_mutex);
    g_server.get_frame_callback = callback;
    pthread_mutex_unlock(&g_server_mutex);
}

// تعيين معاودات الاتصال للمدخلات
void vnc_server_set_input_callbacks(
    void (*pointer_callback)(int x, int y, uint8_t button_mask),
    void (*key_callback)(uint32_t key, bool down)
) {
    pthread_mutex_lock(&g_server_mutex);
    g_server.pointer_event_callback = pointer_callback;
    g_server.key_event_callback = key_callback;
    pthread_mutex_unlock(&g_server_mutex);
}

// الحصول على عدد العملاء
int vnc_server_get_client_count(void) {
    return g_server.client_count;
}

// قطع اتصال عميل
void vnc_server_disconnect_client(int client_id) {
    if (client_id < 0 || client_id >= VNC_MAX_CLIENTS) return;
    
    struct vnc_client* client = g_server.clients[client_id];
    if (client) {
        client->state = VNC_CLIENT_DISCONNECTED;
        close(client->socket_fd);
    }
}

// قطع اتصال جميع العملاء
void vnc_server_disconnect_all_clients(void) {
    for (int i = 0; i < VNC_MAX_CLIENTS; i++) {
        vnc_server_disconnect_client(i);
    }
}

// طلب تحديث
void vnc_server_request_update(void) {
    for (int i = 0; i < VNC_MAX_CLIENTS; i++) {
        if (g_server.clients[i] && g_server.clients[i]->state == VNC_CLIENT_CONNECTED) {
            g_server.clients[i]->update_pending = true;
        }
    }
}

// التحقق من التشغيل
bool vnc_server_is_running(void) {
    return g_server.running;
}

// الحصول على المنفذ
uint16_t vnc_server_get_port(void) {
    return g_server.port;
}

// الحصول على العنوان
const char* vnc_server_get_address(void) {
    return "0.0.0.0";
}
