/* WirePlumber
 *
 * Copyright © 2020 Collabora Ltd.
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "debug.h"
#include <pipewire/pipewire.h>

static gsize initialized = 0;
static gboolean use_color = FALSE;
static gboolean output_is_journal = FALSE;
static GPatternSpec **enabled_categories = NULL;
static gint enabled_level = 4; /* MESSAGE */

struct common_fields
{
  const gchar *log_domain;
  const gchar *file;
  const gchar *line;
  const gchar *func;
  const gchar *message;
  GLogField *message_field;
  gint log_level;
  GType object_type;
  gconstpointer object;
};

/* reference: https://en.wikipedia.org/wiki/ANSI_escape_code#3/4_bit */
#define COLOR_RED            "\033[1;31m"
#define COLOR_GREEN          "\033[1;32m"
#define COLOR_YELLOW         "\033[1;33m"
#define COLOR_BLUE           "\033[1;34m"
#define COLOR_MAGENTA        "\033[1;35m"
#define COLOR_CYAN           "\033[1;36m"
#define COLOR_BRIGHT_RED     "\033[1;91m"
#define COLOR_BRIGHT_GREEN   "\033[1;92m"
#define COLOR_BRIGHT_YELLOW  "\033[1;93m"
#define COLOR_BRIGHT_BLUE    "\033[1;94m"
#define COLOR_BRIGHT_MAGENTA "\033[1;95m"
#define COLOR_BRIGHT_CYAN    "\033[1;96m"

#define RESET_COLOR          "\033[0m"

/* our palette */
#define DOMAIN_COLOR    COLOR_MAGENTA
#define LOCATION_COLOR  COLOR_BLUE
#define OBJECT_COLOR    COLOR_YELLOW

/*
 * priority numbers are based on GLib's gmessages.c
 * reference: http://man7.org/linux/man-pages/man3/syslog.3.html#DESCRIPTION
 */
static const struct {
  GLogLevelFlags log_level;
  gchar name[6];
  gchar priority[2];
  gchar color[8];
} log_level_info[] = {
  { 0,                    "UNK  ", "5", COLOR_BRIGHT_RED },
  { G_LOG_LEVEL_ERROR,    "ERROR", "3", COLOR_RED },
  { G_LOG_LEVEL_CRITICAL, "CRIT ", "4", COLOR_BRIGHT_MAGENTA },
  { G_LOG_LEVEL_WARNING,  "WARN ", "4", COLOR_BRIGHT_YELLOW },
  { G_LOG_LEVEL_MESSAGE,  "MSG  ", "5", COLOR_BRIGHT_GREEN },
  { G_LOG_LEVEL_INFO,     "INFO ", "6", COLOR_GREEN },
  { G_LOG_LEVEL_DEBUG,    "DEBUG", "7", COLOR_CYAN },
  { WP_LOG_LEVEL_TRACE,   "TRACE", "7", COLOR_YELLOW },
};

/* map glib's log levels, which are flags in the range (1<<2) to (1<<8),
  to the 1-7 range; first calculate the integer part of log2(log_level)
  to bring it down to 2-8 and substract 1 */
static inline gint
log_level_index (GLogLevelFlags log_level)
{
  gint logarithm = 0;
  while ((log_level >>= 1) != 0)
    logarithm += 1;
  return (logarithm >= 2 && logarithm <= 8) ? (logarithm - 1) : 0;
}

static void
wp_debug_initialize (void)
{
  if (g_once_init_enter (&initialized)) {
    const gchar *debug = NULL;
    gint n_tokens = 0;
    gchar **tokens = NULL;
    gchar **categories = NULL;

    debug = g_getenv ("WIREPLUMBER_DEBUG");
    if (debug && debug[0] != '\0') {
      /* WP_DEBUG=level:category1,category2 */
      tokens = pw_split_strv (debug, ":", 2, &n_tokens);

      /* set the log level */
      enabled_level = atoi (tokens[0]);

      /* enable filtering of debug categories */
      if (n_tokens > 1) {
        categories = pw_split_strv (tokens[1], ",", INT_MAX, &n_tokens);

        /* alloc space to hold the GPatternSpec pointers */
        enabled_categories = g_malloc_n ((n_tokens + 1), sizeof (gpointer));
        if (!enabled_categories)
          g_error ("out of memory");

        for (gint i = 0; i < n_tokens; i++)
          enabled_categories[i] = g_pattern_spec_new (categories[i]);
        enabled_categories[n_tokens] = NULL;
      }
    }

    use_color = g_log_writer_supports_color (fileno (stderr));
    output_is_journal = g_log_writer_is_journald (fileno (stderr));

    if (categories)
      pw_free_strv (categories);
    if (tokens)
      pw_free_strv (tokens);

    g_once_init_leave (&initialized, TRUE);
  }
}

static inline void
write_debug_message (FILE *s, struct common_fields *cf)
{
  gint64 now;
  time_t now_secs;
  struct tm now_tm;
  gchar time_buf[128];

  now = g_get_real_time ();
  now_secs = (time_t) (now / G_USEC_PER_SEC);
  localtime_r (&now_secs, &now_tm);
  strftime (time_buf, sizeof (time_buf), "%H:%M:%S", &now_tm);

  fprintf (s, "%s.%06d %s%s %s%20.20s %s%s:%s:%s:%s %s\n",
      /* timestamp */
      time_buf,
      (gint) (now % G_USEC_PER_SEC),
      /* level */
      use_color ? log_level_info[cf->log_level].color : "",
      log_level_info[cf->log_level].name,
      /* domain */
      use_color ? DOMAIN_COLOR : "",
      cf->log_domain,
      /* file, line, function */
      use_color ? LOCATION_COLOR : "",
      cf->file,
      cf->line,
      cf->func,
      use_color ? RESET_COLOR : "",
      /* message */
      cf->message);
  fflush (s);
}

