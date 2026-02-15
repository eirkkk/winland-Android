#ifndef AUDIO_BRIDGE_H
#define AUDIO_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// إعدادات الصوت
#define AUDIO_BRIDGE_SAMPLE_RATE    48000
#define AUDIO_BRIDGE_CHANNELS       2
#define AUDIO_BRIDGE_BUFFER_SIZE    4096

// تنسيقات الصوت
typedef enum {
    AUDIO_FORMAT_S16_LE,
    AUDIO_FORMAT_S16_BE,
    AUDIO_FORMAT_FLOAT_LE,
    AUDIO_FORMAT_FLOAT_BE
} audio_format_t;

// هيكل جسر الصوت
struct audio_bridge {
    int initialized;
    int running;
    
    // معلمات الصوت
    uint32_t sample_rate;
    uint32_t channels;
    audio_format_t format;
    
    // المخازن المؤقتة
    void *input_buffer;
    void *output_buffer;
    size_t buffer_size;
    
    // مقبس الصوت
    int socket_fd;
    char socket_path[256];
};

// دوال جسر الصوت
int audio_bridge_init(void);
void audio_bridge_terminate(void);
int audio_bridge_start(void);
void audio_bridge_stop(void);

// إعدادات الصوت
int audio_bridge_set_format(uint32_t sample_rate, uint32_t channels, audio_format_t format);
int audio_bridge_get_format(uint32_t *sample_rate, uint32_t *channels, audio_format_t *format);

// معالجة الصوت
int audio_bridge_process_input(const void *data, size_t size);
int audio_bridge_process_output(void *data, size_t size);
size_t audio_bridge_get_available_input(void);
size_t audio_bridge_get_available_output(void);

// التحكم في مستوى الصوت
int audio_bridge_set_input_volume(float volume);
int audio_bridge_set_output_volume(float volume);
float audio_bridge_get_input_volume(void);
float audio_bridge_get_output_volume(void);

// كتم/إلغاء كتم الصوت
void audio_bridge_set_input_mute(int mute);
void audio_bridge_set_output_mute(int mute);
int audio_bridge_get_input_mute(void);
int audio_bridge_get_output_mute(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_BRIDGE_H
