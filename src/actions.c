#include "actions.h"
#include "ssh.h"
#include <adwaita.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* ── Theme list ── */

static const char *theme_ids[] = {
    "system", "light", "dark",
    "solarized-light", "solarized-dark",
    "monokai",
    "gruvbox-light", "gruvbox-dark",
    "nord", "dracula", "tokyo-night",
    "catppuccin-latte", "catppuccin-mocha",
    NULL
};
static const char *theme_labels[] = {
    "System", "Light", "Dark",
    "Solarized Light", "Solarized Dark",
    "Monokai",
    "Gruvbox Light", "Gruvbox Dark",
    "Nord", "Dracula", "Tokyo Night",
    "Catppuccin Latte", "Catppuccin Mocha",
    NULL
};

static int theme_index_of(const char *id) {
    for (int i = 0; theme_ids[i]; i++)
        if (strcmp(theme_ids[i], id) == 0) return i;
    return 0;
}

/* ── Actions ── */

static void on_new_file(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;

    g_free(win->data);
    g_free(win->original_data);
    win->data_alloc = 4096;
    win->data = g_malloc0(win->data_alloc);
    win->data_len = 0;
    win->original_data = NULL;
    win->original_len = 0;
    win->dirty = FALSE;
    win->cursor_pos = 0;
    win->cursor_nibble = 0;
    win->scroll_offset = 0;
    win->selection_start = 0;
    win->selection_end = 0;
    win->current_file[0] = '\0';
    win->settings.last_file[0] = '\0';
    hex_settings_save(&win->settings);
    gtk_window_set_title(GTK_WINDOW(win->window), "Hex Editor");
    hex_window_queue_redraw(win);
    gtk_widget_grab_focus(GTK_WIDGET(win->hex_view));
}

static void on_save(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;
    if (!win->dirty || !win->data) return;

    /* Remote save */
    if (hex_window_is_remote(win) && ssh_path_is_remote(win->current_file)) {
        hex_window_save_remote(win);
        return;
    }

    if (win->current_file[0] != '\0') {
        char tmp[2112];
        snprintf(tmp, sizeof(tmp), "%s.tmp", win->current_file);
        FILE *f = fopen(tmp, "wb");
        if (f) {
            gboolean ok = (fwrite(win->data, 1, win->data_len, f) == win->data_len);
            ok = ok && (fflush(f) == 0);
            fclose(f);
            if (ok)
                g_rename(tmp, win->current_file);
            else
                g_remove(tmp);
        }
        g_free(win->original_data);
        win->original_data = win->data_len > 0 ? g_memdup2(win->data, win->data_len) : NULL;
        win->original_len = win->original_data ? win->data_len : 0;
        win->dirty = FALSE;

        char *base = g_path_get_basename(win->current_file);
        gtk_window_set_title(GTK_WINDOW(win->window), base);
        g_free(base);
    } else {
        g_action_group_activate_action(G_ACTION_GROUP(win->window), "save-as", NULL);
    }
}

static void on_save_as_cb(GObject *source, GAsyncResult *result, gpointer data) {
    HexWindow *win = data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GFile *file = gtk_file_dialog_save_finish(dialog, result, NULL);
    if (file) {
        char *path = g_file_get_path(file);
        if (path && win->data) {
            char tmp[2112];
            snprintf(tmp, sizeof(tmp), "%s.tmp", path);
            FILE *f = fopen(tmp, "wb");
            if (f) {
                gboolean ok = (fwrite(win->data, 1, win->data_len, f) == win->data_len);
                ok = ok && (fflush(f) == 0);
                fclose(f);
                if (ok)
                    g_rename(tmp, path);
                else
                    g_remove(tmp);
            }

            snprintf(win->current_file, sizeof(win->current_file), "%s", path);
            snprintf(win->settings.last_file, sizeof(win->settings.last_file), "%s", path);
            g_free(win->original_data);
            win->original_data = win->data_len > 0 ? g_memdup2(win->data, win->data_len) : NULL;
            win->original_len = win->original_data ? win->data_len : 0;
            win->dirty = FALSE;
            hex_settings_save(&win->settings);

            char *base = g_path_get_basename(path);
            gtk_window_set_title(GTK_WINDOW(win->window), base);
            g_free(base);
            g_free(path);
        }
        g_object_unref(file);
    }
    g_object_unref(dialog);
}

static void on_save_as(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Save As");
    gtk_file_dialog_save(dialog, GTK_WINDOW(win->window), NULL, on_save_as_cb, win);
}

static void on_open_file_cb(GObject *source, GAsyncResult *result, gpointer data) {
    HexWindow *win = data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GFile *file = gtk_file_dialog_open_finish(dialog, result, NULL);
    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            hex_window_load_file(win, path);
            g_free(path);
        }
        g_object_unref(file);
    }
    g_object_unref(dialog);
}

