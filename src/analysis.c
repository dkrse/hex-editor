#include "analysis.h"
#include <adwaita.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Entropy visualization ── */

void analysis_recalc_entropy(HexWindow *win) {
    g_free(win->entropy_data);
    win->entropy_data = NULL;
    win->entropy_blocks = 0;

    if (!win->data || win->data_len == 0) {
        if (win->entropy_bar)
            gtk_widget_queue_draw(GTK_WIDGET(win->entropy_bar));
        return;
    }

    int nblocks = (int)(win->data_len / 256);
    if (nblocks < 1) nblocks = 1;
    if (nblocks > 512) nblocks = 512;

    gsize block_size = win->data_len / (gsize)nblocks;
    if (block_size < 1) block_size = 1;

    win->entropy_blocks = nblocks;
    win->entropy_data = g_malloc(sizeof(double) * (size_t)nblocks);

    for (int b = 0; b < nblocks; b++) {
        gsize start = (gsize)b * block_size;
        gsize end = (b == nblocks - 1) ? win->data_len : start + block_size;
        gsize len = end - start;

        int freq[256] = {0};
        for (gsize i = start; i < end; i++)
            freq[win->data[i]]++;

        double entropy = 0.0;
        for (int i = 0; i < 256; i++) {
            if (freq[i] == 0) continue;
            double p = (double)freq[i] / (double)len;
            entropy -= p * log2(p);
        }
        win->entropy_data[b] = entropy;
    }

    if (win->entropy_bar)
        gtk_widget_queue_draw(GTK_WIDGET(win->entropy_bar));
}

/* Map entropy (0-8) to color: blue(0) -> green(4) -> yellow(6) -> red(8) */
static void entropy_color(double e, double *r, double *g, double *b) {
    double t = e / 8.0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;

    if (t < 0.5) {
        /* blue -> green */
        double s = t * 2.0;
        *r = 0.0;
        *g = s;
        *b = 1.0 - s;
    } else {
        /* green -> red */
        double s = (t - 0.5) * 2.0;
        *r = s;
        *g = 1.0 - s;
        *b = 0.0;
    }
}

void analysis_draw_entropy(GtkDrawingArea *area, cairo_t *cr,
                            int width, int height, gpointer data) {
    (void)area;
    HexWindow *win = data;

    /* Dark background */
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 1.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    if (!win->entropy_data || win->entropy_blocks <= 0) return;

    double bar_w = (double)width / (double)win->entropy_blocks;
    for (int i = 0; i < win->entropy_blocks; i++) {
        double r, g, b;
        entropy_color(win->entropy_data[i], &r, &g, &b);
        cairo_set_source_rgba(cr, r, g, b, 0.9);
        cairo_rectangle(cr, (double)i * bar_w, 0, bar_w + 1, height);
        cairo_fill(cr);
    }
}

void analysis_entropy_clicked(HexWindow *win, double x, int width) {
    if (!win->entropy_data || win->entropy_blocks <= 0 || width <= 0) return;

    int block = (int)(x / ((double)width / (double)win->entropy_blocks));
    if (block < 0) block = 0;
    if (block >= win->entropy_blocks) block = win->entropy_blocks - 1;

    gsize block_size = win->data_len / (gsize)win->entropy_blocks;
    win->cursor_pos = (gsize)block * block_size;
    if (win->cursor_pos >= win->data_len)
        win->cursor_pos = win->data_len > 0 ? win->data_len - 1 : 0;

    win->cursor_nibble = 0;
    win->selection_start = win->cursor_pos;
    win->selection_end = win->cursor_pos;
    hex_window_queue_redraw(win);
}

/* ── Byte Frequency Histogram ── */

typedef struct { int byte; gsize count; } ByteCount;

static int bc_cmp_desc(const void *a, const void *b) {
    gsize ca = ((const ByteCount *)a)->count;
    gsize cb = ((const ByteCount *)b)->count;
    return (cb > ca) - (cb < ca);
}

static void draw_histogram(GtkDrawingArea *area, cairo_t *cr,
                            int width, int height, gpointer data) {
    (void)area;
    gsize *counts = data;

    cairo_set_source_rgba(cr, 0.12, 0.12, 0.12, 1.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    /* Find max */
    gsize max_count = 1;
    for (int i = 0; i < 256; i++)
        if (counts[i] > max_count) max_count = counts[i];

    double bar_w = (double)width / 256.0;
    int margin_bottom = 20;
    int chart_h = height - margin_bottom;

    for (int i = 0; i < 256; i++) {
        double h = (double)counts[i] / (double)max_count * (double)chart_h;
        double r, g, b;
        entropy_color((double)i / 32.0, &r, &g, &b);
        cairo_set_source_rgba(cr, r, g, b, 0.8);
        cairo_rectangle(cr, (double)i * bar_w, (double)chart_h - h, bar_w, h);
        cairo_fill(cr);
    }

    /* X-axis labels */
    cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 1.0);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    for (int i = 0; i <= 0xFF; i += 0x40) {
        char lbl[8];
        snprintf(lbl, sizeof(lbl), "%02X", i);
        cairo_move_to(cr, (double)i * bar_w, (double)height - 4);
        cairo_show_text(cr, lbl);
    }
}

