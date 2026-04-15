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
    int  window_width;
    int  window_height;
    char last_file[2048];
} HexSettings;

void     hex_settings_load(HexSettings *s);
void     hex_settings_save(const HexSettings *s);
char    *hex_settings_get_config_path(void);

#endif