static void on_open_file(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open File");
    gtk_file_dialog_open(dialog, GTK_WINDOW(win->window), NULL, on_open_file_cb, win);
}

static void on_find(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    hex_window_show_search(data);
}

static void on_goto_offset(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    hex_window_goto_offset(data);
}

static void on_toggle_binary(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;
    win->settings.display_mode = win->settings.display_mode == 0 ? 1 : 0;
    win->cursor_nibble = 0;
    hex_window_apply_settings(win);
    hex_settings_save(&win->settings);
}

static void on_zoom_in(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;
    if (win->settings.font_size < 72) {
        win->settings.font_size += 2;
        hex_window_apply_settings(win);
        hex_settings_save(&win->settings);
    }
}

static void on_zoom_out(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;
    if (win->settings.font_size > 6) {
        win->settings.font_size -= 2;
        hex_window_apply_settings(win);
        hex_settings_save(&win->settings);
    }
}

/* ── Settings dialog ── */

static void on_settings_apply(GtkButton *button, gpointer data) {
    HexWindow *win = data;
    hex_settings_save(&win->settings);
    hex_window_apply_settings(win);
    GtkWidget *toplevel = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(button)));
    gtk_window_destroy(GTK_WINDOW(toplevel));
}

static void on_settings_cancel(GtkButton *button, gpointer data) {
    (void)data;
    GtkWidget *toplevel = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(button)));
    gtk_window_destroy(GTK_WINDOW(toplevel));
}

static void on_theme_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer data) {
    (void)pspec;
    HexWindow *win = data;
    guint idx = gtk_drop_down_get_selected(dropdown);
    if (theme_ids[idx])
        snprintf(win->settings.theme, sizeof(win->settings.theme), "%s", theme_ids[idx]);
}

static void on_font_set(GtkFontDialogButton *button, GParamSpec *pspec, gpointer data) {
    (void)pspec;
    HexWindow *win = data;
    PangoFontDescription *desc = gtk_font_dialog_button_get_font_desc(button);
    if (desc) {
        const char *family = pango_font_description_get_family(desc);
        if (family)
            snprintf(win->settings.font, sizeof(win->settings.font), "%s", family);
        int size = pango_font_description_get_size(desc);
        if (size > 0)
            win->settings.font_size = size / PANGO_SCALE;
    }
}

static void on_gui_font_set(GtkFontDialogButton *button, GParamSpec *pspec, gpointer data) {
    (void)pspec;
    HexWindow *win = data;
    PangoFontDescription *desc = gtk_font_dialog_button_get_font_desc(button);
    if (desc) {
        const char *family = pango_font_description_get_family(desc);
        if (family)
            snprintf(win->settings.gui_font, sizeof(win->settings.gui_font), "%s", family);
        int size = pango_font_description_get_size(desc);
        if (size > 0)
            win->settings.gui_font_size = size / PANGO_SCALE;
    }
}

static void on_bpr_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer data) {
    (void)pspec;
    HexWindow *win = data;
    guint idx = gtk_drop_down_get_selected(dropdown);
    int values[] = {8, 16, 32};
    if (idx < 3) win->settings.bytes_per_row = values[idx];
}

static void on_show_ascii_toggled(GtkCheckButton *btn, gpointer data) {
    HexWindow *win = data;
    win->settings.show_ascii = gtk_check_button_get_active(btn);
}

static void on_uppercase_toggled(GtkCheckButton *btn, gpointer data) {
    HexWindow *win = data;
    win->settings.uppercase_hex = gtk_check_button_get_active(btn);
}

