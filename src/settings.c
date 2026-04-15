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

/* ── SFTP Connections ── */

static char *connections_get_path(void) {
    static char path[1024];
    const char *config = g_get_user_config_dir();
    snprintf(path, sizeof(path), "%s/hex-editor/connections.conf", config);
    return path;
}

void connections_load(SftpConnections *c) {
    memset(c, 0, sizeof(*c));
    FILE *f = fopen(connections_get_path(), "r");
    if (!f) return;

    char line[4096];
    int idx = -1;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '[') {
            idx++;
            if (idx >= MAX_CONNECTIONS) break;
            c->count = idx + 1;
            char *end = strchr(line, ']');
            if (end) *end = '\0';
            g_strlcpy(c->items[idx].name, line + 1, sizeof(c->items[idx].name));
            c->items[idx].port = 22;
            continue;
        }
        if (idx < 0 || idx >= MAX_CONNECTIONS) continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line, *val = eq + 1;
        SftpConnection *s = &c->items[idx];
        if (strcmp(key, "host") == 0) g_strlcpy(s->host, val, sizeof(s->host));
        else if (strcmp(key, "port") == 0) s->port = atoi(val);
        else if (strcmp(key, "user") == 0) g_strlcpy(s->user, val, sizeof(s->user));
        else if (strcmp(key, "remote_path") == 0) g_strlcpy(s->remote_path, val, sizeof(s->remote_path));
        else if (strcmp(key, "use_key") == 0) s->use_key = atoi(val);
        else if (strcmp(key, "key_path") == 0) g_strlcpy(s->key_path, val, sizeof(s->key_path));
    }
    fclose(f);
}

void connections_save(const SftpConnections *c) {
    ensure_config_dir();
    FILE *f = fopen(connections_get_path(), "w");
    if (!f) return;
    fchmod(fileno(f), 0600);
    for (int i = 0; i < c->count; i++) {
        const SftpConnection *s = &c->items[i];
        fprintf(f, "[%s]\n", s->name);
        fprintf(f, "host=%s\n", s->host);
        fprintf(f, "port=%d\n", s->port);
        fprintf(f, "user=%s\n", s->user);
        fprintf(f, "remote_path=%s\n", s->remote_path);
        fprintf(f, "use_key=%d\n", s->use_key);
        fprintf(f, "key_path=%s\n", s->key_path);
        fprintf(f, "\n");
    }
    fclose(f);
}
