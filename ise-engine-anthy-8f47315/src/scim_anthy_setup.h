/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2004 Hiroyuki Ikezoe
 *  Copyright (C) 2004-2005 Takuro Ashie
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __SCIM_ANTHY_SETUP_H__
#define __SCIM_ANTHY_SETUP_H__

#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#define SCIM_ANTHY_USE_GTK
#include <scim.h>
#include "scim_anthy_style_file.h"

namespace scim_anthy {

GtkWidget *create_check_button      (const char *config_key);
GtkWidget *create_spin_button       (const char *config_key,
                                     GtkTable   *table,
                                     int         idx);
GtkWidget *create_entry             (const char *config_key,
                                     GtkTable   *table,
                                     int         idx);
GtkWidget *create_combo             (const char *config_key,
                                     gpointer    candidates_p,
                                     GtkTable   *table,
                                     gint        idx);
GtkWidget *create_option_menu       (const char *config_key,
                                     gpointer    candidates_p,
                                     GtkTable   *table,
                                     gint        idx);
GtkWidget *create_color_button      (const char *config_key);
GtkWidget *create_key_select_button (const char *config_key,
                                     GtkTable   *table,
                                     int         idx);

extern StyleFiles __style_list;
extern StyleFile  __user_style_file;
extern bool       __config_changed;
extern bool       __style_changed;

};

#endif /* __SCIM_ANTHY_SETUP_H__ */
