#define _GNU_SOURCE
#include <adwaita.h>
#include "window.h"
#include "actions.h"
#include "ssh.h"
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

/* ── Theme definitions (same as note-light) ── */

typedef struct {
    const char *name;
    const char *fg;
    const char *bg;
} ThemeDef;

static const ThemeDef custom_themes[] = {
    {"solarized-light", "#657b83", "#fdf6e3"},
    {"solarized-dark",  "#839496", "#002b36"},
    {"monokai",         "#f8f8f2", "#272822"},
    {"gruvbox-light",   "#3c3836", "#fbf1c7"},
    {"gruvbox-dark",    "#ebdbb2", "#282828"},
    {"nord",            "#d8dee9", "#2e3440"},
    {"dracula",         "#f8f8f2", "#282a36"},
    {"tokyo-night",     "#a9b1d6", "#1a1b26"},
    {"catppuccin-latte","#4c4f69", "#eff1f5"},
    {"catppuccin-mocha","#cdd6f4", "#1e1e2e"},
    {NULL, NULL, NULL}
};

static gboolean is_dark_theme(const char *theme) {
    return strcmp(theme, "dark") == 0 ||
           strcmp(theme, "solarized-dark") == 0 ||
           strcmp(theme, "monokai") == 0 ||
           strcmp(theme, "gruvbox-dark") == 0 ||
           strcmp(theme, "nord") == 0 ||
           strcmp(theme, "dracula") == 0 ||
           strcmp(theme, "tokyo-night") == 0 ||
           strcmp(theme, "catppuccin-mocha") == 0;
}

static void get_theme_colors(const char *theme, const char **fg, const char **bg) {
    for (int i = 0; custom_themes[i].name; i++) {
        if (strcmp(theme, custom_themes[i].name) == 0) {
            *fg = custom_themes[i].fg;
            *bg = custom_themes[i].bg;
            return;
        }
    }
    if (strcmp(theme, "system") == 0) {
        AdwStyleManager *sm = adw_style_manager_get_default();
        if (adw_style_manager_get_dark(sm)) {
            *fg = "#d4d4d4"; *bg = "#1e1e1e";
        } else {
            *fg = "#1e1e1e"; *bg = "#ffffff";
        }
    } else if (is_dark_theme(theme)) {
        *fg = "#d4d4d4"; *bg = "#1e1e1e";
    } else {
        *fg = "#1e1e1e"; *bg = "#ffffff";
    }
}

static void css_escape_font(char *out, size_t out_sz, const char *in) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j < out_sz - 1; i++) {
        char c = in[i];
        if (c == '}' || c == '{' || c == ';' || c == '"' || c == '\'' || c == '\\')
            continue;
        out[j++] = c;
    }
    out[j] = '\0';
}

static void apply_css(HexWindow *win) {
    const char *fg_str, *bg_str;
    get_theme_colors(win->settings.theme, &fg_str, &bg_str);

    char safe_font[256], safe_gui_font[256];
    css_escape_font(safe_font, sizeof(safe_font), win->settings.font);
    css_escape_font(safe_gui_font, sizeof(safe_gui_font), win->settings.gui_font);

    char css[4096];
    snprintf(css, sizeof(css),
        "window, window.background { background-color: %s; color: %s; }"
        ".hex-view { font-family: %s; font-size: %dpt; background-color: %s; color: %s; }"
        ".titlebar, headerbar {"
        "  background: %s; color: %s; box-shadow: none; }"
        "headerbar button, headerbar menubutton button,"
        "headerbar menubutton { color: %s; background: transparent; }"
        "headerbar button:hover, headerbar menubutton button:hover {"
        "  background: alpha(%s, 0.1); }"
        ".statusbar { font-size: 10pt; padding: 2px 4px;"
        "  color: alpha(%s, 0.6); background-color: %s; }"
        "popover, popover.menu {"
        "  background: transparent; box-shadow: none; border: none; }"
        "popover > contents, popover.menu > contents {"
        "  background-color: %s; color: %s;"
        "  border-radius: 12px; border: none; box-shadow: 0 2px 8px rgba(0,0,0,0.3); }"
        "popover > arrow, popover.menu > arrow { background: transparent; border: none; }"
        "popover modelbutton { color: %s; }"
        "popover modelbutton:hover { background-color: alpha(%s, 0.15); }"
        "windowcontrols button { color: %s; }"
        "label { color: %s; }"
        "entry { background-color: alpha(%s, 0.08); color: %s;"
        "  border: 1px solid alpha(%s, 0.2); }"
        "button { color: %s; }"
        "checkbutton { color: %s; }"
        "scale { color: %s; }"
        "list, listview, row { background-color: %s; color: %s; }"
        "scrolledwindow { background-color: %s; }"
        "separator { background-color: alpha(%s, 0.15); }",
        bg_str, fg_str,
        safe_font, win->settings.font_size, bg_str, fg_str,
        bg_str, fg_str,
        fg_str,
        fg_str,
        fg_str, bg_str,
        bg_str, fg_str,
        fg_str,
        fg_str,
        fg_str,
        fg_str,
        fg_str, fg_str,
        fg_str,
        fg_str,
        fg_str,
        fg_str,
        bg_str, fg_str,
        bg_str,
        fg_str);

    /* GUI font */
    char gui_css[512];
    snprintf(gui_css, sizeof(gui_css),
        "headerbar, headerbar button, headerbar label,"
        "popover, popover.menu, popover label, popover button,"
        ".statusbar, .statusbar label {"
        "  font-family: %s; font-size: %dpt; }",
        safe_gui_font, win->settings.gui_font_size);
    strncat(css, gui_css, sizeof(css) - strlen(css) - 1);

    gtk_css_provider_load_from_string(win->css_provider, css);
}

static void apply_theme(HexWindow *win) {
    AdwStyleManager *sm = adw_style_manager_get_default();
    if (strcmp(win->settings.theme, "system") == 0)
        adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_DEFAULT);
    else if (is_dark_theme(win->settings.theme))
        adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_FORCE_DARK);
    else
        adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_FORCE_LIGHT);
}

/* ── Hex rendering ── */

