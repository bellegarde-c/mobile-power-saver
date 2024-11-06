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
        CGROUPS_APPS_FREEZE_DIR, getuid(), getuid()

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