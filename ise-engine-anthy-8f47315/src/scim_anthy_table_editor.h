/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#ifndef __SCIM_ANTHY_TABLE_EDITOR_H__
#define __SCIM_ANTHY_TABLE_EDITOR_H__

#include <gtk/gtk.h>

#define SCIM_ANTHY_TYPE_TABLE_EDITOR            (scim_anthy_table_editor_get_type ())
#define SCIM_ANTHY_TABLE_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCIM_ANTHY_TYPE_TABLE_EDITOR, ScimAnthyTableEditor))
#define SCIM_ANTHY_TABLE_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SCIM_ANTHY_TYPE_TABLE_EDITOR, ScimAnthyTableEditorClass))
#define SCIM_ANTHY_IS_TABLE_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCIM_ANTHY_TYPE_TABLE_EDITOR))
#define SCIM_ANTHY_IS_TABLE_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SCIM_ANTHY_TYPE_TABLE_EDITOR))
#define SCIM_ANTHY_TABLE_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SCIM_ANTHY_TYPE_TABLE_EDITOR, ScimAnthyTableEditorClass))


typedef struct _ScimAnthyTableEditorClass ScimAnthyTableEditorClass;
typedef struct _ScimAnthyTableEditor      ScimAnthyTableEditor;

struct _ScimAnthyTableEditor
{
    GtkDialog  parent_instance;

    GtkWidget *treeview;

    GtkWidget *button_area;

    GtkWidget *add_button;
    GtkWidget *remove_button;

    GList     *entries;
};

struct _ScimAnthyTableEditorClass
{
    GtkDialogClass parent_class;

    /* -- signals -- */
    void (*add_entry)    (ScimAnthyTableEditor *editor);
    void (*remove_entry) (ScimAnthyTableEditor *editor);
};


GType       scim_anthy_table_editor_get_type     (void) G_GNUC_CONST;
GtkWidget  *scim_anthy_table_editor_new          (void);
const char *scim_anthy_table_editor_get_nth_text (ScimAnthyTableEditor *editor,
                                                  guint                 nth);
void        scim_anthy_table_editor_set_columns  (ScimAnthyTableEditor *editor,
                                                  const char          **titles);

#endif /* __SCIM_ANTHY_TABLE_EDITOR_H__ */