static void measure_font(HexWindow *win, cairo_t *cr) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_new();
    pango_font_description_set_family(fd, win->settings.font);
    pango_font_description_set_size(fd, win->settings.font_size * PANGO_SCALE);
    pango_layout_set_font_description(layout, fd);
    pango_layout_set_text(layout, "0", 1);
    int pw, ph;
    pango_layout_get_pixel_size(layout, &pw, &ph);
    win->char_width = pw;
    win->row_height = ph + 2;
    pango_font_description_free(fd);
    g_object_unref(layout);
}

/* Column layout:
 * [offset_col] [gap] [hex bytes with spaces] [gap] [ascii]
 *  8 chars       2    bpr*3                    2     bpr
 */
static int offset_col_width(HexWindow *win) {
    return 9 * win->char_width;  /* "00000000 " */
}

static int hex_col_start(HexWindow *win) {
    return offset_col_width(win);
}

static int byte_display_chars(HexWindow *win) {
    /* chars per byte: hex=2, binary=8 */
    return win->settings.display_mode == 1 ? 8 : 2;
}

static int hex_col_width(HexWindow *win) {
    int bpr = win->settings.bytes_per_row;
    int bdc = byte_display_chars(win);
    /* Each byte = bdc chars + 1 space, plus extra space at half */
    return (bpr * (bdc + 1) + 1) * win->char_width;
}

static int ascii_col_start(HexWindow *win) {
    return hex_col_start(win) + hex_col_width(win) + win->char_width;
}

static gsize total_rows(HexWindow *win) {
    if (win->data_len == 0) return 1;
    return (win->data_len + win->settings.bytes_per_row - 1) / win->settings.bytes_per_row;
}

static void update_status(HexWindow *win) {
    char buf[128];
    snprintf(buf, sizeof(buf), "Offset: 0x%08lX (%lu)",
             (unsigned long)win->cursor_pos, (unsigned long)win->cursor_pos);
    gtk_label_set_text(win->status_offset, buf);

    if (win->cursor_pos < win->data_len) {
        unsigned char b = win->data[win->cursor_pos];
        char bin[9];
        for (int bit = 7; bit >= 0; bit--)
            bin[7 - bit] = (b & (1 << bit)) ? '1' : '0';
        bin[8] = '\0';
        snprintf(buf, sizeof(buf), "Value: 0x%02X %s (%u) '%c'",
                 b, bin, b, (b >= 32 && b < 127) ? (char)b : '.');
    } else {
        snprintf(buf, sizeof(buf), "Value: --");
    }
    gtk_label_set_text(win->status_value, buf);

    if (win->data_len > 0) {
        if (win->data_len >= 1024 * 1024)
            snprintf(buf, sizeof(buf), "Size: %.1f MB (%lu bytes)",
                     (double)win->data_len / (1024.0 * 1024.0), (unsigned long)win->data_len);
        else if (win->data_len >= 1024)
            snprintf(buf, sizeof(buf), "Size: %.1f KB (%lu bytes)",
                     (double)win->data_len / 1024.0, (unsigned long)win->data_len);
        else
            snprintf(buf, sizeof(buf), "Size: %lu bytes", (unsigned long)win->data_len);
    } else {
        snprintf(buf, sizeof(buf), "Size: 0 bytes");
    }
    gtk_label_set_text(win->status_size, buf);
}

static void ensure_cursor_visible(HexWindow *win) {
    int bpr = win->settings.bytes_per_row;
    gsize cursor_row = win->cursor_pos / (gsize)bpr;
    if (cursor_row < win->scroll_offset)
        win->scroll_offset = cursor_row;
    else if (cursor_row >= win->scroll_offset + (gsize)win->visible_rows)
        win->scroll_offset = cursor_row - (gsize)win->visible_rows + 1;

    gtk_adjustment_set_value(win->adj, (double)win->scroll_offset);
}

static void update_title(HexWindow *win) {
    if (win->current_file[0]) {
        char *base = g_path_get_basename(win->current_file);
        if (win->dirty) {
            char title[256];
            snprintf(title, sizeof(title), "%s *", base);
            gtk_window_set_title(GTK_WINDOW(win->window), title);
        } else {
            gtk_window_set_title(GTK_WINDOW(win->window), base);
        }
        g_free(base);
    } else {
        gtk_window_set_title(GTK_WINDOW(win->window),
            win->dirty ? "Hex Editor *" : "Hex Editor");
    }
}

void hex_window_queue_redraw(HexWindow *win) {
    gtk_widget_queue_draw(GTK_WIDGET(win->hex_view));
    update_status(win);
}

/* ── Drawing ── */