static void on_settings(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Settings");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, -1);
    gtk_window_set_titlebar(GTK_WINDOW(dialog), adw_header_bar_new());

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_append(GTK_BOX(vbox), grid);

    int row = 0;

    /* Theme */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Theme:"), 0, row, 1, 1);
    GtkWidget *theme_dd = gtk_drop_down_new_from_strings(theme_labels);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(theme_dd), (guint)theme_index_of(win->settings.theme));
    g_signal_connect(theme_dd, "notify::selected", G_CALLBACK(on_theme_changed), win);
    gtk_grid_attach(GTK_GRID(grid), theme_dd, 1, row++, 1, 1);

    /* Font */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Font:"), 0, row, 1, 1);
    PangoFontDescription *desc = pango_font_description_new();
    pango_font_description_set_family(desc, win->settings.font);
    pango_font_description_set_size(desc, win->settings.font_size * PANGO_SCALE);
    GtkFontDialog *font_dialog = gtk_font_dialog_new();
    GtkWidget *font_btn = gtk_font_dialog_button_new(font_dialog);
    gtk_font_dialog_button_set_font_desc(GTK_FONT_DIALOG_BUTTON(font_btn), desc);
    pango_font_description_free(desc);
    g_signal_connect(font_btn, "notify::font-desc", G_CALLBACK(on_font_set), win);
    gtk_grid_attach(GTK_GRID(grid), font_btn, 1, row++, 1, 1);

    /* GUI font */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("GUI Font:"), 0, row, 1, 1);
    PangoFontDescription *gui_desc = pango_font_description_new();
    pango_font_description_set_family(gui_desc, win->settings.gui_font);
    pango_font_description_set_size(gui_desc, win->settings.gui_font_size * PANGO_SCALE);
    GtkFontDialog *gui_font_dialog = gtk_font_dialog_new();
    GtkWidget *gui_font_btn = gtk_font_dialog_button_new(gui_font_dialog);
    gtk_font_dialog_button_set_font_desc(GTK_FONT_DIALOG_BUTTON(gui_font_btn), gui_desc);
    pango_font_description_free(gui_desc);
    g_signal_connect(gui_font_btn, "notify::font-desc", G_CALLBACK(on_gui_font_set), win);
    gtk_grid_attach(GTK_GRID(grid), gui_font_btn, 1, row++, 1, 1);

    /* Bytes per row */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Bytes per Row:"), 0, row, 1, 1);
    const char *bpr_labels[] = {"8", "16", "32", NULL};
    GtkWidget *bpr_dd = gtk_drop_down_new_from_strings(bpr_labels);
    guint bpr_idx = 1;
    if (win->settings.bytes_per_row == 8) bpr_idx = 0;
    else if (win->settings.bytes_per_row == 32) bpr_idx = 2;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(bpr_dd), bpr_idx);
    g_signal_connect(bpr_dd, "notify::selected", G_CALLBACK(on_bpr_changed), win);
    gtk_grid_attach(GTK_GRID(grid), bpr_dd, 1, row++, 1, 1);

    /* Show ASCII */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Show ASCII:"), 0, row, 1, 1);
    GtkWidget *ascii_check = gtk_check_button_new();
    gtk_check_button_set_active(GTK_CHECK_BUTTON(ascii_check), win->settings.show_ascii);
    g_signal_connect(ascii_check, "toggled", G_CALLBACK(on_show_ascii_toggled), win);
    gtk_grid_attach(GTK_GRID(grid), ascii_check, 1, row++, 1, 1);

    /* Uppercase hex */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Uppercase Hex:"), 0, row, 1, 1);
    GtkWidget *upper_check = gtk_check_button_new();
    gtk_check_button_set_active(GTK_CHECK_BUTTON(upper_check), win->settings.uppercase_hex);
    g_signal_connect(upper_check, "toggled", G_CALLBACK(on_uppercase_toggled), win);
    gtk_grid_attach(GTK_GRID(grid), upper_check, 1, row++, 1, 1);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    GtkWidget *apply_btn = gtk_button_new_with_label("Apply");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_settings_cancel), win);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(on_settings_apply), win);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);
    gtk_box_append(GTK_BOX(btn_box), apply_btn);
    gtk_box_append(GTK_BOX(vbox), btn_box);

    gtk_window_present(GTK_WINDOW(dialog));
}

/* ── SFTP/SSH Connection Dialog ── */

static GtkWidget *make_label(const char *text) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 1.0);
    return lbl;
}

typedef struct {
    HexWindow       *win;
    GtkWindow       *dialog;
    SftpConnections  conns;
    GtkListBox      *conn_list;
    GtkEntry        *name_entry;
    GtkEntry        *host_entry;
    GtkEntry        *port_entry;
    GtkEntry        *user_entry;
    GtkEntry        *path_entry;
    GtkEntry        *password_entry;
    GtkCheckButton  *use_key_check;
    GtkEntry        *key_entry;
    GtkWidget       *key_browse_btn;
    GtkWidget       *password_row;
    GtkWidget       *key_row;
    GtkWidget       *key_btn_row;
    int              selected_idx;
} SftpCtx;

static void sftp_populate_list(SftpCtx *ctx) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(ctx->conn_list))))
        gtk_list_box_remove(ctx->conn_list, child);
    for (int i = 0; i < ctx->conns.count; i++) {
        GtkWidget *lbl = gtk_label_new(ctx->conns.items[i].name);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_end(lbl, 8);
        gtk_widget_set_margin_top(lbl, 4);
        gtk_widget_set_margin_bottom(lbl, 4);
        gtk_list_box_append(ctx->conn_list, lbl);
    }
}

static void sftp_update_auth_visibility(SftpCtx *ctx) {
    gboolean use_key = gtk_check_button_get_active(ctx->use_key_check);
    gtk_widget_set_visible(ctx->password_row, !use_key);
    gtk_widget_set_visible(GTK_WIDGET(ctx->password_entry), !use_key);
    gtk_widget_set_visible(ctx->key_row, use_key);
    gtk_widget_set_visible(GTK_WIDGET(ctx->key_entry), use_key);
    gtk_widget_set_visible(ctx->key_btn_row, use_key);
}

