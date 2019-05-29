/* WirePlumber
 *
 * Copyright © 2019 Collabora Ltd.
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __WIREPLUMBER_ENDPOINT_H__
#define __WIREPLUMBER_ENDPOINT_H__

#include "core.h"

G_BEGIN_DECLS

#define WP_TYPE_ENDPOINT (wp_endpoint_get_type ())
G_DECLARE_DERIVABLE_TYPE (WpEndpoint, wp_endpoint, WP, ENDPOINT, GObject)

#define WP_TYPE_ENDPOINT_LINK (wp_endpoint_link_get_type ())
G_DECLARE_DERIVABLE_TYPE (WpEndpointLink, wp_endpoint_link, WP, ENDPOINT_LINK, GObject)

struct _WpEndpointClass
{
  GObjectClass parent_class;

  GVariant * (*get_control_value) (WpEndpoint * self, guint32 control_id);
  gboolean (*set_control_value) (WpEndpoint * self, guint32 control_id,
      GVariant * value);

  gboolean (*prepare_link) (WpEndpoint * self, guint32 stream_id,
      WpEndpointLink * link, GVariant ** properties, GError ** error);
  void (*release_link) (WpEndpoint * self, WpEndpointLink * link);

  const gchar * (*get_endpoint_link_factory) (WpEndpoint * self);
};

void wp_endpoint_register (WpEndpoint * self, WpCore * core);
void wp_endpoint_unregister (WpEndpoint * self);
GPtrArray * wp_endpoint_find (WpCore * core, const gchar * media_class_lookup);

const gchar * wp_endpoint_get_name (WpEndpoint * self);
const gchar * wp_endpoint_get_media_class (WpEndpoint * self);

void wp_endpoint_register_stream (WpEndpoint * self, GVariant * stream);
GVariant * wp_endpoint_list_streams (WpEndpoint * self);

void wp_endpoint_register_control (WpEndpoint * self, GVariant * control);
GVariant * wp_endpoint_list_controls (WpEndpoint * self);

GVariant * wp_endpoint_get_control_value (WpEndpoint * self,
    guint32 control_id);
gboolean wp_endpoint_set_control_value (WpEndpoint * self, guint32 control_id,
    GVariant * value);
void wp_endpoint_notify_control_value (WpEndpoint * self, guint32 control_id);

gboolean wp_endpoint_is_linked (WpEndpoint * self);
GPtrArray * wp_endpoint_get_links (WpEndpoint * self);

struct _WpEndpointLinkClass
{
  GObjectClass parent_class;

  gboolean (*create) (WpEndpointLink * self, GVariant * src_data,
      GVariant * sink_data, GError ** error);
  void (*destroy) (WpEndpointLink * self);
};

void wp_endpoint_link_set_endpoints (WpEndpointLink * self, WpEndpoint * src,
    guint32 src_stream, WpEndpoint * sink, guint32 sink_stream);

WpEndpoint * wp_endpoint_link_get_source_endpoint (WpEndpointLink * self);
guint32 wp_endpoint_link_get_source_stream (WpEndpointLink * self);
WpEndpoint * wp_endpoint_link_get_sink_endpoint (WpEndpointLink * self);
guint32 wp_endpoint_link_get_sink_stream (WpEndpointLink * self);

WpEndpointLink * wp_endpoint_link_new (WpCore * core, WpEndpoint * src,
    guint32 src_stream, WpEndpoint * sink, guint32 sink_stream,
    GError ** error);
void wp_endpoint_link_destroy (WpEndpointLink * self);

G_END_DECLS

#endif