static void freq_dialog_destroy(GtkWidget *w, gpointer data) {
    (void)w;
    g_free(data);
}

void analysis_show_byte_frequency(HexWindow *win) {
    if (!win->data || win->data_len == 0) return;

    gsize *counts = g_new0(gsize, 256);
    for (gsize i = 0; i < win->data_len; i++)
        counts[win->data[i]]++;

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Byte Frequency");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 700, 450);
    gtk_window_set_titlebar(GTK_WINDOW(dialog), adw_header_bar_new());

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_widget_set_margin_top(vbox, 4);
    gtk_widget_set_margin_bottom(vbox, 8);

    /* Histogram chart */
    GtkWidget *chart = gtk_drawing_area_new();
    gtk_widget_set_vexpand(chart, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(chart), draw_histogram, counts, NULL);
    gtk_box_append(GTK_BOX(vbox), chart);

    /* Top 10 most frequent */
    ByteCount sorted[256];
    for (int i = 0; i < 256; i++) { sorted[i].byte = i; sorted[i].count = counts[i]; }
    qsort(sorted, 256, sizeof(ByteCount), bc_cmp_desc);

    GString *top = g_string_new("Top: ");
    for (int i = 0; i < 10 && i < 256 && sorted[i].count > 0; i++) {
        char ch = (sorted[i].byte >= 32 && sorted[i].byte < 127) ? (char)sorted[i].byte : '.';
        g_string_append_printf(top, "0x%02X '%c' (%lu)   ",
                                sorted[i].byte, ch, (unsigned long)sorted[i].count);
    }
    GtkWidget *top_label = gtk_label_new(top->str);
    gtk_label_set_xalign(GTK_LABEL(top_label), 0);
    gtk_label_set_selectable(GTK_LABEL(top_label), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(top_label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(vbox), top_label);
    g_string_free(top, TRUE);

    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    g_signal_connect(dialog, "destroy", G_CALLBACK(freq_dialog_destroy), counts);
    gtk_window_present(GTK_WINDOW(dialog));
}

/* ── Strings Extraction ── */

typedef struct {
    gsize offset;
    int   len;
    char *str;
} FoundString;

typedef struct {
    HexWindow    *win;
    GtkWindow    *dialog;
    FoundString  *strings;
    int           count;
} StringsCtx;

static void strings_ctx_free(GtkWidget *w, gpointer data) {
    (void)w;
    StringsCtx *ctx = data;
    for (int i = 0; i < ctx->count; i++)
        g_free(ctx->strings[i].str);
    g_free(ctx->strings);
    g_free(ctx);
}

static void on_string_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    StringsCtx *ctx = data;
    if (!row) return;
    int idx = gtk_list_box_row_get_index(row);
    if (idx < 0 || idx >= ctx->count) return;

    ctx->win->cursor_pos = ctx->strings[idx].offset;
    ctx->win->cursor_nibble = 0;
    ctx->win->selection_start = ctx->strings[idx].offset;
    ctx->win->selection_end = ctx->strings[idx].offset + (gsize)ctx->strings[idx].len - 1;
    hex_window_queue_redraw(ctx->win);
    gtk_window_destroy(ctx->dialog);
}

void analysis_show_strings(HexWindow *win) {
    if (!win->data || win->data_len == 0) return;

    int min_len = 4;

    /* Find strings */
    GPtrArray *found = g_ptr_array_new();
    gsize run_start = 0;
    int run_len = 0;

    for (gsize i = 0; i <= win->data_len; i++) {
        gboolean printable = (i < win->data_len) &&
                              (win->data[i] >= 32 && win->data[i] < 127);
        if (printable) {
            if (run_len == 0) run_start = i;
            run_len++;
        } else {
            if (run_len >= min_len) {
                FoundString *fs = g_new(FoundString, 1);
                fs->offset = run_start;
                fs->len = run_len;
                int copy_len = run_len > 200 ? 200 : run_len;
                fs->str = g_strndup((char *)win->data + run_start, (gsize)copy_len);
                g_ptr_array_add(found, fs);
            }
            run_len = 0;
        }
    }

    StringsCtx *ctx = g_new0(StringsCtx, 1);
    ctx->win = win;
    ctx->count = (int)found->len;
    ctx->strings = g_new(FoundString, (gsize)ctx->count);
    for (int i = 0; i < ctx->count; i++) {
        FoundString *fs = g_ptr_array_index(found, i);
        ctx->strings[i] = *fs;
        g_free(fs);
    }
    g_ptr_array_unref(found);

    /* Dialog */
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Strings");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 500);
    gtk_window_set_titlebar(GTK_WINDOW(dialog), adw_header_bar_new());
    ctx->dialog = GTK_WINDOW(dialog);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_widget_set_margin_top(vbox, 4);
    gtk_widget_set_margin_bottom(vbox, 8);

    char info[128];
    snprintf(info, sizeof(info), "Found %d strings (min length %d)", ctx->count, min_len);
    GtkWidget *info_label = gtk_label_new(info);
    gtk_label_set_xalign(GTK_LABEL(info_label), 0);
    gtk_box_append(GTK_BOX(vbox), info_label);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget *list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
    g_signal_connect(list, "row-activated", G_CALLBACK(on_string_row_activated), ctx);

    for (int i = 0; i < ctx->count; i++) {
        char row_text[512];
        snprintf(row_text, sizeof(row_text), "0x%08lX  (%3d) %s",
                 (unsigned long)ctx->strings[i].offset,
                 ctx->strings[i].len,
                 ctx->strings[i].str);
        GtkWidget *lbl = gtk_label_new(row_text);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 4);
        gtk_widget_set_margin_top(lbl, 1);
        gtk_widget_set_margin_bottom(lbl, 1);
        gtk_list_box_append(GTK_LIST_BOX(list), lbl);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
    gtk_box_append(GTK_BOX(vbox), scroll);

    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    g_signal_connect(dialog, "destroy", G_CALLBACK(strings_ctx_free), ctx);
    gtk_window_present(GTK_WINDOW(dialog));
}