static void on_use_key_toggled(GtkCheckButton *btn, gpointer data) {
    (void)btn;
    sftp_update_auth_visibility(data);
}

static void on_key_file_selected(GObject *src, GAsyncResult *res, gpointer data) {
    SftpCtx *ctx = data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(src);
    GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            gtk_editable_set_text(GTK_EDITABLE(ctx->key_entry), path);
            g_free(path);
        }
        g_object_unref(file);
    }
}

static void on_key_browse(GtkButton *btn, gpointer data) {
    (void)btn;
    SftpCtx *ctx = data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Private Key");
    char ssh_dir[1024];
    snprintf(ssh_dir, sizeof(ssh_dir), "%s/.ssh", g_get_home_dir());
    GFile *init = g_file_new_for_path(ssh_dir);
    gtk_file_dialog_set_initial_folder(dialog, init);
    g_object_unref(init);
    gtk_file_dialog_open(dialog, ctx->dialog, NULL, on_key_file_selected, ctx);
}

static void on_conn_selected(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    SftpCtx *ctx = data;
    if (!row) { ctx->selected_idx = -1; return; }
    int idx = gtk_list_box_row_get_index(row);
    if (idx < 0 || idx >= ctx->conns.count) return;
    ctx->selected_idx = idx;
    SftpConnection *c = &ctx->conns.items[idx];
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), c->name);
    gtk_editable_set_text(GTK_EDITABLE(ctx->host_entry), c->host);
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", c->port);
    gtk_editable_set_text(GTK_EDITABLE(ctx->port_entry), port_str);
    gtk_editable_set_text(GTK_EDITABLE(ctx->user_entry), c->user);
    gtk_editable_set_text(GTK_EDITABLE(ctx->path_entry), c->remote_path);
    gtk_check_button_set_active(ctx->use_key_check, c->use_key);
    gtk_editable_set_text(GTK_EDITABLE(ctx->key_entry), c->key_path);
    gtk_editable_set_text(GTK_EDITABLE(ctx->password_entry), "");
    sftp_update_auth_visibility(ctx);
}

static void sftp_save_form_to_conn(SftpCtx *ctx, SftpConnection *c) {
    g_strlcpy(c->name, gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry)), sizeof(c->name));
    g_strlcpy(c->host, gtk_editable_get_text(GTK_EDITABLE(ctx->host_entry)), sizeof(c->host));
    c->port = atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->port_entry)));
    if (c->port <= 0) c->port = 22;
    g_strlcpy(c->user, gtk_editable_get_text(GTK_EDITABLE(ctx->user_entry)), sizeof(c->user));
    g_strlcpy(c->remote_path, gtk_editable_get_text(GTK_EDITABLE(ctx->path_entry)), sizeof(c->remote_path));
    c->use_key = gtk_check_button_get_active(ctx->use_key_check);
    g_strlcpy(c->key_path, gtk_editable_get_text(GTK_EDITABLE(ctx->key_entry)), sizeof(c->key_path));
}

static void on_sftp_save(GtkButton *btn, gpointer data) {
    (void)btn;
    SftpCtx *ctx = data;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry));
    if (!name[0]) return;
    if (ctx->selected_idx >= 0 && ctx->selected_idx < ctx->conns.count) {
        sftp_save_form_to_conn(ctx, &ctx->conns.items[ctx->selected_idx]);
    } else if (ctx->conns.count < MAX_CONNECTIONS) {
        int idx = ctx->conns.count++;
        memset(&ctx->conns.items[idx], 0, sizeof(SftpConnection));
        sftp_save_form_to_conn(ctx, &ctx->conns.items[idx]);
        ctx->selected_idx = idx;
    }
    connections_save(&ctx->conns);
    sftp_populate_list(ctx);
}

static void sftp_clear_form(SftpCtx *ctx);

static void on_sftp_delete(GtkButton *btn, gpointer data) {
    (void)btn;
    SftpCtx *ctx = data;
    if (ctx->selected_idx < 0 || ctx->selected_idx >= ctx->conns.count) return;
    for (int i = ctx->selected_idx; i < ctx->conns.count - 1; i++)
        ctx->conns.items[i] = ctx->conns.items[i + 1];
    ctx->conns.count--;
    connections_save(&ctx->conns);
    sftp_populate_list(ctx);
    sftp_clear_form(ctx);
}

static void sftp_clear_form(SftpCtx *ctx) {
    ctx->selected_idx = -1;
    gtk_list_box_unselect_all(ctx->conn_list);
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->host_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->port_entry), "22");
    gtk_editable_set_text(GTK_EDITABLE(ctx->user_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->path_entry), "/");
    gtk_editable_set_text(GTK_EDITABLE(ctx->password_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->key_entry), "");
    gtk_check_button_set_active(ctx->use_key_check, FALSE);
    sftp_update_auth_visibility(ctx);
}

