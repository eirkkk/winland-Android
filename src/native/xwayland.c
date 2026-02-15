#include "xwayland.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include <wayland-server.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xfixes.h>
#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/shape.h>
#include <xcb/xcb_renderutil.h>

#define XWAYLAND_SOCKET_DIR "/tmp/.X11-unix"
#define MAX_X11_DISPLAY 999
#define WM_CLASS "winland-wm"

// السياق العالمي
static struct xwayland_context g_xwayland = {0};
static pthread_mutex_t g_xwayland_mutex = PTHREAD_MUTEX_INITIALIZER;

// اتصال XCB
static xcb_connection_t* g_xcb_conn = NULL;
static xcb_screen_t* g_xcb_screen = NULL;
static xcb_ewmh_connection_t g_xcb_ewmh;
static xcb_window_t g_wm_window = XCB_WINDOW_NONE;
static int g_xcb_fd = -1;

// أحداث Wayland
static struct wl_event_source* g_xcb_event_source = NULL;

// تحويل نوع النافذة
static x11_window_type_t get_window_type(xcb_atom_t atom) {
    if (!g_xcb_conn) return X11_WINDOW_TYPE_NORMAL;
    
    xcb_atom_t* atoms = g_xcb_ewmh.WM_WINDOW_TYPE;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_NORMAL]) return X11_WINDOW_TYPE_NORMAL;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_DIALOG]) return X11_WINDOW_TYPE_DIALOG;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_MENU]) return X11_WINDOW_TYPE_MENU;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_TOOLBAR]) return X11_WINDOW_TYPE_TOOLBAR;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_SPLASH]) return X11_WINDOW_TYPE_SPLASH;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_UTILITY]) return X11_WINDOW_TYPE_UTILITY;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_DOCK]) return X11_WINDOW_TYPE_DOCK;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_DESKTOP]) return X11_WINDOW_TYPE_DESKTOP;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_POPUP_MENU]) return X11_WINDOW_TYPE_POPUP_MENU;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_DROPDOWN_MENU]) return X11_WINDOW_TYPE_DROPDOWN_MENU;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_TOOLTIP]) return X11_WINDOW_TYPE_TOOLTIP;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_NOTIFICATION]) return X11_WINDOW_TYPE_NOTIFICATION;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_COMBO]) return X11_WINDOW_TYPE_COMBO;
    if (atom == atoms[XCB_EWMH_WINDOW_TYPE_DND]) return X11_WINDOW_TYPE_DND;
    
    return X11_WINDOW_TYPE_NORMAL;
}

// إنشاء مقبس X11
static int create_x11_socket(int display) {
    char path[64];
    snprintf(path, sizeof(path), "%s/X%d", XWAYLAND_SOCKET_DIR, display);
    
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    
    // إزالة الملف القديم إذا وجد
    unlink(path);
    
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    
    if (listen(fd, 1) < 0) {
        close(fd);
        unlink(path);
        return -1;
    }
    
    return fd;
}

// العثور على رقم عرض متاح
static int find_available_display(void) {
    struct stat st;
    if (stat(XWAYLAND_SOCKET_DIR, &st) < 0) {
        mkdir(XWAYLAND_SOCKET_DIR, 0755);
    }
    
    for (int i = 0; i <= MAX_X11_DISPLAY; i++) {
        char path[64];
        snprintf(path, sizeof(path), "%s/X%d", XWAYLAND_SOCKET_DIR, i);
        
        if (stat(path, &st) < 0) {
            // الملف غير موجود، تحقق من قفل الشاشة
            snprintf(path, sizeof(path), "/tmp/.X%d-lock", i);
            if (stat(path, &st) < 0) {
                return i;
            }
        }
    }
    
    return -1;
}

// معالج إشارة SIGCHLD
static void sigchld_handler(int sig) {
    int status;
    pid_t pid = waitpid(g_xwayland.pid, &status, WNOHANG);
    
    if (pid == g_xwayland.pid) {
        pthread_mutex_lock(&g_xwayland_mutex);
        
        if (WIFEXITED(status)) {
            g_xwayland.exit_code = WEXITSTATUS(status);
            snprintf(g_xwayland.error_message, sizeof(g_xwayland.error_message),
                     "XWayland exited with code %d", g_xwayland.exit_code);
        } else if (WIFSIGNALED(status)) {
            snprintf(g_xwayland.error_message, sizeof(g_xwayland.error_message),
                     "XWayland killed by signal %d", WTERMSIG(status));
        }
        
        g_xwayland.state = XWAYLAND_STATE_ERROR;
        g_xwayland.pid = -1;
        
        pthread_mutex_unlock(&g_xwayland_mutex);
    }
}