static void draw_hex_view(GtkDrawingArea *area, cairo_t *cr,
                          int width, int height, gpointer data) {
    (void)area;
    HexWindow *win = data;

    if (win->char_width == 0)
        measure_font(win, cr);

    win->visible_rows = height / win->row_height;
    if (win->visible_rows < 1) win->visible_rows = 1;

    /* Update scrollbar */
    gsize total = total_rows(win);
    gtk_adjustment_configure(win->adj,
        (double)win->scroll_offset,
        0, (double)total,
        1, (double)win->visible_rows,
        (double)win->visible_rows);

    /* Get colors */
    const char *fg_str, *bg_str;
    get_theme_colors(win->settings.theme, &fg_str, &bg_str);

    GdkRGBA fg_color, bg_color;
    gdk_rgba_parse(&fg_color, fg_str);
    gdk_rgba_parse(&bg_color, bg_str);

    /* Background */
    cairo_set_source_rgba(cr, bg_color.red, bg_color.green, bg_color.blue, 1.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_new();
    pango_font_description_set_family(fd, win->settings.font);
    pango_font_description_set_size(fd, win->settings.font_size * PANGO_SCALE);
    pango_layout_set_font_description(layout, fd);

    int bpr = win->settings.bytes_per_row;
    const char *hex_fmt = win->settings.uppercase_hex ? "%02X" : "%02x";
    const char *off_fmt = win->settings.uppercase_hex ? "%08lX" : "%08lx";

    /* Highlight colors */
    GdkRGBA cursor_hex_bg   = {0.2, 0.5, 1.0, 0.35};  /* blue — hex/bin active */
    GdkRGBA cursor_ascii_bg = {0.2, 0.8, 0.4, 0.35};  /* green — ASCII active */
    GdkRGBA cross_bg        = {0.5, 0.5, 0.5, 0.15};  /* grey — mirror */
    GdkRGBA modified_fg = {1.0, 0.4, 0.3, 1.0};
    GdkRGBA offset_fg = {fg_color.red, fg_color.green, fg_color.blue, 0.4};
    GdkRGBA sel_bg = {0.2, 0.5, 1.0, 0.18};

    for (int row = 0; row < win->visible_rows + 1; row++) {
        gsize file_row = win->scroll_offset + (gsize)row;
        gsize row_offset = file_row * (gsize)bpr;
        if (row_offset >= win->data_len && win->data_len > 0) break;
        if (win->data_len == 0 && row > 0) break;

        int y = row * win->row_height;

        /* Offset column */
        char buf[32];
        snprintf(buf, sizeof(buf), off_fmt, (unsigned long)row_offset);
        cairo_set_source_rgba(cr, offset_fg.red, offset_fg.green, offset_fg.blue, offset_fg.alpha);
        pango_layout_set_text(layout, buf, -1);
        cairo_move_to(cr, 0, y);
        pango_cairo_show_layout(cr, layout);

        /* Data bytes (hex or binary) */
        int bdc = byte_display_chars(win);
        for (int col = 0; col < bpr; col++) {
            gsize pos = row_offset + (gsize)col;
            if (pos >= win->data_len) break;

            int x = hex_col_start(win) + col * (bdc + 1) * win->char_width;
            /* Extra gap at midpoint */
            if (col >= bpr / 2) x += win->char_width;

            /* Selection highlight */
            gsize sel_lo = win->selection_start < win->selection_end ? win->selection_start : win->selection_end;
            gsize sel_hi = win->selection_start < win->selection_end ? win->selection_end : win->selection_start;
            if (pos >= sel_lo && pos <= sel_hi && sel_lo != sel_hi) {
                cairo_set_source_rgba(cr, sel_bg.red, sel_bg.green, sel_bg.blue, sel_bg.alpha);
                cairo_rectangle(cr, x, y, bdc * win->char_width, win->row_height);
                cairo_fill(cr);
            }

            /* Cursor highlight */
            if (pos == win->cursor_pos) {
                GdkRGBA *bg = win->editing_ascii ? &cross_bg : &cursor_hex_bg;
                cairo_set_source_rgba(cr, bg->red, bg->green, bg->blue, bg->alpha);
                cairo_rectangle(cr, x, y, bdc * win->char_width, win->row_height);
                cairo_fill(cr);
            }

            /* Byte value */
            gboolean modified = win->original_data && pos < win->original_len &&
                                win->data[pos] != win->original_data[pos];
            if (modified)
                cairo_set_source_rgba(cr, modified_fg.red, modified_fg.green, modified_fg.blue, modified_fg.alpha);
            else
                cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);

            if (win->settings.display_mode == 1) {
                /* Binary */
                char bin[9];
                unsigned char b = win->data[pos];
                for (int bit = 7; bit >= 0; bit--)
                    bin[7 - bit] = (b & (1 << bit)) ? '1' : '0';
                bin[8] = '\0';
                pango_layout_set_text(layout, bin, -1);
            } else {
                /* Hex */
                snprintf(buf, sizeof(buf), hex_fmt, win->data[pos]);
                pango_layout_set_text(layout, buf, -1);
            }
            cairo_move_to(cr, x, y);
            pango_cairo_show_layout(cr, layout);
        }

        /* ASCII column */
        if (win->settings.show_ascii) {
            int ax = ascii_col_start(win);

            /* Separator line */
            cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.1);
            cairo_move_to(cr, ax - win->char_width / 2, y);
            cairo_line_to(cr, ax - win->char_width / 2, y + win->row_height);
            cairo_stroke(cr);

            for (int col = 0; col < bpr; col++) {
                gsize pos = row_offset + (gsize)col;
                if (pos >= win->data_len) break;

                /* Selection highlight */
                gsize sel_lo = win->selection_start < win->selection_end ? win->selection_start : win->selection_end;
                gsize sel_hi = win->selection_start < win->selection_end ? win->selection_end : win->selection_start;
                if (pos >= sel_lo && pos <= sel_hi && sel_lo != sel_hi) {
                    cairo_set_source_rgba(cr, sel_bg.red, sel_bg.green, sel_bg.blue, sel_bg.alpha);
                    cairo_rectangle(cr, ax + col * win->char_width, y, win->char_width, win->row_height);
                    cairo_fill(cr);
                }

                /* Cursor highlight in ASCII */
                if (pos == win->cursor_pos) {
                    GdkRGBA *bg = win->editing_ascii ? &cursor_ascii_bg : &cross_bg;
                    cairo_set_source_rgba(cr, bg->red, bg->green, bg->blue, bg->alpha);
                    cairo_rectangle(cr, ax + col * win->char_width, y, win->char_width, win->row_height);
                    cairo_fill(cr);
                }

                unsigned char b = win->data[pos];
                gboolean modified = win->original_data && pos < win->original_len &&
                                    b != win->original_data[pos];
                if (modified)
                    cairo_set_source_rgba(cr, modified_fg.red, modified_fg.green, modified_fg.blue, modified_fg.alpha);
                else
                    cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.7);

                char ch = (b >= 32 && b < 127) ? (char)b : '.';
                pango_layout_set_text(layout, &ch, 1);
                cairo_move_to(cr, ax + col * win->char_width, y);
                pango_cairo_show_layout(cr, layout);
            }
        }
    }

    pango_font_description_free(fd);
    g_object_unref(layout);
}

/* ── Input handling ── */

static int hex_char_value(guint keyval) {
    if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9) return (int)(keyval - GDK_KEY_0);
    if (keyval >= GDK_KEY_a && keyval <= GDK_KEY_f) return 10 + (int)(keyval - GDK_KEY_a);
    if (keyval >= GDK_KEY_A && keyval <= GDK_KEY_F) return 10 + (int)(keyval - GDK_KEY_A);
    return -1;
}

static void set_byte_dirty(HexWindow *win) {
    if (!win->dirty) {
        win->dirty = TRUE;
        update_title(win);
    }
    /* Check if we've restored to original */
    if (win->original_data && win->data_len == win->original_len) {
        if (memcmp(win->data, win->original_data, win->data_len) == 0) {
            win->dirty = FALSE;
            update_title(win);
        }
    }
}

