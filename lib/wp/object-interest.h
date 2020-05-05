/* WirePlumber
 *
 * Copyright © 2020 Collabora Ltd.
 *    @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __WIREPLUMBER_OBJECT_INTEREST_H__
#define __WIREPLUMBER_OBJECT_INTEREST_H__

#include <glib-object.h>
#include "defs.h"
#include "properties.h"

G_BEGIN_DECLS

/**
 * WpConstraintType:
 * @WP_CONSTRAINT_TYPE_NONE: invalid constraint type
 * @WP_CONSTRAINT_TYPE_PW_GLOBAL_PROPERTY: constraint applies
 *   to a PipeWire global property of the object (the ones returned by
 *   wp_proxy_get_global_properties())
 * @WP_CONSTRAINT_TYPE_PW_PROPERTY: constraint applies
 *   to a PipeWire property of the object (the ones returned by
 *   wp_proxy_get_properties())
 * @WP_CONSTRAINT_TYPE_G_PROPERTY: constraint applies to a #GObject
 *   property of the object
 */
typedef enum {
  WP_CONSTRAINT_TYPE_NONE = 0,
  WP_CONSTRAINT_TYPE_PW_GLOBAL_PROPERTY,
  WP_CONSTRAINT_TYPE_PW_PROPERTY,
  WP_CONSTRAINT_TYPE_G_PROPERTY,
} WpConstraintType;

/**
 * WpConstraintVerb:
 * @WP_CONSTRAINT_VERB_EQUALS: `=` the subject's value must equal the
 *   constraint's value
 * @WP_CONSTRAINT_VERB_IN_LIST: `c` the subject's value must equal at least
 *   one of the values in the list given as the constraint's value
 * @WP_CONSTRAINT_VERB_IN_RANGE: `~` the subject's value must be a number
 *   in the range defined by the constraint's value
 * @WP_CONSTRAINT_VERB_MATCHES: `#` the subject's value must match the
 *   pattern specified in the constraint's value
 * @WP_CONSTRAINT_VERB_IS_PRESENT: `+` the subject property must exist
 * @WP_CONSTRAINT_VERB_IS_ABSENT: `-` the subject property must not exist
 */
typedef enum {
  WP_CONSTRAINT_VERB_EQUALS = '=',
  WP_CONSTRAINT_VERB_IN_LIST = 'c',
  WP_CONSTRAINT_VERB_IN_RANGE = '~',
  WP_CONSTRAINT_VERB_MATCHES = '#',
  WP_CONSTRAINT_VERB_IS_PRESENT = '+',
  WP_CONSTRAINT_VERB_IS_ABSENT = '-',
} WpConstraintVerb;

/**
 * WP_TYPE_OBJECT_INTEREST:
 *
 * The #WpObjectInterest #GType
 */
#define WP_TYPE_OBJECT_INTEREST (wp_object_interest_get_type ())
WP_API
GType wp_object_interest_get_type (void) G_GNUC_CONST;

typedef struct _WpObjectInterest WpObjectInterest;

WP_API
WpObjectInterest * wp_object_interest_new (GType gtype, ...) G_GNUC_NULL_TERMINATED;

WP_API
WpObjectInterest * wp_object_interest_new_valist (GType gtype, va_list * args);

WP_API
WpObjectInterest * wp_object_interest_new_type (GType gtype);

WP_API
void wp_object_interest_add_constraint (WpObjectInterest * self,
    WpConstraintType type, const gchar * subject,
    WpConstraintVerb verb, GVariant * value);

WP_API
WpObjectInterest * wp_object_interest_copy (WpObjectInterest * self);

WP_API
void wp_object_interest_free (WpObjectInterest * self);

WP_API
gboolean wp_object_interest_validate (WpObjectInterest * self, GError ** error);

WP_API
gboolean wp_object_interest_matches (WpObjectInterest * self, gpointer object);

WP_API
gboolean wp_object_interest_matches_full (WpObjectInterest * self,
    GType object_type, gpointer object, WpProperties * pw_props,
    WpProperties * pw_global_props);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WpObjectInterest, wp_object_interest_free)

G_END_DECLS

#endif