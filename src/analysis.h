#ifndef HEX_ANALYSIS_H
#define HEX_ANALYSIS_H

#include "window.h"

void analysis_recalc_entropy(HexWindow *win);
void analysis_draw_entropy(GtkDrawingArea *area, cairo_t *cr,
                            int width, int height, gpointer data);
void analysis_entropy_clicked(HexWindow *win, double x, int width);

void analysis_show_byte_frequency(HexWindow *win);
void analysis_show_strings(HexWindow *win);
void analysis_show_checksums(HexWindow *win);

#endif
