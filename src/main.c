#include <adwaita.h>
#include "window.h"
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

static void crash_handler(int sig) {
    void *frames[64];
    int n = backtrace(frames, 64);
    fprintf(stderr, "\n=== CRASH: signal %d ===\n", sig);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    _exit(1);
}

static HexWindow *g_win = NULL;

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    if (g_win) {
        gtk_window_present(GTK_WINDOW(g_win->window));
        return;
    }
    g_win = hex_window_new(app);
    gtk_window_present(GTK_WINDOW(g_win->window));
}

static void on_open(GApplication *app, GFile **files, int n_files,
                     const char *hint, gpointer user_data) {
    (void)hint; (void)user_data;
    on_activate(GTK_APPLICATION(app), NULL);
    if (n_files > 0 && g_win) {
        char *path = g_file_get_path(files[0]);
        if (path) {
            hex_window_load_file(g_win, path);
            g_free(path);
        }
    }
}

static void on_quit(GSimpleAction *action, GVariant *param, gpointer app) {
    (void)action; (void)param;
    g_application_quit(G_APPLICATION(app));
}

int main(int argc, char *argv[]) {
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);

    AdwApplication *app = adw_application_new("com.hex.editor",
                                               G_APPLICATION_HANDLES_OPEN);

    static const GActionEntry app_entries[] = {
        {"quit", on_quit, NULL, NULL, NULL, {0}},
    };

    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries,
                                   G_N_ELEMENTS(app_entries), app);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "open", G_CALLBACK(on_open), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
