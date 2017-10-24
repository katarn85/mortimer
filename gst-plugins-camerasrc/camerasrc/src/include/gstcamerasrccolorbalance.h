/*
 * camerasrc
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jeongmo Yang <jm80.yang@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef __GST_CAMERASRC_COLOR_BALANCE_H__
#define __GST_CAMERASRC_COLOR_BALANCE_H__

#include <gst/gst.h>
#include <gst/interfaces/colorbalance.h>
#include "gstcamerasrc.h"

G_BEGIN_DECLS

#define GST_TYPE_CAMERASRC_COLOR_BALANCE_CHANNEL (gst_camerasrc_color_balance_channel_get_type ())
#define GST_CAMERASRC_COLOR_BALANCE_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CAMERASRC_COLOR_BALANCE_CHANNEL, GstCameraSrcColorBalanceChannel))
#define GST_CAMERASRC_COLOR_BALANCE_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CAMERASRC_COLOR_BALANCE_CHANNEL, GstCameraSrcColorBalanceChannelClass))
#define GST_IS_CAMERASRC_COLOR_BALANCE_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CAMERASRC_COLOR_BALANCE_CHANNEL))
#define GST_IS_CAMERASRC_COLOR_BALANCE_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CAMERASRC_COLOR_BALANCE_CHANNEL))

typedef struct _GstCameraSrcColorBalanceChannel {
  GstColorBalanceChannel parent;

  guint32 id;
} GstCameraSrcColorBalanceChannel;

typedef struct _GstCameraSrcColorBalanceChannelClass {
  GstColorBalanceChannelClass parent;
} GstCameraSrcColorBalanceChannelClass;

GType gst_camerasrc_color_balance_channel_get_type( void );

const GList *gst_camerasrc_color_balance_list_channels( GstCameraSrc* camerasrc );
void gst_camerasrc_color_balance_set_value( GstCameraSrc* camerasrc, GstColorBalanceChannel* color_channel, gint value );
gint gst_camerasrc_color_balance_get_value( GstCameraSrc* camerasrc, GstColorBalanceChannel* color_channel );

#define GST_IMPLEMENT_CAMERASRC_COLOR_BALANCE_METHODS( Type, interface_as_function ) \
 \
static const GList* \
interface_as_function ## _color_balance_list_channels( GstColorBalance* balance ) \
{ \
	Type *this = (Type*) balance; \
	return gst_camerasrc_color_balance_list_channels( this ); \
} \
 \
static void \
interface_as_function ## _color_balance_set_value( GstColorBalance* balance, \
                                                 GstColorBalanceChannel* color_channel, \
                                                 gint value ) \
{ \
	Type *this = (Type*) balance; \
	return gst_camerasrc_color_balance_set_value( this, color_channel, value ); \
} \
 \
static gint \
interface_as_function ## _color_balance_get_value( GstColorBalance* balance, \
                                                 GstColorBalanceChannel* color_channel )\
{ \
	Type *this = (Type*) balance; \
	return gst_camerasrc_color_balance_get_value( this, color_channel ); \
} \
 \
void \
interface_as_function ## _color_balance_interface_init( GstColorBalanceClass* klass ) \
{ \
	GST_COLOR_BALANCE_TYPE( klass ) = GST_COLOR_BALANCE_HARDWARE; \
 \
	/* default virtual functions */ \
	klass->list_channels = interface_as_function ## _color_balance_list_channels; \
	klass->set_value = interface_as_function ## _color_balance_set_value; \
	klass->get_value = interface_as_function ## _color_balance_get_value; \
}

#endif /* __GST_CAMERASRC_COLOR_BALANCE_H__ */