static gboolean on_key_pressed(GtkEventControllerKey *ctrl, guint keyval,
                                guint keycode, GdkModifierType state, gpointer data) {
    (void)ctrl; (void)keycode;
    HexWindow *win = data;
    int bpr = win->settings.bytes_per_row;

    /* Let Ctrl/Alt shortcuts pass through to actions */
    if (state & (GDK_CONTROL_MASK | GDK_ALT_MASK))
        return FALSE;

    /* Tab: switch between hex and ASCII editing */
    if (keyval == GDK_KEY_Tab && win->settings.show_ascii) {
        win->editing_ascii = !win->editing_ascii;
        win->cursor_nibble = 0;
        hex_window_queue_redraw(win);
        return TRUE;
    }

    /* Navigation */
    gboolean shift = (state & GDK_SHIFT_MASK) != 0;
    gsize old_pos = win->cursor_pos;

    if (win->data_len == 0) return FALSE;  /* no navigation on empty data */

    switch (keyval) {
    case GDK_KEY_Left:
        if (win->cursor_pos > 0) win->cursor_pos--;
        win->cursor_nibble = 0;
        break;
    case GDK_KEY_Right:
        if (win->data_len > 0 && win->cursor_pos < win->data_len - 1)
            win->cursor_pos++;
        win->cursor_nibble = 0;
        break;
    case GDK_KEY_Up:
        if (win->cursor_pos >= (gsize)bpr) win->cursor_pos -= (gsize)bpr;
        win->cursor_nibble = 0;
        break;
    case GDK_KEY_Down:
        if (win->cursor_pos + (gsize)bpr < win->data_len) win->cursor_pos += (gsize)bpr;
        win->cursor_nibble = 0;
        break;
    case GDK_KEY_Page_Up:
        if (win->cursor_pos >= (gsize)(bpr * win->visible_rows))
            win->cursor_pos -= (gsize)(bpr * win->visible_rows);
        else
            win->cursor_pos = win->cursor_pos % (gsize)bpr;
        win->cursor_nibble = 0;
        break;
    case GDK_KEY_Page_Down: {
        gsize page = (gsize)(bpr * win->visible_rows);
        if (win->cursor_pos + page < win->data_len)
            win->cursor_pos += page;
        else if (win->data_len > 0)
            win->cursor_pos = win->data_len - 1;
        win->cursor_nibble = 0;
        break;
    }
    case GDK_KEY_Home:
        if (state & GDK_CONTROL_MASK)
            win->cursor_pos = 0;
        else
            win->cursor_pos -= win->cursor_pos % (gsize)bpr;
        win->cursor_nibble = 0;
        break;
    case GDK_KEY_End:
        if (state & GDK_CONTROL_MASK)
            win->cursor_pos = win->data_len > 0 ? win->data_len - 1 : 0;
        else {
            gsize row_start = win->cursor_pos - (win->cursor_pos % (gsize)bpr);
            gsize row_end = row_start + (gsize)bpr - 1;
            if (row_end >= win->data_len && win->data_len > 0) row_end = win->data_len - 1;
            win->cursor_pos = row_end;
        }
        win->cursor_nibble = 0;
        break;
    default:
        /* Editing in data pane */
        if (!win->editing_ascii && win->cursor_pos <= win->data_len) {
            /* Auto-grow buffer if cursor is at end */
            if (win->cursor_pos == win->data_len) {
                if (win->data_len >= win->data_alloc) {
                    win->data_alloc = win->data_alloc ? win->data_alloc * 2 : 4096;
                    win->data = g_realloc(win->data, win->data_alloc);
                }
                win->data[win->data_len] = 0;
                win->data_len++;
            }
            if (win->settings.display_mode == 1) {
                /* Binary mode: accept 0 and 1 */
                int bv = -1;
                if (keyval == GDK_KEY_0) bv = 0;
                else if (keyval == GDK_KEY_1) bv = 1;
                if (bv >= 0) {
                    unsigned char b = win->data[win->cursor_pos];
                    int bit = 7 - win->cursor_nibble;
                    if (bv)
                        b |= (unsigned char)(1 << bit);
                    else
                        b &= (unsigned char)~(1 << bit);
                    win->data[win->cursor_pos] = b;
                    win->cursor_nibble++;
                    if (win->cursor_nibble >= 8) {
                        win->cursor_nibble = 0;
                        win->cursor_pos++;
                    }
                    set_byte_dirty(win);
                    ensure_cursor_visible(win);
                    hex_window_queue_redraw(win);
                    return TRUE;
                }
            } else {
                /* Hex mode */
                int hv = hex_char_value(keyval);
                if (hv >= 0) {
                    unsigned char b = win->data[win->cursor_pos];
                    if (win->cursor_nibble == 0) {
                        b = (unsigned char)((hv << 4) | (b & 0x0F));
                        win->data[win->cursor_pos] = b;
                        win->cursor_nibble = 1;
                    } else {
                        b = (unsigned char)((b & 0xF0) | hv);
                        win->data[win->cursor_pos] = b;
                        win->cursor_nibble = 0;
                        win->cursor_pos++;
                    }
                    set_byte_dirty(win);
                    ensure_cursor_visible(win);
                    hex_window_queue_redraw(win);
                    return TRUE;
                }
            }
        }

        /* ASCII editing */
        if (win->editing_ascii &&
            keyval >= 0x20 && keyval <= 0x7E && win->cursor_pos <= win->data_len) {
            if (win->cursor_pos == win->data_len) {
                if (win->data_len >= win->data_alloc) {
                    win->data_alloc = win->data_alloc ? win->data_alloc * 2 : 4096;
                    win->data = g_realloc(win->data, win->data_alloc);
                }
                win->data[win->data_len] = 0;
                win->data_len++;
            }
            win->data[win->cursor_pos] = (unsigned char)keyval;
            set_byte_dirty(win);
            if (win->cursor_pos < win->data_len)
                win->cursor_pos++;
            ensure_cursor_visible(win);
            hex_window_queue_redraw(win);
            return TRUE;
        }

        return FALSE;
    }

    /* Selection with shift */
    if (shift) {
        if (old_pos == win->selection_start && old_pos == win->selection_end)
            win->selection_start = old_pos;
        win->selection_end = win->cursor_pos;
    } else {
        win->selection_start = win->cursor_pos;
        win->selection_end = win->cursor_pos;
    }

    ensure_cursor_visible(win);
    hex_window_queue_redraw(win);
    return TRUE;
}