static inline gchar *
format_message (struct common_fields *cf)
{
  return g_strdup_printf ("%s<%s%s%p>%s %s",
      use_color ? OBJECT_COLOR : "",
      cf->object_type != 0 ? g_type_name (cf->object_type) : "",
      cf->object_type != 0 ? ":" : "",
      cf->object,
      use_color ? RESET_COLOR : "",
      cf->message);
}

static inline void
extract_common_fields (struct common_fields *cf, const GLogField *fields,
    gsize n_fields)
{
  for (gint i = 0; i < n_fields; i++) {
    if (g_strcmp0 (fields[i].key, "GLIB_DOMAIN") == 0) {
      cf->log_domain = fields[i].value;
    }
    else if (g_strcmp0 (fields[i].key, "MESSAGE") == 0) {
      cf->message = fields[i].value;
      cf->message_field = (GLogField *) &fields[i];
    }
    else if (g_strcmp0 (fields[i].key, "CODE_FILE") == 0) {
      cf->file = fields[i].value;
    }
    else if (g_strcmp0 (fields[i].key, "CODE_LINE") == 0) {
      cf->line = fields[i].value;
    }
    else if (g_strcmp0 (fields[i].key, "CODE_FUNC") == 0) {
      cf->func = fields[i].value;
    }
    else if (g_strcmp0 (fields[i].key, "WP_OBJECT_TYPE") == 0 &&
        fields[i].length == sizeof (GType)) {
      cf->object_type = *((GType *) fields[i].value);
    }
    else if (g_strcmp0 (fields[i].key, "WP_OBJECT") == 0 &&
        fields[i].length == sizeof (gconstpointer)) {
      cf->object = *((gconstpointer *) fields[i].value);
    }
  }
}

gboolean
wp_log_level_is_enabled (GLogLevelFlags log_level)
{
  wp_debug_initialize ();
  return log_level_index (log_level) <= enabled_level;
}

GLogWriterOutput
wp_log_writer_default (GLogLevelFlags log_level,
    const GLogField *fields, gsize n_fields, gpointer user_data)
{
  struct common_fields cf = {0};
  g_autofree gchar *full_message = NULL;

  g_return_val_if_fail (fields != NULL, G_LOG_WRITER_UNHANDLED);
  g_return_val_if_fail (n_fields > 0, G_LOG_WRITER_UNHANDLED);

  /* in the unlikely event that someone messed with stderr... */
  if (G_UNLIKELY (!stderr || fileno (stderr) < 0))
    return G_LOG_WRITER_UNHANDLED;

  /* one-time initialization */
  wp_debug_initialize ();

  cf.log_level = log_level_index (log_level);

  /* check if debug level is enabled */
  if (cf.log_level > enabled_level)
    return G_LOG_WRITER_UNHANDLED;

  extract_common_fields (&cf, fields, n_fields);

  /* check if debug category is enabled */
  if (cf.log_domain && enabled_categories) {
    GPatternSpec **cat = enabled_categories;
    guint len;
    g_autofree gchar *reverse_domain = NULL;

    len = strlen (cf.log_domain);
    reverse_domain = g_strreverse (g_strndup (cf.log_domain, len));

    cat = enabled_categories;
    while (*cat && !g_pattern_match (*cat, len, cf.log_domain, reverse_domain))
      cat++;

    /* reached the end of the enabled categories,
       therefore our category is not enabled */
    if (*cat == NULL)
      return G_LOG_WRITER_UNHANDLED;
  }

  /* format the message to include the object */
  if (cf.object && cf.message) {
    cf.message_field->value = cf.message = full_message =
        format_message (&cf);
  }

  /* write complete field information to the journal if we are logging to it */
  if (output_is_journal &&
      g_log_writer_journald (log_level, fields, n_fields, user_data) == G_LOG_WRITER_HANDLED)
    return G_LOG_WRITER_HANDLED;

  write_debug_message (stderr, &cf);
  return G_LOG_WRITER_HANDLED;
}

void
wp_log_structured_standard (
    const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *file,
    const gchar *line,
    const gchar *func,
    GType object_type,
    gconstpointer object,
    const gchar *message_format,
    ...)
{
  g_autofree gchar *message = NULL;
  GLogField fields[8] = {
    { "PRIORITY", log_level_info[log_level_index (log_level)].priority, -1 },
    { "CODE_FILE", file, -1 },
    { "CODE_LINE", line, -1 },
    { "CODE_FUNC", func, -1 },
    { "MESSAGE", NULL, -1 },
  };
  gsize n_fields = 5;
  va_list args;

  if (log_domain != NULL) {
    fields[n_fields].key = "GLIB_DOMAIN";
    fields[n_fields].value = log_domain;
    fields[n_fields].length = -1;
    n_fields++;
  }

  if (object != NULL) {
    if (object_type != 0) {
      fields[n_fields].key = "WP_OBJECT_TYPE";
      fields[n_fields].value = &object_type;
      fields[n_fields].length = sizeof (GType);
      n_fields++;
    }

    fields[n_fields].key = "WP_OBJECT";
    fields[n_fields].value = &object;
    fields[n_fields].length = sizeof (gconstpointer);
    n_fields++;
  }

  va_start (args, message_format);
  fields[4].value = message = g_strdup_vprintf (message_format, args);
  va_end (args);

  g_log_structured_array (log_level, fields, n_fields);
}