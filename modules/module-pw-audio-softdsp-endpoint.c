/* WirePlumber
 *
 * Copyright © 2019 Collabora Ltd.
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * module-pw-audio-softdsp-endpoint provides a WpEndpoint implementation
 * that wraps an audio device node in pipewire and plugs a DSP node, as well
 * as optional merger+volume nodes that are used as entry points for the
 * various streams that this endpoint may have
 */

#include <wp/wp.h>
#include <pipewire/pipewire.h>

struct _WpPwAudioSoftdspEndpoint
{
  WpEndpoint parent;
};

G_DECLARE_FINAL_TYPE (WpPwAudioSoftdspEndpoint, endpoint,
    WP_PW, AUDIO_SOFTDSP_ENDPOINT, WpEndpoint)

G_DEFINE_TYPE (WpPwAudioSoftdspEndpoint, endpoint, WP_TYPE_ENDPOINT)

static void
endpoint_init (WpPwAudioSoftdspEndpoint * self)
{
}

static gboolean
endpoint_prepare_link (WpEndpoint * self, guint32 stream_id,
    WpEndpointLink * link, GVariant ** properties, GError ** error)
{
  return TRUE;
}

static void
endpoint_class_init (WpPwAudioSoftdspEndpointClass * klass)
{
  WpEndpointClass *endpoint_class = (WpEndpointClass *) klass;

  endpoint_class->prepare_link = endpoint_prepare_link;
}

static gpointer
endpoint_factory (WpFactory * factory, GType type, GVariant * properties)
{
  const gchar *name = NULL;
  const gchar *media_class = NULL;

  if (type != WP_TYPE_ENDPOINT)
    return NULL;

  /* Get the name and media-class */
  if (!g_variant_lookup (properties, "name", "&s", &name))
      return NULL;
  if (!g_variant_lookup (properties, "media-class", "&s", &media_class))
      return NULL;

  return g_object_new (endpoint_get_type (),
      "name", name,
      "media-class", media_class,
      NULL);
}

void
wireplumber__module_init (WpModule * module, WpCore * core, GVariant * args)
{
  wp_factory_new (core, "pw-audio-softdsp-endpoint", endpoint_factory);
}