static void on_scroll(GtkEventControllerScroll *ctrl, double dx, double dy, gpointer data) {
    (void)ctrl; (void)dx;
    HexWindow *win = data;
    gsize total = total_rows(win);
    if (dy < 0 && win->scroll_offset >= 3)
        win->scroll_offset -= 3;
    else if (dy < 0)
        win->scroll_offset = 0;
    else if (dy > 0)
        win->scroll_offset += 3;

    if (win->scroll_offset + (gsize)win->visible_rows > total && total > (gsize)win->visible_rows)
        win->scroll_offset = total - (gsize)win->visible_rows;

    gtk_adjustment_set_value(win->adj, (double)win->scroll_offset);
    hex_window_queue_redraw(win);
}

static void on_scrollbar_changed(GtkAdjustment *adj, gpointer data) {
    HexWindow *win = data;
    win->scroll_offset = (gsize)gtk_adjustment_get_value(adj);
    gtk_widget_queue_draw(GTK_WIDGET(win->hex_view));
}

/* Click to position cursor */
static void on_click_pressed(GtkGestureClick *gesture, int n_press,
                              double x, double y, gpointer data) {
    (void)gesture; (void)n_press;
    HexWindow *win = data;
    if (win->data_len == 0) return;

    int bpr = win->settings.bytes_per_row;
    int row = (int)y / win->row_height;
    gsize file_row = win->scroll_offset + (gsize)row;
    gsize row_offset = file_row * (gsize)bpr;

    /* Determine if click is in hex or ascii area */
    int hx = hex_col_start(win);
    int ax = ascii_col_start(win);

    gsize pos = row_offset;
    if (x >= ax && win->settings.show_ascii) {
        /* ASCII area */
        int col = (int)(x - ax) / win->char_width;
        if (col < 0) col = 0;
        if (col >= bpr) col = bpr - 1;
        pos = row_offset + (gsize)col;
        win->editing_ascii = TRUE;
    } else if (x >= hx) {
        /* Data area (hex or binary) */
        int bdc = byte_display_chars(win);
        int cell_w = (bdc + 1) * win->char_width;
        int rel_x = (int)(x - hx);
        int col = rel_x / cell_w;
        /* Account for midpoint gap */
        if (col >= bpr / 2) {
            rel_x -= win->char_width;
            col = rel_x / cell_w;
        }
        if (col < 0) col = 0;
        if (col >= bpr) col = bpr - 1;
        pos = row_offset + (gsize)col;
        win->editing_ascii = FALSE;
    }

    if (pos >= win->data_len) pos = win->data_len > 0 ? win->data_len - 1 : 0;
    win->cursor_pos = pos;
    win->cursor_nibble = 0;
    win->selection_start = pos;
    win->selection_end = pos;

    gtk_widget_grab_focus(GTK_WIDGET(win->hex_view));
    hex_window_queue_redraw(win);
}

/* ── File operations ── */

void hex_window_load_file(HexWindow *win, const char *path) {
    if (!path || path[0] == '\0') return;

    struct stat st;
    if (g_stat(path, &st) != 0) return;
    gsize file_size = (gsize)st.st_size;

    /* Limit to 64 MB for responsiveness */
    if (file_size > 64 * 1024 * 1024) file_size = 64 * 1024 * 1024;

    FILE *fp = fopen(path, "rb");
    if (!fp) return;

    unsigned char *buf = g_malloc(file_size);
    gsize len = fread(buf, 1, file_size, fp);
    gboolean read_err = ferror(fp);
    fclose(fp);

    if (read_err || len == 0) {
        g_free(buf);
        return;
    }

    g_free(win->data);
    g_free(win->original_data);

    win->data = buf;
    win->data_len = len;
    win->data_alloc = len;
    win->original_data = g_memdup2(buf, len);
    win->original_len = len;
    win->dirty = FALSE;

    win->cursor_pos = 0;
    win->cursor_nibble = 0;
    win->scroll_offset = 0;
    win->selection_start = 0;
    win->selection_end = 0;
    win->editing_ascii = FALSE;

    snprintf(win->current_file, sizeof(win->current_file), "%s", path);
    snprintf(win->settings.last_file, sizeof(win->settings.last_file), "%s", path);
    hex_settings_save(&win->settings);

    update_title(win);
    hex_window_queue_redraw(win);
}

/* ── Search ── */

void hex_window_show_search(HexWindow *win) {
    gboolean visible = gtk_widget_get_visible(win->search_bar);
    gtk_widget_set_visible(win->search_bar, !visible);
    if (!visible)
        gtk_widget_grab_focus(win->search_entry);
}

static void parse_hex_string(const char *str, unsigned char **out, int *out_len) {
    *out = NULL;
    *out_len = 0;
    int slen = (int)strlen(str);
    if (slen <= 0) return;
    /* Max output is slen/2 + 1 hex bytes; allocate conservatively */
    int alloc = slen / 2 + 1;
    unsigned char *buf = g_malloc(alloc);
    int n = 0;

    for (int i = 0; i < slen; ) {
        while (i < slen && (str[i] == ' ' || str[i] == ',')) i++;
        if (i >= slen) break;
        if (i + 1 < slen && isxdigit((unsigned char)str[i]) && isxdigit((unsigned char)str[i+1])) {
            unsigned int val = 0;
            if (sscanf(str + i, "%2x", &val) == 1 && n < alloc)
                buf[n++] = (unsigned char)(val & 0xFF);
            i += 2;
        } else if (isxdigit((unsigned char)str[i])) {
            unsigned int val = 0;
            if (sscanf(str + i, "%1x", &val) == 1 && n < alloc)
                buf[n++] = (unsigned char)(val & 0xFF);
            i++;
        } else {
            /* Treat as ASCII text search */
            g_free(buf);
            *out = (unsigned char *)g_strdup(str);
            *out_len = slen;
            return;
        }
    }
    *out = buf;
    *out_len = n;
}

