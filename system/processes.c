/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>

#include <gio/gio.h>

#include "processes.h"
#include "../common/utils.h"

#define MAX_BUFSZ (1024*64*2)
#define PROCPATHLEN 64  // must hold /proc/2000222000/task/2000222000/cmdline

struct _ProcessesPrivate {
    GList *processes;
};

G_DEFINE_TYPE_WITH_CODE (
    Processes,
    processes,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Processes)
)

struct Process {
    pid_t pid;
    char *cmdline;
};

static void
process_free (gpointer user_data)
{
    struct Process *process = user_data;

    g_free (process->cmdline);
    g_free (process);
}

// From https://gitlab.com/procps-ng/procps
//
static int read_unvectored(char *restrict const  dst,
                           unsigned              sz,
                           const char           *whom,
                           const char           *what,
                           char                  sep)
{
    char path[PROCPATHLEN];
    int fd, len;
    unsigned n = 0;

    if(sz <= 0) return 0;
    if(sz >= INT_MAX) sz = INT_MAX-1;
    dst[0] = '\0';

    len = snprintf(path, sizeof(path), "%s/%s", whom, what);
    if(len <= 0 || (size_t)len >= sizeof(path)) return 0;
    fd = open(path, O_RDONLY);
    if(fd==-1) return 0;

    for(;;){
        ssize_t r = read(fd,dst+n,sz-n);
        if(r==-1){
            if(errno==EINTR) continue;
            break;
        }
        if(r<=0) break;  // EOF
        n += r;
        if(n==sz) {      // filled the buffer
            --n;         // make room for '\0'
            break;
        }
    }
    close(fd);
    if(n){
        unsigned i = n;
        while(i && dst[i-1]=='\0') --i; // skip trailing zeroes
        while(i--)
            if(dst[i]=='\n' || dst[i]=='\0') dst[i]=sep;
        if(dst[n-1]==' ') dst[n-1]='\0';
    }
    dst[n] = '\0';
    return n;
}

static gboolean
process_in_list (GList          *names,
                 struct Process *process)
{
    char *name;

    if (g_strcmp0 (process->cmdline, "") == 0)
        return FALSE;

    GFOREACH (names, name) {
        if (g_strrstr (process->cmdline, name) != NULL)
            return TRUE;
    }
    return FALSE;
}

static GList *
get_processes (Processes *self)
{
    GList *processes = NULL;
    g_autoptr (GDir) proc_dir = NULL;
    const char *pid_dir;

    proc_dir = g_dir_open ("/proc", 0, NULL);
    if (proc_dir == NULL) {
        g_warning ("/proc not mounted");
        return NULL;
    }

    while ((pid_dir = g_dir_read_name (proc_dir)) != NULL) {
        g_autofree char *contents = NULL;
        g_autofree char *directory = g_build_filename (
            "/proc", pid_dir, NULL
        );

        if ((contents = g_malloc (MAX_BUFSZ)) == NULL)
            return NULL;

        if (read_unvectored(contents, MAX_BUFSZ, directory, "cmdline", ' ')) {
            struct Process *process = g_malloc (sizeof (struct Process));

            process->cmdline = g_strdup (contents);
            sscanf (pid_dir, "%d", &process->pid);

            processes = g_list_prepend (processes, process);
        }
    }

    return processes;
}

static void
processes_dispose (GObject *processes)
{
    G_OBJECT_CLASS (processes_parent_class)->dispose (processes);
}

static void
processes_finalize (GObject *processes)
{
    Processes *self = PROCESSES (processes);

    g_list_free_full (self->priv->processes, process_free);

    G_OBJECT_CLASS (processes_parent_class)->finalize (processes);
}

static void
processes_class_init (ProcessesClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = processes_dispose;
    object_class->finalize = processes_finalize;
}

static void
processes_init (Processes *self)
{
    self->priv = processes_get_instance_private (self);

    self->priv->processes = NULL;
}

/**
 * processes_new:
 *
 * Creates a new #Processes
 *
 * Returns: (transfer full): a new #Processes
 *
 **/
GObject *
processes_new (void)
{
    GObject *processes;

    processes = g_object_new (TYPE_PROCESSES, NULL);

    return processes;
}

/**
 * processes_suspend:
 *
 * Suspend processes in list
 *
 * @param #Processes
 * @param names: processes list
 */
void
processes_suspend (Processes *self,
                   GList     *names) {

    struct Process *process;

    if (self->priv->processes != NULL) {
        g_list_free_full (self->priv->processes, g_free);
    }

    self->priv->processes = get_processes (self);
    g_return_if_fail (self->priv->processes != NULL);

    GFOREACH (self->priv->processes, process)
        if (process_in_list (names, process))
            kill (process->pid, SIGSTOP);
}

/**
 * processes_resume:
 *
 * resume processes
 *
 * @param #Processes
 * @param names: processes list
 *
 */
void
processes_resume (Processes *self,
                  GList     *names) {

    struct Process *process;

    if (self->priv->processes == NULL)
        return;

    GFOREACH (self->priv->processes, process)
        if (process_in_list (names, process))
            kill (process->pid, SIGCONT);

    g_list_free_full (self->priv->processes, g_free);
    self->priv->processes = NULL;
}