// قراءة خصائص النافذة
static void read_window_properties(struct xwayland_surface* surface) {
    if (!g_xcb_conn || !surface) return;
    
    xcb_window_t win = surface->window_id;
    
    // قراءة العنوان
    xcb_icccm_get_text_property_reply_t title_reply;
    if (xcb_icccm_get_wm_name_reply(g_xcb_conn,
                                     xcb_icccm_get_wm_name(g_xcb_conn, win),
                                     &title_reply, NULL)) {
        strncpy(surface->props.title, title_reply.name, 
                sizeof(surface->props.title) - 1);
        xcb_icccm_get_text_property_reply_wipe(&title_reply);
    }
    
    // قراءة الفئة والمثيل
    xcb_icccm_get_wm_class_reply_t class_reply;
    if (xcb_icccm_get_wm_class_reply(g_xcb_conn,
                                      xcb_icccm_get_wm_class(g_xcb_conn, win),
                                      &class_reply, NULL)) {
        strncpy(surface->props.class_name, class_reply.class_name,
                sizeof(surface->props.class_name) - 1);
        strncpy(surface->props.instance, class_reply.instance_name,
                sizeof(surface->props.instance) - 1);
        xcb_icccm_get_wm_class_reply_wipe(&class_reply);
    }
    
    // قراءة نوع النافذة
    xcb_ewmh_get_atoms_reply_t type_reply;
    if (xcb_ewmh_get_wm_window_type_reply(&g_xcb_ewmh,
                                           xcb_ewmh_get_wm_window_type(&g_xcb_ewmh, win),
                                           &type_reply, NULL)) {
        if (type_reply.atoms_len > 0) {
            surface->props.window_type = get_window_type(type_reply.atoms[0]);
        }
        xcb_ewmh_get_atoms_reply_wipe(&type_reply);
    }
    
    // قراءة الحالة
    xcb_atom_t state_atoms[16];
    int state_count = 0;
    
    xcb_ewmh_get_atoms_reply_t state_reply;
    if (xcb_ewmh_get_wm_state_reply(&g_xcb_ewmh,
                                     xcb_ewmh_get_wm_state(&g_xcb_ewmh, win),
                                     &state_reply, NULL)) {
        for (unsigned int i = 0; i < state_reply.atoms_len && i < 16; i++) {
            state_atoms[state_count++] = state_reply.atoms[i];
        }
        xcb_ewmh_get_atoms_reply_wipe(&state_reply);
    }
    
    // التحقق من الحالات
    for (int i = 0; i < state_count; i++) {
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_MODAL)
            surface->props.modal = true;
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_FULLSCREEN)
            surface->props.fullscreen = true;
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_MAXIMIZED_VERT)
            surface->props.maximized_vert = true;
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_MAXIMIZED_HORZ)
            surface->props.maximized_horz = true;
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_HIDDEN)
            surface->props.minimized = true;
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_ABOVE)
            surface->props.above = true;
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_BELOW)
            surface->props.below = true;
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_STICKY)
            surface->props.sticky = true;
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_SKIP_TASKBAR)
            surface->props.skip_taskbar = true;
        if (state_atoms[i] == g_xcb_ewmh._NET_WM_STATE_SKIP_PAGER)
            surface->props.skip_pager = true;
    }
    
    // قراءة الهندسة
    xcb_get_geometry_reply_t* geom = xcb_get_geometry_reply(g_xcb_conn,
                                                             xcb_get_geometry(g_xcb_conn, win),
                                                             NULL);
    if (geom) {
        surface->props.x = geom->x;
        surface->props.y = geom->y;
        surface->props.width = geom->width;
        surface->props.height = geom->height;
        free(geom);
    }
    
    // قراءة الأحجام الطبيعية
    xcb_size_hints_t hints;
    if (xcb_icccm_get_wm_normal_hints_reply(g_xcb_conn,
                                             xcb_icccm_get_wm_normal_hints(g_xcb_conn, win),
                                             &hints, NULL)) {
        if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
            surface->props.min_width = hints.min_width;
            surface->props.min_height = hints.min_height;
        }
        if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
            surface->props.max_width = hints.max_width;
            surface->props.max_height = hints.max_height;
        }
    }
}

// إنشاء سطح XWayland
struct xwayland_surface* xwayland_surface_create(uint32_t window_id) {
    pthread_mutex_lock(&g_xwayland_mutex);
    
    // التحقق من عدم وجود السابق
    struct xwayland_surface* existing = xwayland_surface_lookup(window_id);
    if (existing) {
        pthread_mutex_unlock(&g_xwayland_mutex);
        return existing;
    }
    
    struct xwayland_surface* surface = calloc(1, sizeof(*surface));
    if (!surface) {
        pthread_mutex_unlock(&g_xwayland_mutex);
        return NULL;
    }
    