static void do_search(HexWindow *win) {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(win->search_entry));
    if (!text || !text[0]) return;

    g_free(win->match_offsets);
    win->match_offsets = NULL;
    win->match_count = 0;
    win->match_current = -1;

    unsigned char *pattern;
    int plen;
    parse_hex_string(text, &pattern, &plen);
    if (plen <= 0 || plen > 4096 || !pattern) { g_free(pattern); return; }

    /* Find all matches */
    gsize pat_len = (gsize)plen;
    GPtrArray *matches = g_ptr_array_new();
    for (gsize i = 0; i + pat_len <= win->data_len; i++) {
        if (memcmp(win->data + i, pattern, pat_len) == 0)
            g_ptr_array_add(matches, GSIZE_TO_POINTER(i));
    }
    g_free(pattern);

    win->match_count = (int)matches->len;
    if (win->match_count > 0) {
        win->match_offsets = g_malloc(sizeof(gsize) * (size_t)win->match_count);
        for (int i = 0; i < win->match_count; i++)
            win->match_offsets[i] = GPOINTER_TO_SIZE(matches->pdata[i]);

        /* Jump to first match at or after cursor */
        win->match_current = 0;
        for (int i = 0; i < win->match_count; i++) {
            if (win->match_offsets[i] >= win->cursor_pos) {
                win->match_current = i;
                break;
            }
        }
        win->cursor_pos = win->match_offsets[win->match_current];
        ensure_cursor_visible(win);
    }
    g_ptr_array_unref(matches);

    char buf[64];
    if (win->match_count > 0)
        snprintf(buf, sizeof(buf), "%d/%d", win->match_current + 1, win->match_count);
    else
        snprintf(buf, sizeof(buf), "No matches");
    gtk_label_set_text(win->match_label, buf);

    hex_window_queue_redraw(win);
}

static void on_search_activate(GtkEntry *entry, gpointer data) {
    (void)entry;
    HexWindow *win = data;

    if (win->match_count > 0) {
        win->match_current = (win->match_current + 1) % win->match_count;
        win->cursor_pos = win->match_offsets[win->match_current];
        ensure_cursor_visible(win);

        char buf[64];
        snprintf(buf, sizeof(buf), "%d/%d", win->match_current + 1, win->match_count);
        gtk_label_set_text(win->match_label, buf);
        hex_window_queue_redraw(win);
    } else {
        do_search(win);
    }
}

static void on_search_changed(GtkEditable *editable, gpointer data) {
    (void)editable;
    do_search(data);
}

/* ── Goto offset dialog ── */

static void on_close_search(HexWindow *win) {
    gtk_widget_set_visible(win->search_bar, FALSE);
    gtk_widget_grab_focus(GTK_WIDGET(win->hex_view));
}

static void on_goto_entry_activate(GtkEntry *e, gpointer d) {
    (void)d;
    HexWindow *w = g_object_get_data(G_OBJECT(e), "hex-win");
    GtkWidget *dlg = g_object_get_data(G_OBJECT(e), "hex-dialog");
    const char *text = gtk_editable_get_text(GTK_EDITABLE(e));
    if (!text[0]) return;

    char *endptr = NULL;
    errno = 0;
    gsize offset;
    if (text[0] == '0' && text[1] && (text[1] == 'x' || text[1] == 'X'))
        offset = (gsize)strtoull(text + 2, &endptr, 16);
    else if (strpbrk(text, "ABCDEFabcdef"))
        offset = (gsize)strtoull(text, &endptr, 16);
    else
        offset = (gsize)strtoull(text, &endptr, 0);

    if (errno == ERANGE || (endptr && *endptr != '\0' && endptr == text))
        offset = 0;  /* invalid input, go to start */

    if (offset >= w->data_len && w->data_len > 0) offset = w->data_len - 1;
    w->cursor_pos = offset;
    w->cursor_nibble = 0;
    w->selection_start = offset;
    w->selection_end = offset;
    ensure_cursor_visible(w);
    hex_window_queue_redraw(w);
    gtk_window_destroy(GTK_WINDOW(dlg));
}

void hex_window_goto_offset(HexWindow *win) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Go to Offset");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, -1);
    gtk_window_set_titlebar(GTK_WINDOW(dialog), adw_header_bar_new());

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    GtkWidget *label = gtk_label_new("Offset (hex or decimal):");
    gtk_box_append(GTK_BOX(vbox), label);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "e.g. 0x1A0 or 416");
    gtk_box_append(GTK_BOX(vbox), entry);

    /* Store win pointer in entry for callback */
    g_object_set_data(G_OBJECT(entry), "hex-win", win);
    g_object_set_data(G_OBJECT(entry), "hex-dialog", dialog);

    g_signal_connect(entry, "activate", G_CALLBACK(on_goto_entry_activate), NULL);

    gtk_window_present(GTK_WINDOW(dialog));
    gtk_widget_grab_focus(entry);
}

/* ── Window construction ── */

/* ── SSH/SFTP ── */

gboolean hex_window_is_remote(HexWindow *win) {
    return win->ssh_host[0] != '\0';
}

static void update_ssh_status(HexWindow *win) {
    gboolean connected = hex_window_is_remote(win);
    if (connected) {
        char label[512];
        snprintf(label, sizeof(label), "SSH: %s@%s", win->ssh_user, win->ssh_host);
        gtk_button_set_label(GTK_BUTTON(win->ssh_status_btn), label);
    } else {
        gtk_button_set_label(GTK_BUTTON(win->ssh_status_btn), "SSH: Off");
    }

    GAction *a;
    a = g_action_map_lookup_action(G_ACTION_MAP(win->window), "open-remote");
    if (a) g_simple_action_set_enabled(G_SIMPLE_ACTION(a), connected);
    a = g_action_map_lookup_action(G_ACTION_MAP(win->window), "sftp-disconnect");
    if (a) g_simple_action_set_enabled(G_SIMPLE_ACTION(a), connected);
}

void hex_window_ssh_connect(HexWindow *win, const char *host, const char *user,
                             int port, const char *key, const char *remote_path) {
    if (hex_window_is_remote(win))
        hex_window_ssh_disconnect(win);

    g_strlcpy(win->ssh_host, host, sizeof(win->ssh_host));
    g_strlcpy(win->ssh_user, user, sizeof(win->ssh_user));
    win->ssh_port = port;
    g_strlcpy(win->ssh_key, key, sizeof(win->ssh_key));
    g_strlcpy(win->ssh_remote_path, remote_path, sizeof(win->ssh_remote_path));

    snprintf(win->ssh_mount, sizeof(win->ssh_mount),
             "/tmp/hex-editor-sftp-%d-%s@%s", (int)getpid(), user, host);

    ssh_ctl_start(win->ssh_ctl_dir, sizeof(win->ssh_ctl_dir),
                  win->ssh_ctl_path, sizeof(win->ssh_ctl_path),
                  host, user, port, key);

    update_ssh_status(win);
}

