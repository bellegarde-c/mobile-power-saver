/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>
#include <glib.h>
#include <unistd.h>

#include "define.h"
#include "utils.h"

void write_to_file (const char *filename,
                    const char *value)
{
    FILE *file;

    if (!g_file_test (filename, G_FILE_TEST_EXISTS))
        return;

    file = fopen(filename, "w");

    g_return_if_fail (file != NULL);

    fprintf (file, "%s", value);
    fclose (file);
}


GList *get_applications (void)
{
    g_autoptr (GDir) sys_dir = NULL;
    g_autofree char *dirname = g_strdup_printf(
        CGROUPS_USER_APPS_DIR, getuid(), getuid()
    );
    const char *app_dir;
    GList *apps = NULL;

    sys_dir = g_dir_open (dirname, 0, NULL);
    if (sys_dir == NULL) {
        g_warning ("Can't find cgroups user app slice: %s", dirname);
        return NULL;
    }

    while ((app_dir = g_dir_read_name (sys_dir)) != NULL) {
        if (g_str_has_prefix (app_dir, "app-") &&
                g_str_has_suffix (app_dir, ".scope")) {
            char *app = g_build_filename (
                dirname, app_dir, "cgroup.freeze", NULL
            );

            if (!g_file_test (app, G_FILE_TEST_EXISTS))
                continue;

            apps = g_list_prepend (apps, app);
        }
    }
    return apps;
}

GList *get_cgroup_services (const char *path)
{
    g_autoptr (GDir) sys_dir = NULL;

    const char *cgroup_dir;
    GList *services = NULL;

    sys_dir = g_dir_open (path, 0, NULL);
    if (sys_dir == NULL) {
        g_warning ("Can't find cgroup: %s", path);
        return NULL;
    }

    while ((cgroup_dir = g_dir_read_name (sys_dir)) != NULL) {
        g_autofree char *cgroup = NULL;

        if (!g_str_has_suffix (cgroup_dir, ".service"))
            continue;

        cgroup = g_build_filename (
            path, cgroup_dir, "cgroup.procs", NULL
        );

        if (!g_file_test (cgroup, G_FILE_TEST_EXISTS))
            continue;

        services = g_list_prepend (services, g_strdup (cgroup));
    }
    return services;
}

GList*
get_cgroup_slices (const char *path)
{
    g_autoptr (GDir) sys_dir = NULL;

    const char *cgroup_dir;
    GList *slices = NULL;

    sys_dir = g_dir_open (path, 0, NULL);
    if (sys_dir == NULL) {
        g_warning ("Can't find cgroup: %s", path);
        return NULL;
    }

    while ((cgroup_dir = g_dir_read_name (sys_dir)) != NULL) {
        g_autofree char *slice = NULL;

        if (!g_str_has_suffix (cgroup_dir, ".slice"))
            continue;

        slice = g_build_filename (
            path, cgroup_dir, NULL
        );

        slices = g_list_prepend (slices, g_strdup (slice));
    }
    return slices;
}

GList*
get_cgroup_pids (const char *path)
{
    GList *pids = NULL;
    pid_t _pid;

    FILE *file = fopen(path, "r");

    g_return_val_if_fail (file != NULL, NULL);

    while(fscanf(file, "%d", &_pid) != EOF) {
        pid_t *pid = g_malloc (sizeof (pid_t));

        *pid = _pid;
        pids = g_list_append (pids, pid);
    }

    fclose (file);

    return pids;
}

GList*
get_list_from_variant (GVariant *value)
{
    GList *list = NULL;
    g_autoptr (GVariantIter) iter;
    const char *item;

    g_variant_get (value, "as", &iter);
    while (g_variant_iter_loop (iter, "s", &item)) {
        list = g_list_append (list, g_strdup (item));
    }

    return list;
}