/* ── Checksum Calculator ── */

static uint32_t crc32_table[256];
static gboolean crc32_table_init = FALSE;

static void crc32_init(void) {
    if (crc32_table_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (c & 1 ? 0xEDB88320 : 0);
        crc32_table[i] = c;
    }
    crc32_table_init = TRUE;
}

static uint32_t crc32_compute(const unsigned char *data, gsize len) {
    crc32_init();
    uint32_t crc = 0xFFFFFFFF;
    for (gsize i = 0; i < len; i++)
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    return crc ^ 0xFFFFFFFF;
}

static void on_copy_checksums(GtkButton *btn, gpointer data) {
    (void)btn;
    GtkLabel *label = GTK_LABEL(data);
    const char *text = gtk_label_get_text(label);
    GdkClipboard *cb = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(cb, text);
}

void analysis_show_checksums(HexWindow *win) {
    if (!win->data || win->data_len == 0) return;

    /* Determine range */
    gsize lo, hi;
    if (win->selection_start != win->selection_end) {
        lo = MIN(win->selection_start, win->selection_end);
        hi = MAX(win->selection_start, win->selection_end);
    } else {
        lo = 0;
        hi = win->data_len - 1;
    }
    gsize len = hi - lo + 1;
    const unsigned char *data = win->data + lo;

    /* CRC32 */
    uint32_t crc = crc32_compute(data, len);

    /* MD5, SHA1, SHA256 via GChecksum */
    GChecksum *md5 = g_checksum_new(G_CHECKSUM_MD5);
    g_checksum_update(md5, data, (gssize)len);
    const char *md5_str = g_checksum_get_string(md5);

    GChecksum *sha1 = g_checksum_new(G_CHECKSUM_SHA1);
    g_checksum_update(sha1, data, (gssize)len);
    const char *sha1_str = g_checksum_get_string(sha1);

    GChecksum *sha256 = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(sha256, data, (gssize)len);
    const char *sha256_str = g_checksum_get_string(sha256);

    /* Build result text */
    char range_info[128];
    if (lo == 0 && hi == win->data_len - 1)
        snprintf(range_info, sizeof(range_info), "Entire file (%lu bytes)", (unsigned long)len);
    else
        snprintf(range_info, sizeof(range_info), "Selection 0x%lX-0x%lX (%lu bytes)",
                 (unsigned long)lo, (unsigned long)hi, (unsigned long)len);

    GString *result = g_string_new(NULL);
    g_string_append_printf(result, "%s\n\n", range_info);
    g_string_append_printf(result, "CRC32:   %08X\n", crc);
    g_string_append_printf(result, "MD5:     %s\n", md5_str);
    g_string_append_printf(result, "SHA1:    %s\n", sha1_str);
    g_string_append_printf(result, "SHA256:  %s", sha256_str);

    /* Dialog */
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Checksums");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 550, 250);
    gtk_window_set_titlebar(GTK_WINDOW(dialog), adw_header_bar_new());

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 16);

    GtkWidget *label = gtk_label_new(result->str);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_widget_set_vexpand(label, TRUE);
    gtk_box_append(GTK_BOX(vbox), label);

    GtkWidget *copy_btn = gtk_button_new_with_label("Copy All");
    gtk_widget_set_halign(copy_btn, GTK_ALIGN_END);
    g_signal_connect(copy_btn, "clicked", G_CALLBACK(on_copy_checksums), label);
    gtk_box_append(GTK_BOX(vbox), copy_btn);

    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    gtk_window_present(GTK_WINDOW(dialog));

    g_string_free(result, TRUE);
    g_checksum_free(md5);
    g_checksum_free(sha1);
    g_checksum_free(sha256);
}
