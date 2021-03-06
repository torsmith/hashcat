/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "memory.h"
#include "event.h"
#include "user_options.h"
#include "shared.h"
#include "pidfile.h"

static int check_running_process (hashcat_ctx_t *hashcat_ctx)
{
  pidfile_ctx_t *pidfile_ctx = hashcat_ctx->pidfile_ctx;

  char *pidfile_filename = pidfile_ctx->filename;

  FILE *fp = fopen (pidfile_filename, "rb");

  if (fp == NULL) return 0;

  pidfile_data_t *pd = (pidfile_data_t *) hcmalloc (sizeof (pidfile_data_t));

  const size_t nread = fread (pd, sizeof (pidfile_data_t), 1, fp);

  fclose (fp);

  if (nread != 1)
  {
    event_log_error (hashcat_ctx, "Cannot read %s", pidfile_filename);

    return -1;
  }

  if (pd->pid)
  {
    #if defined (_POSIX)

    char *pidbin;

    hc_asprintf (&pidbin, "/proc/%u/cmdline", pd->pid);

    if (hc_path_exist (pidbin) == true)
    {
      event_log_error (hashcat_ctx, "Already an instance running on pid %u", pd->pid);

      return -1;
    }

    hcfree (pidbin);

    #elif defined (_WIN)

    HANDLE hProcess = OpenProcess (PROCESS_ALL_ACCESS, FALSE, pd->pid);

    char *pidbin  = (char *) hcmalloc (HCBUFSIZ_LARGE);
    char *pidbin2 = (char *) hcmalloc (HCBUFSIZ_LARGE);

    int pidbin_len  = GetModuleFileName (NULL, pidbin, HCBUFSIZ_LARGE);
    int pidbin2_len = GetModuleFileNameEx (hProcess, NULL, pidbin2, HCBUFSIZ_LARGE);

    pidbin[pidbin_len]   = 0;
    pidbin2[pidbin2_len] = 0;

    if (pidbin2_len)
    {
      if (strcmp (pidbin, pidbin2) == 0)
      {
        event_log_error (hashcat_ctx, "Already an instance %s running on pid %d", pidbin2, pd->pid);

        return -1;
      }
    }

    hcfree (pidbin2);
    hcfree (pidbin);

    #endif
  }

  hcfree (pd);

  return 0;
}

static int init_pidfile (hashcat_ctx_t *hashcat_ctx)
{
  pidfile_ctx_t *pidfile_ctx = hashcat_ctx->pidfile_ctx;

  pidfile_data_t *pd = (pidfile_data_t *) hcmalloc (sizeof (pidfile_data_t));

  pidfile_ctx->pd = pd;

  const int rc = check_running_process (hashcat_ctx);

  if (rc == -1) return -1;

  #if defined (_POSIX)
  pd->pid = getpid ();
  #elif defined (_WIN)
  pd->pid = GetCurrentProcessId ();
  #endif

  return 0;
}

static int write_pidfile (hashcat_ctx_t *hashcat_ctx)
{
  const pidfile_ctx_t *pidfile_ctx = hashcat_ctx->pidfile_ctx;

  pidfile_data_t *pd = pidfile_ctx->pd;

  char *pidfile_filename = pidfile_ctx->filename;

  FILE *fp = fopen (pidfile_filename, "wb");

  if (fp == NULL)
  {
    event_log_error (hashcat_ctx, "%s: %m", pidfile_filename);

    return -1;
  }

  fwrite (pd, sizeof (pidfile_data_t), 1, fp);

  fflush (fp);

  fclose (fp);

  return 0;
}

void unlink_pidfile (hashcat_ctx_t *hashcat_ctx)
{
  pidfile_ctx_t *pidfile_ctx = hashcat_ctx->pidfile_ctx;

  unlink (pidfile_ctx->filename);
}

int pidfile_ctx_init (hashcat_ctx_t *hashcat_ctx)
{
  folder_config_t *folder_config = hashcat_ctx->folder_config;
  pidfile_ctx_t   *pidfile_ctx   = hashcat_ctx->pidfile_ctx;
  user_options_t  *user_options  = hashcat_ctx->user_options;

  hc_asprintf (&pidfile_ctx->filename, "%s/%s.pid", folder_config->session_dir, user_options->session);

  const int rc_init_pidfile = init_pidfile (hashcat_ctx);

  if (rc_init_pidfile == -1) return -1;

  write_pidfile (hashcat_ctx);

  return 0;
}

void pidfile_ctx_destroy (hashcat_ctx_t *hashcat_ctx)
{
  pidfile_ctx_t *pidfile_ctx = hashcat_ctx->pidfile_ctx;

  hcfree (pidfile_ctx->filename);

  hcfree (pidfile_ctx->pd);

  memset (pidfile_ctx, 0, sizeof (pidfile_ctx_t));
}
