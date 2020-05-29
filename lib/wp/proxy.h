/* WirePlumber
 *
 * Copyright © 2019 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __WIREPLUMBER_PROXY_H__
#define __WIREPLUMBER_PROXY_H__

#include <gio/gio.h>

#include "spa-pod.h"
#include "properties.h"

G_BEGIN_DECLS

struct pw_proxy;
struct spa_param_info;
typedef struct _WpCore WpCore;

/**
 * WpProxyFeatures:
 *
 * Flags that specify functionality that is available on this class.
 * Use wp_proxy_augment() to enable more features and wp_proxy_get_features()
 * to find out which features are already enabled.
 *
 * Subclasses may also specify additional features that can be ORed with these
 * ones and they can also be enabled with wp_proxy_augment().
 */
typedef enum { /*< flags >*/
  /* standard features */
  WP_PROXY_FEATURE_PW_PROXY     = (1 << 0),
  WP_PROXY_FEATURE_INFO         = (1 << 1),
  WP_PROXY_FEATURE_BOUND        = (1 << 2),

  /* param caching features */
  WP_PROXY_FEATURE_PROPS        = (1 << 3),

  WP_PROXY_FEATURE_LAST         = (1 << 16), /*< skip >*/
} WpProxyFeatures;

/**
 * WP_PROXY_FEATURES_STANDARD:
 *
 * A constant set of features that contains the standard features that are
 * available in the #WpProxy class. The standard features are usually all
 * enabled at once, even if not requested explicitly. It is a good practice,
 * though, to enable only the features that you actually need. This leaves
 * room for optimizations in the #WpProxy class.
 */
#define WP_PROXY_FEATURES_STANDARD \
    (WP_PROXY_FEATURE_PW_PROXY | WP_PROXY_FEATURE_INFO | WP_PROXY_FEATURE_BOUND)

/**
 * WP_TYPE_PROXY:
 *
 * The #WpProxy #GType
 */
#define WP_TYPE_PROXY (wp_proxy_get_type ())
WP_API
G_DECLARE_DERIVABLE_TYPE (WpProxy, wp_proxy, WP, PROXY, GObject)

/**
 * WpProxyClass:
 * @pw_iface_type: the PipeWire type of the interface that is being proxied by
 *    this class (ex. `PW_TYPE_INTERFACE_Node` for #WpNode)
 * @pw_iface_version: the PipeWire version of the interface that is being
 *    proxied by this class
 */
struct _WpProxyClass
{
  GObjectClass parent_class;

  const gchar * pw_iface_type;
  guint32 pw_iface_version;

  void (*augment) (WpProxy *self, WpProxyFeatures features);

  gconstpointer (*get_info) (WpProxy * self);
  WpProperties * (*get_properties) (WpProxy * self);
  struct spa_param_info * (*get_param_info) (WpProxy * self, guint * n_params);

  gint (*enum_params) (WpProxy * self, guint32 id, guint32 start, guint32 num,
      const WpSpaPod * filter);
  gint (*subscribe_params) (WpProxy * self, guint32 *ids, guint32 n_ids);
  gint (*set_param) (WpProxy * self, guint32 id, guint32 flags,
      const WpSpaPod * param);

  /* signals */

  void (*pw_proxy_created) (WpProxy * self, struct pw_proxy * proxy);
  void (*pw_proxy_destroyed) (WpProxy * self);
  void (*bound) (WpProxy * self, guint32 id);
  void (*prop_changed) (WpProxy * self, const gchar * prop_name);
};

WP_API
void wp_proxy_request_destroy (WpProxy * self);

/* features API */

WP_API
void wp_proxy_augment (WpProxy *self,
    WpProxyFeatures wanted_features, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);

WP_API
gboolean wp_proxy_augment_finish (WpProxy * self, GAsyncResult * res,
    GError ** error);

WP_API
WpProxyFeatures wp_proxy_get_features (WpProxy * self);

/* the owner core */

WP_API
WpCore * wp_proxy_get_core (WpProxy * self);

/* global object API */

WP_API
guint32 wp_proxy_get_global_permissions (WpProxy * self);

WP_API
WpProperties * wp_proxy_get_global_properties (WpProxy * self);

/* native pw_proxy object getter (requires FEATURE_PW_PROXY) */

WP_API
struct pw_proxy * wp_proxy_get_pw_proxy (WpProxy * self);

/* native info structure + wrappers (requires FEATURE_INFO) */

WP_API
gconstpointer wp_proxy_get_info (WpProxy * self);

WP_API
WpProperties * wp_proxy_get_properties (WpProxy * self);

WP_API
const gchar * wp_proxy_get_property (WpProxy * self, const gchar * key);

WP_API
GVariant * wp_proxy_get_param_info (WpProxy * self);

/* the bound id (aka global id, requires FEATURE_BOUND) */

WP_API
guint32 wp_proxy_get_bound_id (WpProxy * self);

/* params API */

WP_API
void wp_proxy_enum_params (WpProxy * self, const gchar * id,
    const WpSpaPod *filter, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);

WP_API
WpIterator * wp_proxy_enum_params_finish (WpProxy * self, GAsyncResult * res,
    GError ** error);

WP_API
void wp_proxy_set_param (WpProxy * self, const gchar * id,
    const WpSpaPod * param);

/* PARAM_PropInfo - PARAM_Props */

WP_API
WpIterator * wp_proxy_iterate_prop_info (WpProxy * self);

WP_API
WpSpaPod * wp_proxy_get_prop (WpProxy * self, const gchar * prop_name);

WP_API
void wp_proxy_set_prop (WpProxy * self, const gchar * prop_name,
    WpSpaPod * value);

G_END_DECLS

#endif