static void on_sftp_new(GtkButton *btn, gpointer data) {
    (void)btn;
    sftp_clear_form(data);
}

/* Async SSH connect */

typedef struct {
    SftpCtx    *ctx;
    GPtrArray  *argv;
    char        host[256];
    char        user[128];
    int         port;
    char        key[1024];
    char        remote[1024];
    GtkWidget  *connect_btn;
} ConnectTaskData;

static void connect_task_data_free(gpointer p) {
    ConnectTaskData *d = p;
    if (d->argv) g_ptr_array_unref(d->argv);
    g_free(d);
}

static void ssh_connect_thread(GTask *task, gpointer src, gpointer data,
                                GCancellable *cancel) {
    (void)src; (void)cancel;
    ConnectTaskData *d = data;
    g_ptr_array_add(d->argv, NULL);

    char *stdout_buf = NULL;
    GError *err = NULL;
    gint status = 0;
    gboolean ok = g_spawn_sync(
        NULL, (char **)d->argv->pdata, NULL,
        G_SPAWN_SEARCH_PATH,
        NULL, NULL, &stdout_buf, NULL, &status, &err);
    g_free(stdout_buf);

    if (!ok) {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
            "SSH failed: %s", err ? err->message : "unknown");
        if (err) g_error_free(err);
        return;
    }
    if (!g_spawn_check_wait_status(status, NULL)) {
        int code = WIFEXITED(status) ? WEXITSTATUS(status) : status;
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
            "SSH connection failed (exit %d).\nCheck hostname, credentials, and SSH key.", code);
        return;
    }
    g_task_return_boolean(task, TRUE);
}

static void ssh_connect_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    ConnectTaskData *d = data;
    SftpCtx *ctx = d->ctx;
    GError *err = NULL;

    if (!g_task_propagate_boolean(G_TASK(res), &err)) {
        gtk_widget_set_sensitive(d->connect_btn, TRUE);
        gtk_button_set_label(GTK_BUTTON(d->connect_btn), "Connect");
        GtkAlertDialog *alert = gtk_alert_dialog_new("%s",
            err ? err->message : "Unknown SSH error");
        gtk_alert_dialog_show(alert, ctx->dialog);
        g_object_unref(alert);
        if (err) g_error_free(err);
        return;
    }

    hex_window_ssh_connect(ctx->win, d->host, d->user, d->port, d->key, d->remote);
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
}

static void on_sftp_connect(GtkButton *btn, gpointer data) {
    SftpCtx *ctx = data;
    const char *host = gtk_editable_get_text(GTK_EDITABLE(ctx->host_entry));
    const char *user = gtk_editable_get_text(GTK_EDITABLE(ctx->user_entry));
    const char *remote = gtk_editable_get_text(GTK_EDITABLE(ctx->path_entry));
    const char *port_str = gtk_editable_get_text(GTK_EDITABLE(ctx->port_entry));
    gboolean use_key = gtk_check_button_get_active(ctx->use_key_check);

    if (!host[0] || !user[0]) return;
    if (!remote[0]) remote = "/";

    int port = atoi(port_str[0] ? port_str : "22");
    if (port <= 0) port = 22;

    gtk_widget_set_sensitive(GTK_WIDGET(btn), FALSE);
    gtk_button_set_label(btn, "Connecting...");

    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-p"));
    g_ptr_array_add(av, g_strdup_printf("%d", port));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("StrictHostKeyChecking=accept-new"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("BatchMode=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ConnectTimeout=10"));
    if (use_key) {
        const char *key = gtk_editable_get_text(GTK_EDITABLE(ctx->key_entry));
        if (key[0]) {
            g_ptr_array_add(av, g_strdup("-i"));
            g_ptr_array_add(av, g_strdup(key));
        }
    }
    g_ptr_array_add(av, g_strdup_printf("%s@%s", user, host));
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("echo"));
    g_ptr_array_add(av, g_strdup("ok"));

    ConnectTaskData *td = g_new0(ConnectTaskData, 1);
    td->ctx = ctx;
    td->argv = av;
    td->connect_btn = GTK_WIDGET(btn);
    g_strlcpy(td->host, host, sizeof(td->host));
    g_strlcpy(td->user, user, sizeof(td->user));
    td->port = port;
    if (use_key) {
        const char *key = gtk_editable_get_text(GTK_EDITABLE(ctx->key_entry));
        g_strlcpy(td->key, key, sizeof(td->key));
    }
    g_strlcpy(td->remote, remote, sizeof(td->remote));

    GTask *task = g_task_new(NULL, NULL, ssh_connect_done, td);
    g_task_set_task_data(task, td, connect_task_data_free);
    g_task_run_in_thread(task, ssh_connect_thread);
    g_object_unref(task);
}

static void on_sftp_dialog_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    g_free(data);
}

