/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>

#include <gio/gio.h>

#include "bus.h"
#include "services.h"
#include "../common/define.h"
#include "../common/utils.h"

struct _ServicesPrivate {
    GBusType service_type;
};

G_DEFINE_TYPE_WITH_CODE (
    Services,
    services,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Services)
)

static GList *
get_cgroups_paths (Services *self)
{
    GList *paths = NULL;

    if (self->priv->service_type == G_BUS_TYPE_SESSION) {
        paths = get_cgroup_slices (
            g_strdup_printf(
                CGROUPS_USER_DIR, getuid(), getuid()
            )
        );
    } else {
        paths = g_list_append (
            paths,
            g_strdup (CGROUPS_SYSTEM_SERVICES_DIR)
        );
    }

    return paths;
}

static void
services_set_service_freeze_state (Services   *self,
                                   const char *path,
                                   const char *service,
                                   const char *state)
{
    g_autofree char *filename = NULL;

    filename = g_build_filename (
        path, service, "cgroup.freeze", NULL
    );

    if (g_file_test (filename, G_FILE_TEST_EXISTS))
        write_to_file (filename, state);
}

static void
services_set_services_freeze_state (Services   *self,
                                    GList      *blacklist,
                                    const char *state)
{
    GList *paths = get_cgroups_paths (self);
    const char *path;
    const char *service;

    GFOREACH (paths, path) {
        GList *services = get_cgroup_services (path);

        GFOREACH_SUB (services, service) {
            g_autofree char *filename = NULL;

            if (in_list (blacklist, service))
                continue;

            services_set_service_freeze_state (self, path, service, state);
        }
        g_list_free_full (services, g_free);
    }
    g_list_free_full (paths, g_free);
}

static void
services_dispose (GObject *services)
{
    G_OBJECT_CLASS (services_parent_class)->dispose (services);
}

static void
services_finalize (GObject *services)
{
    G_OBJECT_CLASS (services_parent_class)->finalize (services);
}

static void
services_class_init (ServicesClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = services_dispose;
    object_class->finalize = services_finalize;
}

static void
services_init (Services *self)
{
    self->priv = services_get_instance_private (self);
}

/**
 * services_new:
 *
 * Creates a new #Services
 *
 * @param #GBusType: target service type (G_BUS_TYPE_SYSTEM/G_BUS_TYPE_SESSION).
 * Yes, we use DBus type :)
 *
 * Returns: (transfer full): a new #Services
 *
 **/
GObject *
services_new (GBusType service_type)
{
    GObject *services;

    services = g_object_new (TYPE_SERVICES, NULL);

    SERVICES (services)->priv->service_type = service_type;

    return services;
}

/**
 * services_freeze:
 *
 * Freeze services
 *
 * @param #Services
 * @param services: services to freeze
 *
 **/
void
services_freeze (Services *self,
                 GList    *services)
{
    GList *paths = get_cgroups_paths (self);
    const char *service;
    const char *path;

    GFOREACH (services, service) {
        GFOREACH_SUB (paths, path) {
            services_set_service_freeze_state (self, path, service, "1");
        }
    }

    g_list_free_full (paths, g_free);
}

/**
 * services_unfreeze:
 *
 * Unfreeze services
 *
 * @param #Services
 * @param services: services to unfreeze
 *
 **/
void
services_unfreeze (Services *self,
                   GList   *services)
{
    GList *paths = get_cgroups_paths (self);
    const char *service;
    const char *path;

    GFOREACH (services, service) {
        GFOREACH_SUB (paths, path) {
            services_set_service_freeze_state (self, path, service, "0");
        }
    }

    g_list_free_full (paths, g_free);
}

/**
 * services_freeze_all:
 *
 * Freeze all services
 *
 * @param #Services
 * @param blacklist: blacklisted services
 *
 **/
void
services_freeze_all (Services *self,
                     GList    *blacklist)
{
    services_set_services_freeze_state (self, blacklist, "1");
}

/**
 * services_unfreeze_all:
 *
 * Unfreeze all services
 *
 * @param #Services
 * @param blacklist: blacklisted services
 *
 **/
void
services_unfreeze_all (Services *self,
                       GList    *blacklist)
{
    services_set_services_freeze_state (self, blacklist, "0");
}