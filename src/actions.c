#include "actions.h"
#include <adwaita.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

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
    win->data_len = 1;  /* start with 1 byte so cursor has something */
    win->original_data = g_malloc0(1);
    win->original_len = 1;
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
        win->original_data = g_memdup2(win->data, win->data_len);
        win->original_len = win->data_len;
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
            win->original_data = g_memdup2(win->data, win->data_len);
            win->original_len = win->data_len;
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

/* ── Register all actions ── */

void hex_actions_setup(HexWindow *win, GtkApplication *app) {
    (void)app;
    static const GActionEntry win_entries[] = {
        {"new-file",     on_new_file,     NULL, NULL, NULL, {0}},
        {"open-file",    on_open_file,    NULL, NULL, NULL, {0}},
        {"save",         on_save,         NULL, NULL, NULL, {0}},
        {"save-as",      on_save_as,      NULL, NULL, NULL, {0}},
        {"find",         on_find,         NULL, NULL, NULL, {0}},
        {"goto-offset",  on_goto_offset,  NULL, NULL, NULL, {0}},
        {"toggle-binary",on_toggle_binary,NULL, NULL, NULL, {0}},
        {"zoom-in",      on_zoom_in,      NULL, NULL, NULL, {0}},
        {"zoom-out",     on_zoom_out,     NULL, NULL, NULL, {0}},
        {"settings",     on_settings,     NULL, NULL, NULL, {0}},
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
