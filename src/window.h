#ifndef HEX_WINDOW_H
#define HEX_WINDOW_H

#include <gtk/gtk.h>
#include "settings.h"

typedef struct {
    gsize         offset;
    unsigned char old_val;
    unsigned char new_val;
    gboolean      was_insert; /* TRUE if this byte was appended (grew data_len) */
} HexUndoEntry;

typedef struct {
    GtkApplicationWindow *window;
    GtkDrawingArea       *hex_view;
    GtkLabel             *status_offset;
    GtkLabel             *status_value;
    GtkLabel             *status_size;
    GtkLabel             *status_scroll;
    GtkLabel             *inspector_label;
    GtkWidget            *inspector_panel;
    GtkDrawingArea       *entropy_bar;
    double               *entropy_data;
    int                   entropy_blocks;
    HexSettings           settings;
    GtkCssProvider       *css_provider;
    char                  current_file[2048];

    /* File data */
    unsigned char        *data;
    gsize                 data_len;
    gsize                 data_alloc;
    unsigned char        *original_data;
    gsize                 original_len;
    gboolean              dirty;

    /* Undo/Redo */
    HexUndoEntry         *undo_stack;
    int                   undo_count;
    int                   undo_alloc;
    int                   undo_pos;

    /* Cursor / selection */
    gsize                 cursor_pos;     /* byte offset */
    int                   cursor_nibble;  /* 0=high, 1=low */
    gboolean              editing_ascii;  /* cursor in ASCII pane */
    gsize                 selection_start;
    gsize                 selection_end;
    gboolean              selecting;

    /* View state */
    gsize                 scroll_offset;  /* first visible row */
    int                   visible_rows;
    int                   row_height;
    int                   char_width;

    /* Search & Replace */
    GtkWidget            *search_bar;
    GtkWidget            *search_entry;
    GtkWidget            *replace_entry;
    GtkWidget            *replace_box;
    GtkLabel             *match_label;
    gsize                *match_offsets;
    int                   match_count;
    int                   match_current;

    /* Goto offset dialog */
    guint                 redraw_idle_id;

    /* SSH/SFTP state */
    char                  ssh_host[256];
    char                  ssh_user[128];
    int                   ssh_port;
    char                  ssh_key[1024];
    char                  ssh_remote_path[1024];
    char                  ssh_mount[2048];
    char                  ssh_ctl_path[512];
    char                  ssh_ctl_dir[256];
    GtkWidget            *ssh_status_btn;
} HexWindow;

HexWindow *hex_window_new(GtkApplication *app);
void       hex_window_apply_settings(HexWindow *win);
void       hex_window_load_file(HexWindow *win, const char *path);
void       hex_window_show_search(HexWindow *win);
void       hex_window_show_find_replace(HexWindow *win);
void       hex_window_goto_offset(HexWindow *win);
void       hex_window_undo(HexWindow *win);
void       hex_window_redo(HexWindow *win);
void       hex_window_copy(HexWindow *win);
void       hex_window_paste(HexWindow *win);
unsigned char *hex_parse_hex_string(const char *text, gsize *out_len);
void       hex_window_queue_redraw(HexWindow *win);
void       hex_window_ssh_connect(HexWindow *win, const char *host,
                                   const char *user, int port,
                                   const char *key, const char *remote_path);
void       hex_window_ssh_disconnect(HexWindow *win);
gboolean   hex_window_is_remote(HexWindow *win);
void       hex_window_open_remote_file(HexWindow *win, const char *remote_path);
gboolean   hex_window_save_remote(HexWindow *win);

#endif
