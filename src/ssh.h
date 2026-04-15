#ifndef SSH_H
#define SSH_H

#include <glib.h>
#include <gio/gio.h>

gboolean ssh_path_is_remote(const char *path);

const char *ssh_to_remote_path(const char *ssh_mount, const char *ssh_remote_path,
                               const char *local_path, char *buf, size_t buflen);

GPtrArray *ssh_argv_new(const char *host, const char *user, int port,
                        const char *key, const char *ctl_path);

void ssh_ctl_start(char *ctl_dir, size_t ctl_dir_size,
                   char *ctl_path, size_t ctl_path_size,
                   const char *host, const char *user, int port, const char *key);

void ssh_ctl_stop(char *ctl_path, char *ctl_dir,
                  const char *host, const char *user);

gboolean ssh_spawn_sync(GPtrArray *argv, char **out_stdout, gsize *out_len);

/* Read remote file (binary-safe). Caller must g_free(*out_contents). */
gboolean ssh_cat_file(const char *host, const char *user, int port,
                      const char *key, const char *ctl_path,
                      const char *remote_path,
                      char **out_contents, gsize *out_len,
                      gsize max_file_size);

/* Write binary content to remote file. */
gboolean ssh_write_file(const char *host, const char *user, int port,
                        const char *key, const char *ctl_path,
                        const char *remote_path,
                        const char *content, gsize len);

#endif
