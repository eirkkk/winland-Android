#ifndef WEBRTC_SERVER_H
#define WEBRTC_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// حالات الاتصال
typedef enum {
    WEBRTC_STATE_DISCONNECTED,
    WEBRTC_STATE_CONNECTING,
    WEBRTC_STATE_CONNECTED,
    WEBRTC_STATE_ERROR
} webrtc_state_t;

// أنواع الوسائط
typedef enum {
    WEBRTC_MEDIA_VIDEO,
    WEBRTC_MEDIA_AUDIO,
    WEBRTC_MEDIA_DATA
} webrtc_media_type_t;

// إعدادات WebRTC
struct webrtc_config {
    // الشبكة
    uint16_t signaling_port;        // منفذ الإشارات (WebSocket)
    uint16_t stun_port;
    char stun_server[256];
    char turn_server[256];
    char turn_username[128];
    char turn_password[128];
    
    // الفيديو
    int video_width;
    int video_height;
    int video_fps;
    int video_bitrate;
    
    // الصوت
    int audio_sample_rate;
    int audio_channels;
    int audio_bitrate;
    
    // الأمان
    bool use_tls;
    char cert_path[512];
    char key_path[512];
};

// معلومات العميل
struct webrtc_client {
    uint32_t id;
    webrtc_state_t state;
    char remote_address[256];
    
    // الإحصائيات
    uint64_t bytes_sent;
    uint64_t bytes_received;
    float video_fps;
    int video_bitrate;
    int audio_bitrate;
    float rtt_ms;
    float packet_loss;
};

// هيكل خادم WebRTC
struct webrtc_server {
    int initialized;
    int running;
    
    // الإعدادات
    struct webrtc_config config;
    
    // المقابس
    int signaling_socket;
    int stun_socket;
    
    // العملاء
    struct webrtc_client* clients;
    int client_count;
    int client_capacity;
    
    // الحالة
    webrtc_state_t state;
    
    // معاودات الاتصال
    void (*on_client_connected)(uint32_t client_id);
    void (*on_client_disconnected)(uint32_t client_id);
    void (*on_ice_candidate)(const char* candidate);
    void (*on_offer)(const char* sdp);
    void (*on_answer)(const char* sdp);
};

// دوال التهيئة
int webrtc_server_init(const struct webrtc_config* config);
void webrtc_server_terminate(void);
int webrtc_server_start(void);
void webrtc_server_stop(void);

// الإعدادات
void webrtc_server_set_config(const struct webrtc_config* config);
const struct webrtc_config* webrtc_server_get_config(void);

// إدارة العملاء
int webrtc_server_get_client_count(void);
struct webrtc_client* webrtc_server_get_client(uint32_t client_id);
void webrtc_server_disconnect_client(uint32_t client_id);
void webrtc_server_disconnect_all(void);

// إشارات WebRTC
int webrtc_server_send_offer(uint32_t client_id, const char* sdp);
int webrtc_server_send_answer(uint32_t client_id, const char* sdp);
int webrtc_server_send_ice_candidate(uint32_t client_id, const char* candidate);
int webrtc_server_process_signaling_message(uint32_t client_id, const char* message);

// الوسائط
int webrtc_server_send_video_frame(const uint8_t* data, size_t size, uint32_t timestamp);
int webrtc_server_send_audio_samples(const uint8_t* data, size_t size, uint32_t timestamp);
int webrtc_server_send_data_message(const char* message);

// الإحصائيات
webrtc_state_t webrtc_server_get_state(void);
void webrtc_server_get_stats(struct webrtc_client* client);

// الترميز
int webrtc_server_set_video_codec(const char* codec);  // "VP8", "VP9", "H264"
int webrtc_server_set_audio_codec(const char* codec);  // "OPUS", "G711"

// معاودات الاتصال
void webrtc_server_set_callbacks(
    void (*on_client_connected)(uint32_t client_id),
    void (*on_client_disconnected)(uint32_t client_id),
    void (*on_ice_candidate)(const char* candidate),
    void (*on_offer)(const char* sdp),
    void (*on_answer)(const char* sdp)
);

#ifdef __cplusplus
}
#endif

#endif // WEBRTC_SERVER_H