static void on_sftp_dialog(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;

    SftpCtx *ctx = g_new0(SftpCtx, 1);
    ctx->win = win;
    ctx->selected_idx = -1;
    connections_load(&ctx->conns);

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "SFTP/SSH Connection");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 520, 460);
    gtk_window_set_titlebar(GTK_WINDOW(dialog), adw_header_bar_new());
    ctx->dialog = GTK_WINDOW(dialog);

    g_signal_connect(dialog, "destroy", G_CALLBACK(on_sftp_dialog_destroy), ctx);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(hbox, 12);
    gtk_widget_set_margin_end(hbox, 12);
    gtk_widget_set_margin_top(hbox, 12);
    gtk_widget_set_margin_bottom(hbox, 12);
    gtk_window_set_child(GTK_WINDOW(dialog), hbox);

    /* Left: saved connections list */
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(left_box, 160, -1);

    GtkWidget *list_label = gtk_label_new("Connections");
    gtk_label_set_xalign(GTK_LABEL(list_label), 0);
    gtk_box_append(GTK_BOX(left_box), list_label);

    GtkWidget *list_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(list_scroll, TRUE);
    ctx->conn_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(ctx->conn_list, GTK_SELECTION_SINGLE);
    g_signal_connect(ctx->conn_list, "row-activated", G_CALLBACK(on_conn_selected), ctx);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(list_scroll), GTK_WIDGET(ctx->conn_list));
    gtk_box_append(GTK_BOX(left_box), list_scroll);

    gtk_box_append(GTK_BOX(hbox), left_box);
    gtk_box_append(GTK_BOX(hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    /* Right: form */
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(right_box, TRUE);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    int row = 0;

    gtk_grid_attach(GTK_GRID(grid), make_label("Name:"), 0, row, 1, 1);
    ctx->name_entry = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(ctx->name_entry), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->name_entry), 1, row++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), make_label("Host:"), 0, row, 1, 1);
    ctx->host_entry = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(ctx->host_entry), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->host_entry), 1, row++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), make_label("Port:"), 0, row, 1, 1);
    ctx->port_entry = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_text(GTK_EDITABLE(ctx->port_entry), "22");
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->port_entry), 1, row++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), make_label("User:"), 0, row, 1, 1);
    ctx->user_entry = GTK_ENTRY(gtk_entry_new());
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->user_entry), 1, row++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), make_label("Remote Path:"), 0, row, 1, 1);
    ctx->path_entry = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_text(GTK_EDITABLE(ctx->path_entry), "/");
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->path_entry), 1, row++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), make_label("Use Private Key:"), 0, row, 1, 1);
    ctx->use_key_check = GTK_CHECK_BUTTON(gtk_check_button_new());
    g_signal_connect(ctx->use_key_check, "toggled", G_CALLBACK(on_use_key_toggled), ctx);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->use_key_check), 1, row++, 2, 1);

    GtkWidget *pass_lbl = make_label("Password:");
    gtk_grid_attach(GTK_GRID(grid), pass_lbl, 0, row, 1, 1);
    ctx->password_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_visibility(ctx->password_entry, FALSE);
    gtk_widget_set_hexpand(GTK_WIDGET(ctx->password_entry), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->password_entry), 1, row++, 2, 1);
    ctx->password_row = pass_lbl;

    GtkWidget *key_lbl = make_label("Key File:");
    gtk_grid_attach(GTK_GRID(grid), key_lbl, 0, row, 1, 1);
    ctx->key_entry = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(ctx->key_entry), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->key_entry), 1, row, 1, 1);
    ctx->key_row = key_lbl;

    ctx->key_browse_btn = gtk_button_new_with_label("...");
    g_signal_connect(ctx->key_browse_btn, "clicked", G_CALLBACK(on_key_browse), ctx);
    gtk_grid_attach(GTK_GRID(grid), ctx->key_browse_btn, 2, row++, 1, 1);
    ctx->key_btn_row = ctx->key_browse_btn;

    sftp_update_auth_visibility(ctx);
    gtk_box_append(GTK_BOX(right_box), grid);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 12);
    gtk_widget_set_valign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_vexpand(btn_box, TRUE);

    GtkWidget *new_btn = gtk_button_new_with_label("New");
    g_signal_connect(new_btn, "clicked", G_CALLBACK(on_sftp_new), ctx);
    gtk_box_append(GTK_BOX(btn_box), new_btn);

    GtkWidget *del_btn = gtk_button_new_with_label("Delete");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_sftp_delete), ctx);
    gtk_box_append(GTK_BOX(btn_box), del_btn);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_sftp_save), ctx);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    GtkWidget *connect_btn = gtk_button_new_with_label("Connect");
    gtk_widget_add_css_class(connect_btn, "suggested-action");
    g_signal_connect(connect_btn, "clicked", G_CALLBACK(on_sftp_connect), ctx);
    gtk_box_append(GTK_BOX(btn_box), connect_btn);

    gtk_box_append(GTK_BOX(right_box), btn_box);
    gtk_box_append(GTK_BOX(hbox), right_box);

    sftp_populate_list(ctx);
    gtk_window_present(GTK_WINDOW(dialog));
}

