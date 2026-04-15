#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gstdio.h>

static void ensure_config_dir(void) {
    char path[1024];
    const char *config = g_get_user_config_dir();
    snprintf(path, sizeof(path), "%s/hex-editor", config);
    g_mkdir_with_parents(path, 0755);
}

char *hex_settings_get_config_path(void) {
    static char path[1024];
    const char *config = g_get_user_config_dir();
    snprintf(path, sizeof(path), "%s/hex-editor/settings.conf", config);
    return path;
}

void hex_settings_load(HexSettings *s) {
    strncpy(s->font, "Monospace", sizeof(s->font) - 1);
    s->font_size = 13;
    strncpy(s->gui_font, "Sans", sizeof(s->gui_font) - 1);
    s->gui_font_size = 10;
    strncpy(s->theme, "system", sizeof(s->theme) - 1);
    s->bytes_per_row = 16;
    s->show_ascii = TRUE;
    s->uppercase_hex = TRUE;
    s->display_mode = 0;
    s->window_width = 820;
    s->window_height = 600;
    s->last_file[0] = '\0';

    char *path = hex_settings_get_config_path();
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = g_strstrip(line);
        char *val = g_strstrip(eq + 1);

        #define SAFE_COPY(dst, src) do { \
            strncpy((dst), (src), sizeof(dst) - 1); \
            (dst)[sizeof(dst) - 1] = '\0'; \
        } while (0)

        if (strcmp(key, "font") == 0) {
            if (val[0]) SAFE_COPY(s->font, val);
        } else if (strcmp(key, "font_size") == 0) {
            int v = atoi(val); if (v >= 6 && v <= 72) s->font_size = v;
        } else if (strcmp(key, "gui_font") == 0) {
            if (val[0]) SAFE_COPY(s->gui_font, val);
        } else if (strcmp(key, "gui_font_size") == 0) {
            int v = atoi(val); if (v >= 6 && v <= 72) s->gui_font_size = v;
        } else if (strcmp(key, "theme") == 0) {
            if (val[0]) SAFE_COPY(s->theme, val);
        } else if (strcmp(key, "bytes_per_row") == 0) {
            int v = atoi(val); if (v == 8 || v == 16 || v == 32) s->bytes_per_row = v;
        } else if (strcmp(key, "show_ascii") == 0)
            s->show_ascii = (strcmp(val, "1") == 0);
        else if (strcmp(key, "uppercase_hex") == 0)
            s->uppercase_hex = (strcmp(val, "1") == 0);
        else if (strcmp(key, "display_mode") == 0) {
            int v = atoi(val); if (v == 0 || v == 1) s->display_mode = v;
        } else if (strcmp(key, "window_width") == 0) {
            int v = atoi(val); if (v >= 200 && v <= 8192) s->window_width = v;
        } else if (strcmp(key, "window_height") == 0) {
            int v = atoi(val); if (v >= 200 && v <= 8192) s->window_height = v;
        } else if (strcmp(key, "last_file") == 0)
            SAFE_COPY(s->last_file, val);

        #undef SAFE_COPY
    }
    fclose(f);
}

void hex_settings_save(const HexSettings *s) {
    ensure_config_dir();
    char *path = hex_settings_get_config_path();

    char tmp[1088];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    int fd = g_open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return;
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); return; }

    fprintf(f, "font=%s\n", s->font);
    fprintf(f, "font_size=%d\n", s->font_size);
    fprintf(f, "gui_font=%s\n", s->gui_font);
    fprintf(f, "gui_font_size=%d\n", s->gui_font_size);
    fprintf(f, "theme=%s\n", s->theme);
    fprintf(f, "bytes_per_row=%d\n", s->bytes_per_row);
    fprintf(f, "show_ascii=%d\n", s->show_ascii);
    fprintf(f, "uppercase_hex=%d\n", s->uppercase_hex);
    fprintf(f, "display_mode=%d\n", s->display_mode);
    fprintf(f, "window_width=%d\n", s->window_width);
    fprintf(f, "window_height=%d\n", s->window_height);
    fprintf(f, "last_file=%s\n", s->last_file);

    gboolean ok = (fflush(f) == 0);
    fclose(f);
    if (ok)
        g_rename(tmp, path);
    else
        g_remove(tmp);
}