void hex_window_ssh_disconnect(HexWindow *win) {
    if (!hex_window_is_remote(win)) return;

    ssh_ctl_stop(win->ssh_ctl_path, win->ssh_ctl_dir,
                 win->ssh_host, win->ssh_user);

    win->ssh_host[0] = '\0';
    win->ssh_user[0] = '\0';
    win->ssh_port = 0;
    win->ssh_key[0] = '\0';
    win->ssh_remote_path[0] = '\0';
    win->ssh_mount[0] = '\0';

    update_ssh_status(win);
}

void hex_window_open_remote_file(HexWindow *win, const char *remote_path) {
    char *contents = NULL;
    gsize len = 0;

    if (!ssh_cat_file(win->ssh_host, win->ssh_user, win->ssh_port,
                      win->ssh_key, win->ssh_ctl_path,
                      remote_path, &contents, &len, 64 * 1024 * 1024)) {
        return;
    }

    g_free(win->data);
    g_free(win->original_data);

    win->data = (unsigned char *)contents;
    win->data_len = len;
    win->data_alloc = len;
    win->original_data = g_memdup2(contents, len);
    win->original_len = len;
    win->dirty = FALSE;

    win->cursor_pos = 0;
    win->cursor_nibble = 0;
    win->scroll_offset = 0;
    win->selection_start = 0;
    win->selection_end = 0;
    win->editing_ascii = FALSE;

    /* Store virtual path for save */
    snprintf(win->current_file, sizeof(win->current_file), "%s%s",
             win->ssh_mount, remote_path);

    char *base = g_path_get_basename(remote_path);
    char title[512];
    snprintf(title, sizeof(title), "%s [%s@%s]", base, win->ssh_user, win->ssh_host);
    gtk_window_set_title(GTK_WINDOW(win->window), title);
    g_free(base);

    hex_window_queue_redraw(win);
}

gboolean hex_window_save_remote(HexWindow *win) {
    if (!hex_window_is_remote(win)) return FALSE;
    if (!ssh_path_is_remote(win->current_file)) return FALSE;

    char remote[4096];
    ssh_to_remote_path(win->ssh_mount, win->ssh_remote_path,
                       win->current_file, remote, sizeof(remote));

    gboolean ok = ssh_write_file(win->ssh_host, win->ssh_user, win->ssh_port,
                                  win->ssh_key, win->ssh_ctl_path,
                                  remote, (const char *)win->data, win->data_len);

    if (ok) {
        g_free(win->original_data);
        win->original_data = g_memdup2(win->data, win->data_len);
        win->original_len = win->data_len;
        win->dirty = FALSE;

        char *base = g_path_get_basename(remote);
        char title[512];
        snprintf(title, sizeof(title), "%s [%s@%s]", base, win->ssh_user, win->ssh_host);
        gtk_window_set_title(GTK_WINDOW(win->window), title);
        g_free(base);
    }

    return ok;
}

/* ── Settings apply ── */

void hex_window_apply_settings(HexWindow *win) {
    apply_theme(win);
    apply_css(win);
    win->char_width = 0; /* force re-measure */
    hex_window_queue_redraw(win);
}

static void save_window_state(HexWindow *win) {
    int w = gtk_widget_get_width(GTK_WIDGET(win->window));
    int h = gtk_widget_get_height(GTK_WIDGET(win->window));
    if (w > 100 && h > 100) {
        win->settings.window_width = w;
        win->settings.window_height = h;
    }
    hex_settings_save(&win->settings);
}

static void on_window_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    HexWindow *win = data;
    if (hex_window_is_remote(win))
        hex_window_ssh_disconnect(win);
    if (win->css_provider) {
        gtk_style_context_remove_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(win->css_provider));
        g_object_unref(win->css_provider);
    }
    g_free(win->data);
    g_free(win->original_data);
    g_free(win->match_offsets);
    g_free(win);
}

static void on_save_dialog_response(GObject *source, GAsyncResult *result, gpointer data) {
    HexWindow *win = data;
    GtkAlertDialog *dlg = GTK_ALERT_DIALOG(source);
    GError *err = NULL;
    int button = gtk_alert_dialog_choose_finish(dlg, result, &err);
    if (err) {
        g_error_free(err);
        return;
    }

    if (button == 0) {
        /* Save */
        g_action_group_activate_action(G_ACTION_GROUP(win->window), "save", NULL);
        save_window_state(win);
        gtk_window_destroy(GTK_WINDOW(win->window));
    } else if (button == 1) {
        /* Discard */
        win->dirty = FALSE;
        save_window_state(win);
        gtk_window_destroy(GTK_WINDOW(win->window));
    }
    /* button == 2: Cancel — do nothing */
}

static gboolean on_close_request(GtkWindow *window, gpointer data) {
    (void)window;
    HexWindow *win = data;

    if (win->dirty) {
        GtkAlertDialog *dlg = gtk_alert_dialog_new("Save changes before closing?");
        gtk_alert_dialog_set_detail(dlg, "Your changes will be lost if you don't save them.");
        const char *buttons[] = {"Save", "Discard", "Cancel", NULL};
        gtk_alert_dialog_set_buttons(dlg, buttons);
        gtk_alert_dialog_set_cancel_button(dlg, 2);
        gtk_alert_dialog_set_default_button(dlg, 0);
        gtk_alert_dialog_choose(dlg, GTK_WINDOW(win->window), NULL,
                                on_save_dialog_response, win);
        g_object_unref(dlg);
        return TRUE;  /* prevent close */
    }

    save_window_state(win);
    return FALSE;  /* allow close */
}