/* ── Open Remote File browser ── */

typedef struct {
    HexWindow *win;
    GtkWindow   *dialog;
    GtkLabel    *path_label;
    GtkListBox  *file_list;
    char         current_dir[4096];
} OpenRemoteCtx;

static void remote_browse_populate(OpenRemoteCtx *ctx);

static void on_remote_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    OpenRemoteCtx *ctx = data;
    if (!row) return;

    GtkWidget *lbl = gtk_widget_get_first_child(GTK_WIDGET(row));
    if (!lbl) return;
    const char *name = gtk_label_get_text(GTK_LABEL(lbl));
    if (!name || !name[0]) return;

    size_t nlen = strlen(name);

    if (strcmp(name, "..") == 0) {
        size_t dlen = strlen(ctx->current_dir);
        if (dlen > 1 && ctx->current_dir[dlen - 1] == '/')
            ctx->current_dir[dlen - 1] = '\0';
        char *last = strrchr(ctx->current_dir, '/');
        if (last && last != ctx->current_dir)
            *(last + 1) = '\0';
        else
            g_strlcpy(ctx->current_dir, "/", sizeof(ctx->current_dir));
        remote_browse_populate(ctx);
        return;
    }

    if (name[nlen - 1] == '/') {
        size_t dlen = strlen(ctx->current_dir);
        if (dlen > 0 && ctx->current_dir[dlen - 1] != '/')
            g_strlcat(ctx->current_dir, "/", sizeof(ctx->current_dir));
        g_strlcat(ctx->current_dir, name, sizeof(ctx->current_dir));
        remote_browse_populate(ctx);
        return;
    }

    char full_path[8192];
    size_t dlen = strlen(ctx->current_dir);
    if (dlen > 0 && ctx->current_dir[dlen - 1] == '/')
        snprintf(full_path, sizeof(full_path), "%s%s", ctx->current_dir, name);
    else
        snprintf(full_path, sizeof(full_path), "%s/%s", ctx->current_dir, name);

    hex_window_open_remote_file(ctx->win, full_path);
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
}

static void remote_browse_populate(OpenRemoteCtx *ctx) {
    char label_text[4480];
    snprintf(label_text, sizeof(label_text), "%s@%s:%s",
             ctx->win->ssh_user, ctx->win->ssh_host, ctx->current_dir);
    gtk_label_set_text(ctx->path_label, label_text);

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(ctx->file_list))))
        gtk_list_box_remove(ctx->file_list, child);

    GPtrArray *av = ssh_argv_new(ctx->win->ssh_host, ctx->win->ssh_user,
                                  ctx->win->ssh_port, ctx->win->ssh_key,
                                  ctx->win->ssh_ctl_path);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("ls"));
    g_ptr_array_add(av, g_strdup("-1pA"));
    g_ptr_array_add(av, g_strdup(ctx->current_dir));

    char *stdout_buf = NULL;
    gboolean ok = ssh_spawn_sync(av, &stdout_buf, NULL);
    g_ptr_array_unref(av);

    if (strcmp(ctx->current_dir, "/") != 0) {
        GtkWidget *lbl = gtk_label_new("..");
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_top(lbl, 2);
        gtk_widget_set_margin_bottom(lbl, 2);
        gtk_list_box_append(ctx->file_list, lbl);
    }

    if (!ok || !stdout_buf) {
        GtkWidget *lbl = gtk_label_new("(failed to list directory)");
        gtk_list_box_append(ctx->file_list, lbl);
        g_free(stdout_buf);
        return;
    }

    GPtrArray *dirs = g_ptr_array_new_with_free_func(g_free);
    GPtrArray *files = g_ptr_array_new_with_free_func(g_free);

    char **lines = g_strsplit(stdout_buf, "\n", -1);
    for (int i = 0; lines[i]; i++) {
        if (lines[i][0] == '\0') continue;
        size_t len = strlen(lines[i]);
        if (lines[i][len - 1] == '/')
            g_ptr_array_add(dirs, g_strdup(lines[i]));
        else
            g_ptr_array_add(files, g_strdup(lines[i]));
    }
    g_strfreev(lines);
    g_free(stdout_buf);

    for (guint i = 0; i < dirs->len; i++) {
        const char *name = g_ptr_array_index(dirs, i);
        GtkWidget *lbl = gtk_label_new(name);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_top(lbl, 2);
        gtk_widget_set_margin_bottom(lbl, 2);
        gtk_list_box_append(ctx->file_list, lbl);
    }

    for (guint i = 0; i < files->len; i++) {
        const char *name = g_ptr_array_index(files, i);
        GtkWidget *lbl = gtk_label_new(name);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_top(lbl, 2);
        gtk_widget_set_margin_bottom(lbl, 2);
        gtk_list_box_append(ctx->file_list, lbl);
    }

    g_ptr_array_unref(dirs);
    g_ptr_array_unref(files);
}