    surface->window_id = window_id;
    surface->props.window_type = X11_WINDOW_TYPE_NORMAL;
    
    wl_list_insert(&g_xwayland.surfaces, &surface->link);
    g_xwayland.surface_count++;
    
    // قراءة الخصائص
    read_window_properties(surface);
    
    // إشعار المعاودة
    if (g_xwayland.on_surface_create) {
        g_xwayland.on_surface_create(&g_xwayland, surface);
    }
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return surface;
}

// البحث عن سطح
struct xwayland_surface* xwayland_surface_lookup(uint32_t window_id) {
    struct xwayland_surface* surface;
    wl_list_for_each(surface, &g_xwayland.surfaces, link) {
        if (surface->window_id == window_id) {
            return surface;
        }
    }
    return NULL;
}

// إتلاف سطح
void xwayland_surface_destroy(struct xwayland_surface* surface) {
    if (!surface) return;
    
    pthread_mutex_lock(&g_xwayland_mutex);
    
    // إشعار المعاودة
    if (g_xwayland.on_surface_destroy) {
        g_xwayland.on_surface_destroy(&g_xwayland, surface);
    }
    
    // إزالة من القائمة
    wl_list_remove(&surface->link);
    g_xwayland.surface_count--;
    
    // إلغاء التركيز إذا كان هذا السطح محدداً
    if (g_xwayland.focused_surface == surface) {
        g_xwayland.focused_surface = NULL;
    }
    if (g_xwayland.cursor_surface == surface) {
        g_xwayland.cursor_surface = NULL;
    }
    
    free(surface);
    
    pthread_mutex_unlock(&g_xwayland_mutex);
}

