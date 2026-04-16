#ifndef HEX_SETTINGS_H
#define HEX_SETTINGS_H

#include <gtk/gtk.h>

typedef struct {
    char font[256];
    int  font_size;
    char gui_font[256];
    int  gui_font_size;
    char theme[64];
    int  bytes_per_row;       /* 8 or 16 */
    gboolean show_ascii;
    gboolean uppercase_hex;
    int  display_mode;        /* 0=hex, 1=binary */
    gboolean show_inspector;
    int  window_width;
    int  window_height;
    char last_file[2048];
    char recent_files[10][2048];
    int  recent_count;
} HexSettings;

void     hex_settings_load(HexSettings *s);
void     hex_settings_save(const HexSettings *s);
void     hex_settings_add_recent(HexSettings *s, const char *path);
char    *hex_settings_get_config_path(void);

/* SFTP/SSH connections */
#define MAX_CONNECTIONS 32

typedef struct {
    char name[128];
    char host[256];
    int  port;
    char user[128];
    char remote_path[1024];
    gboolean use_key;
    char key_path[1024];
} SftpConnection;

typedef struct {
    SftpConnection items[MAX_CONNECTIONS];
    int count;
} SftpConnections;

void connections_load(SftpConnections *c);
void connections_save(const SftpConnections *c);

#endif