static void on_open_remote_destroy(GtkWidget *w, gpointer data) {
    (void)w;
    g_free(data);
}

static void on_open_remote(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;

    OpenRemoteCtx *ctx = g_new0(OpenRemoteCtx, 1);
    ctx->win = win;
    g_strlcpy(ctx->current_dir, win->ssh_remote_path, sizeof(ctx->current_dir));

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Open Remote File");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 450, 500);
    gtk_window_set_titlebar(GTK_WINDOW(dialog), adw_header_bar_new());
    ctx->dialog = GTK_WINDOW(dialog);

    g_signal_connect(dialog, "destroy", G_CALLBACK(on_open_remote_destroy), ctx);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);

    ctx->path_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(ctx->path_label, 0);
    gtk_label_set_ellipsize(ctx->path_label, PANGO_ELLIPSIZE_START);
    gtk_box_append(GTK_BOX(vbox), GTK_WIDGET(ctx->path_label));

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    ctx->file_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(ctx->file_list, GTK_SELECTION_SINGLE);
    g_signal_connect(ctx->file_list, "row-activated", G_CALLBACK(on_remote_row_activated), ctx);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(ctx->file_list));
    gtk_box_append(GTK_BOX(vbox), scroll);

    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    remote_browse_populate(ctx);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_sftp_disconnect(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    HexWindow *win = data;
    hex_window_ssh_disconnect(win);
}

/* ── Register all actions ── */

void hex_actions_setup(HexWindow *win, GtkApplication *app) {
    (void)app;
    static const GActionEntry win_entries[] = {
        {"new-file",        on_new_file,        NULL, NULL, NULL, {0}},
        {"open-file",       on_open_file,       NULL, NULL, NULL, {0}},
        {"save",            on_save,            NULL, NULL, NULL, {0}},
        {"save-as",         on_save_as,         NULL, NULL, NULL, {0}},
        {"find",            on_find,            NULL, NULL, NULL, {0}},
        {"goto-offset",     on_goto_offset,     NULL, NULL, NULL, {0}},
        {"toggle-binary",   on_toggle_binary,   NULL, NULL, NULL, {0}},
        {"zoom-in",         on_zoom_in,         NULL, NULL, NULL, {0}},
        {"zoom-out",        on_zoom_out,        NULL, NULL, NULL, {0}},
        {"settings",        on_settings,        NULL, NULL, NULL, {0}},
        {"sftp-connect",    on_sftp_dialog,     NULL, NULL, NULL, {0}},
        {"sftp-disconnect", on_sftp_disconnect, NULL, NULL, NULL, {0}},
        {"open-remote",     on_open_remote,     NULL, NULL, NULL, {0}},
    };

    g_action_map_add_action_entries(G_ACTION_MAP(win->window), win_entries,
                                   G_N_ELEMENTS(win_entries), win);

    /* Keyboard shortcuts */
    const char *accels_open[]   = {"<Ctrl>o", NULL};
    const char *accels_save[]   = {"<Ctrl>s", NULL};
    const char *accels_saveas[] = {"<Ctrl><Shift>s", NULL};
    const char *accels_new[]    = {"<Ctrl>n", NULL};
    const char *accels_find[]   = {"<Ctrl>f", NULL};
    const char *accels_goto[]   = {"<Ctrl>g", NULL};
    const char *accels_zin[]    = {"<Ctrl>plus", "<Ctrl>equal", NULL};
    const char *accels_zout[]   = {"<Ctrl>minus", NULL};
    const char *accels_bin[]    = {"<Ctrl>b", NULL};
    const char *accels_quit[]   = {"<Ctrl>q", NULL};

    gtk_application_set_accels_for_action(app, "win.toggle-binary", accels_bin);
    gtk_application_set_accels_for_action(app, "win.open-file",   accels_open);
    gtk_application_set_accels_for_action(app, "win.save",        accels_save);
    gtk_application_set_accels_for_action(app, "win.save-as",     accels_saveas);
    gtk_application_set_accels_for_action(app, "win.new-file",    accels_new);
    gtk_application_set_accels_for_action(app, "win.find",        accels_find);
    gtk_application_set_accels_for_action(app, "win.goto-offset", accels_goto);
    gtk_application_set_accels_for_action(app, "win.zoom-in",     accels_zin);
    gtk_application_set_accels_for_action(app, "win.zoom-out",    accels_zout);
    gtk_application_set_accels_for_action(app, "app.quit",        accels_quit);
}