// تكوين السطح
int xwayland_surface_configure(struct xwayland_surface* surface, int x, int y,
                                int width, int height) {
    if (!surface || !g_xcb_conn) return -1;
    
    pthread_mutex_lock(&g_xwayland_mutex);
    
    uint32_t values[4] = {x, y, width, height};
    uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    
    xcb_configure_window(g_xcb_conn, surface->window_id, mask, values);
    xcb_flush(g_xcb_conn);
    
    surface->props.x = x;
    surface->props.y = y;
    surface->props.width = width;
    surface->props.height = height;
    surface->configured = true;
    
    if (surface->on_configure) {
        surface->on_configure(surface, x, y, width, height);
    }
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// تعيين حالة الظهور
int xwayland_surface_set_mapped(struct xwayland_surface* surface, bool mapped) {
    if (!surface) return -1;
    
    pthread_mutex_lock(&g_xwayland_mutex);
    
    surface->mapped = mapped;
    
    if (mapped) {
        if (surface->on_map) {
            surface->on_map(surface);
        }
    } else {
        if (surface->on_unmap) {
            surface->on_unmap(surface);
        }
    }
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// تنشيط السطح
int xwayland_surface_set_activated(struct xwayland_surface* surface, bool activated) {
    if (!surface || !g_xcb_conn) return -1;
    
    pthread_mutex_lock(&g_xwayland_mutex);
    
    xcb_client_message_event_t event = {0};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = surface->window_id;
    event.type = g_xcb_ewmh._NET_ACTIVE_WINDOW;
    event.data.data32[0] = activated ? 1 : 0;
    event.data.data32[1] = XCB_CURRENT_TIME;
    event.data.data32[2] = g_wm_window;
    
    xcb_send_event(g_xcb_conn, 0, g_xcb_screen->root,
                   XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (const char*)&event);
    xcb_flush(g_xcb_conn);
    
    if (activated && surface->on_activate) {
        surface->on_activate(surface);
    }
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// إغلاق السطح
int xwayland_surface_close(struct xwayland_surface* surface) {
    if (!surface || !g_xcb_conn) return -1;
    
    pthread_mutex_lock(&g_xwayland_mutex);
    
    if (surface->on_close) {
        surface->on_close(surface);
    }
    
    // إرسال حدث إغلاق
    xcb_icccm_get_wm_protocols_reply_t protocols;
    if (xcb_icccm_get_wm_protocols_reply(g_xcb_conn,
                                          xcb_icccm_get_wm_protocols(g_xcb_conn, 
                                                                     surface->window_id,
                                                                     g_xcb_ewmh.WM_PROTOCOLS),
                                          &protocols, NULL)) {
        bool has_delete = false;
        for (unsigned int i = 0; i < protocols.atoms_len; i++) {
            if (protocols.atoms[i] == g_xcb_ewmh.WM_DELETE_WINDOW) {
                has_delete = true;
                break;
            }
        }
        xcb_icccm_get_wm_protocols_reply_wipe(&protocols);
        
        if (has_delete) {
            xcb_client_message_event_t event = {0};
            event.response_type = XCB_CLIENT_MESSAGE;
            event.format = 32;
            event.window = surface->window_id;
            event.type = g_xcb_ewmh.WM_PROTOCOLS;
            event.data.data32[0] = g_xcb_ewmh.WM_DELETE_WINDOW;
            event.data.data32[1] = XCB_CURRENT_TIME;
            
            xcb_send_event(g_xcb_conn, 0, surface->window_id,
                           XCB_EVENT_MASK_NO_EVENT, (const char*)&event);
            xcb_flush(g_xcb_conn);
        } else {
            // إنهاء بالقوة
            xcb_kill_client(g_xcb_conn, surface->window_id);
            xcb_flush(g_xcb_conn);
        }
    }
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// تعيين ملء الشاشة
int xwayland_surface_set_fullscreen(struct xwayland_surface* surface, bool fullscreen) {
    if (!surface || !g_xcb_conn) return -1;
    
    pthread_mutex_lock(&g_xwayland_mutex);
    
    xcb_client_message_event_t event = {0};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = surface->window_id;
    event.type = g_xcb_ewmh._NET_WM_STATE;
    event.data.data32[0] = fullscreen ? 1 : 0;  // إضافة/إزالة
    event.data.data32[1] = g_xcb_ewmh._NET_WM_STATE_FULLSCREEN;
    event.data.data32[2] = 0;
    event.data.data32[3] = 1;  // مصدر التطبيق
    
    xcb_send_event(g_xcb_conn, 0, g_xcb_screen->root,
                   XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (const char*)&event);
    xcb_flush(g_xcb_conn);
    
    surface->props.fullscreen = fullscreen;
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// تعيين التكبير
int xwayland_surface_set_maximized(struct xwayland_surface* surface, bool maximized) {
    if (!surface || !g_xcb_conn) return -1;
    
    pthread_mutex_lock(&g_xwayland_mutex);
    
    xcb_client_message_event_t event = {0};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = surface->window_id;
    event.type = g_xcb_ewmh._NET_WM_STATE;
    event.data.data32[0] = maximized ? 1 : 0;
    event.data.data32[1] = g_xcb_ewmh._NET_WM_STATE_MAXIMIZED_HORZ;
    event.data.data32[2] = g_xcb_ewmh._NET_WM_STATE_MAXIMIZED_VERT;
    event.data.data32[3] = 1;
    
    xcb_send_event(g_xcb_conn, 0, g_xcb_screen->root,
                   XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (const char*)&event);
    xcb_flush(g_xcb_conn);
    
    surface->props.maximized_horz = maximized;
    surface->props.maximized_vert = maximized;
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// تعيين التصغير
int xwayland_surface_set_minimized(struct xwayland_surface* surface, bool minimized) {
    if (!surface || !g_xcb_conn) return -1;
    
    pthread_mutex_lock(&g_xwayland_mutex);
    
    if (minimized) {
        xcb_icccm_wm_hints_t hints;
        hints.flags = XCB_ICCCM_WM_HINT_STATE;
        hints.initial_state = XCB_ICCCM_WM_STATE_ICONIC;
        xcb_icccm_set_wm_hints(g_xcb_conn, surface->window_id, &hints);
        
        // إخفاء النافذة
        xcb_unmap_window(g_xcb_conn, surface->window_id);
    } else {
        xcb_map_window(g_xcb_conn, surface->window_id);
    }
    
    xcb_flush(g_xcb_conn);
    surface->props.minimized = minimized;
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// معالجة أحداث XCB
static int handle_xcb_event(int fd, uint32_t mask, void* data) {
    if (!g_xcb_conn) return 0;
    
    xcb_generic_event_t* event;
    while ((event = xcb_poll_for_event(g_xcb_conn))) {
        uint8_t response_type = event->response_type & ~0x80;
        
        switch (response_type) {
            case XCB_CREATE_NOTIFY: {
                xcb_create_notify_event_t* e = (xcb_create_notify_event_t*)event;
                if (e->override_redirect) break;
                
                struct xwayland_surface* surface = xwayland_surface_create(e->window);
                if (surface) {
                    surface->props.x = e->x;
                    surface->props.y = e->y;
                    surface->props.width = e->width;
                    surface->props.height = e->height;
                }
                break;
            }
            
            case XCB_DESTROY_NOTIFY: {
                xcb_destroy_notify_event_t* e = (xcb_destroy_notify_event_t*)event;
                struct xwayland_surface* surface = xwayland_surface_lookup(e->window);
                if (surface) {
                    xwayland_surface_destroy(surface);
                }
                break;
            }
            
            case XCB_MAP_REQUEST: {
                xcb_map_request_event_t* e = (xcb_map_request_event_t*)event;
                struct xwayland_surface* surface = xwayland_surface_lookup(e->window);
                if (surface) {
                    xcb_map_window(g_xcb_conn, e->window);
                    xwayland_surface_set_mapped(surface, true);
                }
                break;
            }
            
            case XCB_UNMAP_NOTIFY: {
                xcb_unmap_notify_event_t* e = (xcb_unmap_notify_event_t*)event;
                struct xwayland_surface* surface = xwayland_surface_lookup(e->window);
                if (surface) {
                    xwayland_surface_set_mapped(surface, false);
                }
                break;
            }
            
            case XCB_CONFIGURE_REQUEST: {
                xcb_configure_request_event_t* e = (xcb_configure_request_event_t*)event;
                struct xwayland_surface* surface = xwayland_surface_lookup(e->window);
                if (surface) {
                    xwayland_surface_configure(surface, e->x, e->y, e->width, e->height);
                }
                break;
            }
            
            case XCB_PROPERTY_NOTIFY: {
                xcb_property_notify_event_t* e = (xcb_property_notify_event_t*)event;
                struct xwayland_surface* surface = xwayland_surface_lookup(e->window);
                if (surface) {
                    read_window_properties(surface);
                }
                break;
            }
            
            case XCB_CLIENT_MESSAGE: {
                xcb_client_message_event_t* e = (xcb_client_message_event_t*)event;
                // معالجة رسائل العميل
                break;
            }
            
            case XCB_FOCUS_IN: {
                xcb_focus_in_event_t* e = (xcb_focus_in_event_t*)event;
                struct xwayland_surface* surface = xwayland_surface_lookup(e->event);
                if (surface) {
                    xwayland_set_focused_surface(surface);
                }
                break;
            }
        }
        
        free(event);
    }
    
    xcb_flush(g_xcb_conn);
    return 0;
}

// تهيئة XCB WM
static int init_xcb_wm(void) {
    int screen_num;
    g_xcb_conn = xcb_connect(g_xwayland.display_name, &screen_num);
    if (!g_xcb_conn || xcb_connection_has_error(g_xcb_conn)) {
        snprintf(g_xwayland.error_message, sizeof(g_xwayland.error_message),
                 "Failed to connect to X server");
        return -1;
    }
    
    g_xcb_screen = xcb_aux_get_screen(g_xcb_conn, screen_num);
    if (!g_xcb_screen) {
        xcb_disconnect(g_xcb_conn);
        g_xcb_conn = NULL;
        return -1;
    }
    
    // تهيئة EWMH
    xcb_intern_atom_cookie_t* cookie = xcb_ewmh_init_atoms(g_xcb_conn, &g_xcb_ewmh);
    if (!xcb_ewmh_init_atoms_replies(&g_xcb_ewmh, cookie, NULL)) {
        xcb_disconnect(g_xcb_conn);
        g_xcb_conn = NULL;
        return -1;
    }
    
    // إنشاء نافذة مدير النوافذ
    g_wm_window = xcb_generate_id(g_xcb_conn);
    xcb_create_window(g_xcb_conn, XCB_COPY_FROM_PARENT, g_wm_window,
                      g_xcb_screen->root, 0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_ONLY,
                      XCB_COPY_FROM_PARENT, 0, NULL);
    
    // تعيين خصائص مدير النوافذ
    xcb_change_property(g_xcb_conn, XCB_PROP_MODE_REPLACE, g_wm_window,
                        g_xcb_ewmh._NET_SUPPORTING_WM_CHECK,
                        XCB_ATOM_WINDOW, 32, 1, &g_wm_window);
    xcb_change_property(g_xcb_conn, XCB_PROP_MODE_REPLACE, g_xcb_screen->root,
                        g_xcb_ewmh._NET_SUPPORTING_WM_CHECK,
                        XCB_ATOM_WINDOW, 32, 1, &g_wm_window);
    
    // تعيين اسم مدير النوافذ
    xcb_ewmh_set_wm_name(&g_xcb_ewmh, g_wm_window, strlen(WM_CLASS), WM_CLASS);
    
    // تعيين الدعم
    xcb_atom_t supported[] = {
        g_xcb_ewmh._NET_SUPPORTED,
        g_xcb_ewmh._NET_SUPPORTING_WM_CHECK,
        g_xcb_ewmh._NET_WM_NAME,
        g_xcb_ewmh._NET_WM_VISIBLE_NAME,
        g_xcb_ewmh._NET_WM_WINDOW_TYPE,
        g_xcb_ewmh._NET_WM_STATE,
        g_xcb_ewmh._NET_WM_STATE_FULLSCREEN,
        g_xcb_ewmh._NET_WM_STATE_MAXIMIZED_VERT,
        g_xcb_ewmh._NET_WM_STATE_MAXIMIZED_HORZ,
        g_xcb_ewmh._NET_WM_STATE_HIDDEN,
        g_xcb_ewmh._NET_WM_STATE_MODAL,
        g_xcb_ewmh._NET_WM_STATE_ABOVE,
        g_xcb_ewmh._NET_WM_STATE_BELOW,
        g_xcb_ewmh._NET_WM_STATE_STICKY,
        g_xcb_ewmh._NET_WM_STATE_SKIP_TASKBAR,
        g_xcb_ewmh._NET_WM_STATE_SKIP_PAGER,
        g_xcb_ewmh._NET_WM_DESKTOP,
        g_xcb_ewmh._NET_NUMBER_OF_DESKTOPS,
        g_xcb_ewmh._NET_CURRENT_DESKTOP,
        g_xcb_ewmh._NET_ACTIVE_WINDOW,
        g_xcb_ewmh._NET_CLOSE_WINDOW,
        g_xcb_ewmh._NET_MOVERESIZE_WINDOW,
        g_xcb_ewmh._NET_WM_MOVERESIZE,
        g_xcb_ewmh._NET_FRAME_EXTENTS,
        g_xcb_ewmh._NET_REQUEST_FRAME_EXTENTS,
    };
    
    xcb_ewmh_set_supported(&g_xcb_ewmh, screen_num,
                           sizeof(supported) / sizeof(supported[0]), supported);
    
    // اختيار أحداث الجذر
    uint32_t values[] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        XCB_EVENT_MASK_PROPERTY_CHANGE
    };
    xcb_change_window_attributes(g_xcb_conn, g_xcb_screen->root,
                                  XCB_CW_EVENT_MASK, values);
    
    xcb_flush(g_xcb_conn);
    
    // الحصول على وصف الملف
    g_xcb_fd = xcb_get_file_descriptor(g_xcb_conn);
    
    // إضافة مصدر الحدث
    struct wl_event_loop* loop = wl_display_get_event_loop(g_xwayland.wl_display);
    g_xcb_event_source = wl_event_loop_add_fd(loop, g_xcb_fd, WL_EVENT_READABLE,
                                               handle_xcb_event, NULL);
    
    return 0;
}

// تشغيل XWayland
int xwayland_start(void) {
    pthread_mutex_lock(&g_xwayland_mutex);
    
    if (g_xwayland.state == XWAYLAND_STATE_RUNNING ||
        g_xwayland.state == XWAYLAND_STATE_STARTING) {
        pthread_mutex_unlock(&g_xwayland_mutex);
        return 0;
    }
    
    g_xwayland.state = XWAYLAND_STATE_STARTING;
    
    // العثور على عرض متاح
    g_xwayland.display_number = find_available_display();
    if (g_xwayland.display_number < 0) {
        snprintf(g_xwayland.error_message, sizeof(g_xwayland.error_message),
                 "No available X11 display");
        g_xwayland.state = XWAYLAND_STATE_ERROR;
        pthread_mutex_unlock(&g_xwayland_mutex);
        return -1;
    }
    
    snprintf(g_xwayland.display_name, sizeof(g_xwayland.display_name),
             ":%d", g_xwayland.display_number);
    
    // إنشاء مقبس X11
    g_xwayland.listen_fd = create_x11_socket(g_xwayland.display_number);
    if (g_xwayland.listen_fd < 0) {
        snprintf(g_xwayland.error_message, sizeof(g_xwayland.error_message),
                 "Failed to create X11 socket");
        g_xwayland.state = XWAYLAND_STATE_ERROR;
        pthread_mutex_unlock(&g_xwayland_mutex);
        return -1;
    }
    
    // إنشاء أنابيب WM
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, g_xwayland.wm_fd) < 0) {
        close(g_xwayland.listen_fd);
        g_xwayland.state = XWAYLAND_STATE_ERROR;
        pthread_mutex_unlock(&g_xwayland_mutex);
        return -1;
    }
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    
    // تشغيل XWayland
    pid_t pid = fork();
    if (pid < 0) {
        close(g_xwayland.listen_fd);
        close(g_xwayland.wm_fd[0]);
        close(g_xwayland.wm_fd[1]);
        pthread_mutex_lock(&g_xwayland_mutex);
        g_xwayland.state = XWAYLAND_STATE_ERROR;
        pthread_mutex_unlock(&g_xwayland_mutex);
        return -1;
    }
    
    if (pid == 0) {
        // العملية الفرعية - XWayland
        close(g_xwayland.wm_fd[0]);
        
        char listen_fd_str[16];
        char wm_fd_str[16];
        snprintf(listen_fd_str, sizeof(listen_fd_str), "%d", g_xwayland.listen_fd);
        snprintf(wm_fd_str, sizeof(wm_fd_str), "%d", g_xwayland.wm_fd[1]);
        
        // تعيين متغيرات البيئة
        setenv("DISPLAY", g_xwayland.display_name, 1);
        
        char* args[] = {
            "Xwayland",
            g_xwayland.display_name,
            "-rootless",
            "-core",
            "-listen", listen_fd_str,
            "-wm", wm_fd_str,
            g_xwayland.no_touch_pointer ? "-noTouchPointer" : NULL,
            NULL
        };
        
        // إزالة NULLs
        int argc = 0;
        while (args[argc]) argc++;
        
        execvp("Xwayland", args);
        _exit(1);
    }
    
    // العملية الأم
    pthread_mutex_lock(&g_xwayland_mutex);
    g_xwayland.pid = pid;
    close(g_xwayland.wm_fd[1]);
    
    // انتظار جاهزية XWayland
    close(g_xwayland.listen_fd);
    
    // انتظار الاتصال
    int client_fd = accept(g_xwayland.listen_fd, NULL, NULL);
    if (client_fd >= 0) {
        close(client_fd);
    }
    
    // تهيئة XCB WM
    if (init_xcb_wm() < 0) {
        kill(pid, SIGTERM);
        g_xwayland.state = XWAYLAND_STATE_ERROR;
        pthread_mutex_unlock(&g_xwayland_mutex);
        return -1;
    }
    
    g_xwayland.state = XWAYLAND_STATE_RUNNING;
    
    // إشعار الجاهزية
    if (g_xwayland.on_ready) {
        g_xwayland.on_ready(&g_xwayland);
    }
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// إيقاف XWayland
int xwayland_stop(void) {
    pthread_mutex_lock(&g_xwayland_mutex);
    
    if (g_xwayland.state != XWAYLAND_STATE_RUNNING) {
        pthread_mutex_unlock(&g_xwayland_mutex);
        return 0;
    }
    
    // إيقاف XCB
    if (g_xcb_event_source) {
        wl_event_source_remove(g_xcb_event_source);
        g_xcb_event_source = NULL;
    }
    
    if (g_xcb_conn) {
        xcb_disconnect(g_xcb_conn);
        g_xcb_conn = NULL;
    }
    
    // إيقاف XWayland
    if (g_xwayland.pid > 0) {
        kill(g_xwayland.pid, SIGTERM);
        
        // انتظار الإنهاء
        int status;
        waitpid(g_xwayland.pid, &status, 0);
        g_xwayland.pid = -1;
    }
    
    // تنظيف النوافذ
    struct xwayland_surface* surface;
    struct xwayland_surface* tmp;
    wl_list_for_each_safe(surface, tmp, &g_xwayland.surfaces, link) {
        xwayland_surface_destroy(surface);
    }
    
    // إزالة ملف المقبس
    char path[64];
    snprintf(path, sizeof(path), "%s/X%d", XWAYLAND_SOCKET_DIR, 
             g_xwayland.display_number);
    unlink(path);
    
    g_xwayland.state = XWAYLAND_STATE_STOPPED;
    g_xwayland.surface_count = 0;
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// إعادة التشغيل
int xwayland_restart(void) {
    xwayland_stop();
    usleep(500000);  // انتظر نصف ثانية
    return xwayland_start();
}

// التهيئة
int xwayland_init(struct wl_display* display) {
    pthread_mutex_lock(&g_xwayland_mutex);
    
    if (g_xwayland.initialized) {
        pthread_mutex_unlock(&g_xwayland_mutex);
        return 0;
    }
    
    memset(&g_xwayland, 0, sizeof(g_xwayland));
    g_xwayland.wl_display = display;
    g_xwayland.pid = -1;
    g_xwayland.state = XWAYLAND_STATE_STOPPED;
    
    wl_list_init(&g_xwayland.surfaces);
    wl_list_init(&g_xwayland.unpaired_surfaces);
    
    // تثبيت معالج SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
    
    g_xwayland.initialized = 1;
    
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

// الإنهاء
void xwayland_terminate(void) {
    xwayland_stop();
    
    pthread_mutex_lock(&g_xwayland_mutex);
    g_xwayland.initialized = 0;
    pthread_mutex_unlock(&g_xwayland_mutex);
}

// الحصول على السياق
struct xwayland_context* xwayland_get_context(void) {
    return &g_xwayland;
}

// التحقق من التشغيل
bool xwayland_is_running(void) {
    pthread_mutex_lock(&g_xwayland_mutex);
    bool running = (g_xwayland.state == XWAYLAND_STATE_RUNNING);
    pthread_mutex_unlock(&g_xwayland_mutex);
    return running;
}

// الحصول على الحالة
xwayland_state_t xwayland_get_state(void) {
    pthread_mutex_lock(&g_xwayland_mutex);
    xwayland_state_t state = g_xwayland.state;
    pthread_mutex_unlock(&g_xwayland_mutex);
    return state;
}

const char* xwayland_get_state_string(void) {
    switch (xwayland_get_state()) {
        case XWAYLAND_STATE_STOPPED: return "stopped";
        case XWAYLAND_STATE_STARTING: return "starting";
        case XWAYLAND_STATE_RUNNING: return "running";
        case XWAYLAND_STATE_ERROR: return "error";
        default: return "unknown";
    }
}

const char* xwayland_get_display_name(void) {
    return g_xwayland.display_name;
}

int xwayland_get_display_number(void) {
    return g_xwayland.display_number;
}

// التركيز
int xwayland_set_focused_surface(struct xwayland_surface* surface) {
    pthread_mutex_lock(&g_xwayland_mutex);
    g_xwayland.focused_surface = surface;
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

struct xwayland_surface* xwayland_get_focused_surface(void) {
    pthread_mutex_lock(&g_xwayland_mutex);
    struct xwayland_surface* surface = g_xwayland.focused_surface;
    pthread_mutex_unlock(&g_xwayland_mutex);
    return surface;
}

int xwayland_activate_surface(struct xwayland_surface* surface) {
    if (!surface) return -1;
    return xwayland_surface_set_activated(surface, true);
}

// المؤشر
int xwayland_set_cursor_surface(struct xwayland_surface* surface) {
    pthread_mutex_lock(&g_xwayland_mutex);
    g_xwayland.cursor_surface = surface;
    pthread_mutex_unlock(&g_xwayland_mutex);
    return 0;
}

struct xwayland_surface* xwayland_get_cursor_surface(void) {
    pthread_mutex_lock(&g_xwayland_mutex);
    struct xwayland_surface* surface = g_xwayland.cursor_surface;
    pthread_mutex_unlock(&g_xwayland_mutex);
    return surface;
}

// التكوين
void xwayland_set_lazy_mode(bool lazy) {
    pthread_mutex_lock(&g_xwayland_mutex);
    g_xwayland.lazy_mode = lazy;
    pthread_mutex_unlock(&g_xwayland_mutex);
}

void xwayland_set_terminate_delay(bool delay) {
    pthread_mutex_lock(&g_xwayland_mutex);
    g_xwayland.terminate_delay = delay;
    pthread_mutex_unlock(&g_xwayland_mutex);
}

void xwayland_set_no_touch_pointer(bool no_touch) {
    pthread_mutex_lock(&g_xwayland_mutex);
    g_xwayland.no_touch_pointer = no_touch;
    pthread_mutex_unlock(&g_xwayland_mutex);
}

// معاودات الاتصال
void xwayland_set_callbacks(
    void (*on_ready)(struct xwayland_context* ctx),
    void (*on_surface_create)(struct xwayland_context* ctx,
                               struct xwayland_surface* surface),
    void (*on_surface_destroy)(struct xwayland_context* ctx,
                                struct xwayland_surface* surface),
    void (*on_surface_activate)(struct xwayland_context* ctx,
                                 struct xwayland_surface* surface)
) {
    pthread_mutex_lock(&g_xwayland_mutex);
    g_xwayland.on_ready = on_ready;
    g_xwayland.on_surface_create = on_surface_create;
    g_xwayland.on_surface_destroy = on_surface_destroy;
    g_xwayland.on_surface_activate = on_surface_activate;
    pthread_mutex_unlock(&g_xwayland_mutex);
}

// التحقق من سطح X11
bool xwayland_is_x11_surface(struct wl_surface* surface) {
    // TODO: التحقق مما إذا كان السطح ينتمي إلى XWayland
    return false;
}

// الإحصائيات
int xwayland_get_surface_count(void) {
    pthread_mutex_lock(&g_xwayland_mutex);
    int count = g_xwayland.surface_count;
    pthread_mutex_unlock(&g_xwayland_mutex);
    return count;
}

void xwayland_get_stats(int* x11_windows, int* wayland_windows, 
                        int* total_windows) {
    pthread_mutex_lock(&g_xwayland_mutex);
    if (x11_windows) *x11_windows = g_xwayland.surface_count;
    // TODO: عد نوافذ Wayland
    if (wayland_windows) *wayland_windows = 0;
    if (total_windows) *total_windows = g_xwayland.surface_count;
    pthread_mutex_unlock(&g_xwayland_mutex);
}

// الأخطاء
const char* xwayland_get_last_error(void) {
    return g_xwayland.error_message;
}

void xwayland_clear_error(void) {
    pthread_mutex_lock(&g_xwayland_mutex);
    g_xwayland.error_message[0] = '\0';
    pthread_mutex_unlock(&g_xwayland_mutex);
}