HexWindow *hex_window_new(GtkApplication *app) {
    HexWindow *win = g_new0(HexWindow, 1);
    hex_settings_load(&win->settings);

    /* Window */
    win->window = GTK_APPLICATION_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(GTK_WINDOW(win->window), "Hex Editor");
    gtk_window_set_default_size(GTK_WINDOW(win->window),
                                win->settings.window_width, win->settings.window_height);

    /* CSS */
    win->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(win->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    /* Header bar */
    GtkWidget *header = adw_header_bar_new();

    /* Menu button */
    GtkWidget *menu_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_btn), "open-menu-symbolic");

    GMenu *menu = g_menu_new();
    g_menu_append(menu, "New", "win.new-file");
    g_menu_append(menu, "Open...", "win.open-file");
    g_menu_append(menu, "Save", "win.save");
    g_menu_append(menu, "Save As...", "win.save-as");

    GMenu *edit_section = g_menu_new();
    g_menu_append(edit_section, "Go to Offset...", "win.goto-offset");
    g_menu_append(edit_section, "Find...", "win.find");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(edit_section));
    g_object_unref(edit_section);

    GMenu *view_section = g_menu_new();
    g_menu_append(view_section, "Toggle Hex/Binary", "win.toggle-binary");
    g_menu_append(view_section, "Zoom In", "win.zoom-in");
    g_menu_append(view_section, "Zoom Out", "win.zoom-out");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(view_section));
    g_object_unref(view_section);

    GMenu *ssh_section = g_menu_new();
    g_menu_append(ssh_section, "SSH Connect...", "win.sftp-connect");
    g_menu_append(ssh_section, "Open Remote File...", "win.open-remote");
    g_menu_append(ssh_section, "SSH Disconnect", "win.sftp-disconnect");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(ssh_section));
    g_object_unref(ssh_section);

    GMenu *settings_section = g_menu_new();
    g_menu_append(settings_section, "Settings...", "win.settings");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(settings_section));
    g_object_unref(settings_section);

    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_btn), G_MENU_MODEL(menu));
    g_object_unref(menu);

    adw_header_bar_pack_end(ADW_HEADER_BAR(header), menu_btn);

    /* SSH status button */
    win->ssh_status_btn = gtk_button_new_with_label("SSH: Off");
    gtk_widget_add_css_class(win->ssh_status_btn, "flat");
    g_signal_connect_swapped(win->ssh_status_btn, "clicked",
        G_CALLBACK(gtk_widget_activate_action_variant),
        win->ssh_status_btn);
    /* Clicking the button opens the SFTP dialog */
    gtk_actionable_set_action_name(GTK_ACTIONABLE(win->ssh_status_btn), "win.sftp-connect");
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), win->ssh_status_btn);

    /* Set header as titlebar */
    gtk_window_set_titlebar(GTK_WINDOW(win->window), header);

    /* Main vertical layout */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(win->window), vbox);

    /* Search bar */
    win->search_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(win->search_bar, 8);
    gtk_widget_set_margin_end(win->search_bar, 8);
    gtk_widget_set_margin_top(win->search_bar, 4);
    gtk_widget_set_margin_bottom(win->search_bar, 4);
    gtk_widget_set_visible(win->search_bar, FALSE);

    GtkWidget *search_label = gtk_label_new("Search (hex or text):");
    gtk_box_append(GTK_BOX(win->search_bar), search_label);

    win->search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(win->search_entry), "e.g. FF D8 or hello");
    gtk_widget_set_hexpand(win->search_entry, TRUE);
    g_signal_connect(win->search_entry, "activate", G_CALLBACK(on_search_activate), win);
    g_signal_connect(win->search_entry, "changed", G_CALLBACK(on_search_changed), win);
    gtk_box_append(GTK_BOX(win->search_bar), win->search_entry);

    win->match_label = GTK_LABEL(gtk_label_new(""));
    gtk_box_append(GTK_BOX(win->search_bar), GTK_WIDGET(win->match_label));


    GtkWidget *close_search_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    g_signal_connect_swapped(close_search_btn, "clicked",
        G_CALLBACK(on_close_search), win);
    gtk_box_append(GTK_BOX(win->search_bar), close_search_btn);

    gtk_box_append(GTK_BOX(vbox), win->search_bar);

    /* Content area: hex view + scrollbar */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(hbox, TRUE);
    gtk_box_append(GTK_BOX(vbox), hbox);

    /* Hex drawing area */
    win->hex_view = GTK_DRAWING_AREA(gtk_drawing_area_new());
    gtk_widget_add_css_class(GTK_WIDGET(win->hex_view), "hex-view");
    gtk_widget_set_hexpand(GTK_WIDGET(win->hex_view), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(win->hex_view), TRUE);
    gtk_widget_set_focusable(GTK_WIDGET(win->hex_view), TRUE);
    gtk_drawing_area_set_draw_func(win->hex_view, draw_hex_view, win, NULL);
    gtk_box_append(GTK_BOX(hbox), GTK_WIDGET(win->hex_view));

    /* Scrollbar */
    win->adj = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
    win->scrollbar = GTK_SCROLLBAR(gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, win->adj));
    g_signal_connect(win->adj, "value-changed", G_CALLBACK(on_scrollbar_changed), win);
    gtk_box_append(GTK_BOX(hbox), GTK_WIDGET(win->scrollbar));

    /* Keyboard input */
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_key_pressed), win);
    gtk_widget_add_controller(GTK_WIDGET(win->window), key_ctrl);

    /* Mouse scroll */
    GtkEventController *scroll_ctrl = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll_ctrl, "scroll", G_CALLBACK(on_scroll), win);
    gtk_widget_add_controller(GTK_WIDGET(win->hex_view), scroll_ctrl);

    /* Mouse click */
    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(on_click_pressed), win);
    gtk_widget_add_controller(GTK_WIDGET(win->hex_view), GTK_EVENT_CONTROLLER(click));

    /* Status bar */
    GtkWidget *status = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(status, "statusbar");
    gtk_widget_set_margin_start(status, 8);
    gtk_widget_set_margin_end(status, 8);

    win->status_offset = GTK_LABEL(gtk_label_new("Offset: --"));
    gtk_box_append(GTK_BOX(status), GTK_WIDGET(win->status_offset));

    win->status_value = GTK_LABEL(gtk_label_new("Value: --"));
    gtk_box_append(GTK_BOX(status), GTK_WIDGET(win->status_value));

    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(status), spacer);

    win->status_size = GTK_LABEL(gtk_label_new("Size: 0 bytes"));
    gtk_box_append(GTK_BOX(status), GTK_WIDGET(win->status_size));

    gtk_box_append(GTK_BOX(vbox), status);

    /* Actions */
    hex_actions_setup(win, app);

    /* Apply settings */
    hex_window_apply_settings(win);

    /* Save window size on close */
    g_signal_connect(win->window, "close-request", G_CALLBACK(on_close_request), win);
    g_signal_connect(win->window, "destroy", G_CALLBACK(on_window_destroy), win);

    /* Load last file */
    if (win->settings.last_file[0])
        hex_window_load_file(win, win->settings.last_file);

    gtk_widget_grab_focus(GTK_WIDGET(win->hex_view));
    return win;
}
