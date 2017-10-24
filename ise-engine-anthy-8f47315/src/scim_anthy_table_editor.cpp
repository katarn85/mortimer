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

#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#include "scim_anthy_table_editor.h"

#include <string.h>
#include <ctype.h>
#include "scim_anthy_intl.h"

enum {
    ADD_ENTRY_SIGNAL,
    REMOVE_ENTRY_SIGNAL,
    LAST_SIGNAL,
};

static void scim_anthy_table_editor_class_init   (ScimAnthyTableEditorClass *klass);
static void scim_anthy_table_editor_init         (ScimAnthyTableEditor      *object);
static void scim_anthy_table_editor_dispose      (GObject                   *object);

static void scim_anthy_table_editor_add_entry    (ScimAnthyTableEditor *editor);
static void scim_anthy_table_editor_remove_entry (ScimAnthyTableEditor *editor);

static gint compare_string                  (GtkTreeModel     *model,
                                             GtkTreeIter      *a,
                                             GtkTreeIter      *b,
                                             gpointer          user_data);

static void on_table_view_selection_changed (GtkTreeSelection *selection,
                                             gpointer          data);
static void on_add_button_clicked           (GtkButton        *button,
                                             gpointer          data);
static void on_remove_button_clicked        (GtkButton        *button,
                                             gpointer          data);
static void on_entry_activate               (GtkEntry         *entry,
                                             gpointer          data);
static void on_entry_changed                (GtkEditable      *editable,
                                             gpointer          data);
static void on_sequence_entry_insert_text   (GtkEditable      *editable,
                                             const gchar      *text,
                                             gint              length,
                                             gint             *position,
                                             gpointer          data);

static guint editor_signals[LAST_SIGNAL] = { 0 };
static GtkDialogClass *parent_class = NULL;

GType
scim_anthy_table_editor_get_type (void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo info = {
            sizeof (ScimAnthyTableEditorClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            (GClassInitFunc) scim_anthy_table_editor_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof (ScimAnthyTableEditor),
            0,              /* n_preallocs */
            (GInstanceInitFunc) scim_anthy_table_editor_init,
        };

        type = g_type_register_static (GTK_TYPE_DIALOG,
                                       "ScimAnthyTableEditor",
                                       &info, (GTypeFlags) 0);
    }

    return type;
}

static void
scim_anthy_table_editor_class_init (ScimAnthyTableEditorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    //GtkWidgetClass *widget_class     = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkDialogClass *) g_type_class_peek_parent (klass);

    editor_signals[ADD_ENTRY_SIGNAL] =
      g_signal_new ("add-entry",
  		  G_TYPE_FROM_CLASS (klass),
  		  G_SIGNAL_RUN_LAST,
  		  G_STRUCT_OFFSET (ScimAnthyTableEditorClass, add_entry),
  		  NULL, NULL,
  		  g_cclosure_marshal_VOID__VOID,
  		  G_TYPE_NONE, 0);
    editor_signals[REMOVE_ENTRY_SIGNAL] =
      g_signal_new ("remove-entry",
  		  G_TYPE_FROM_CLASS (klass),
  		  G_SIGNAL_RUN_LAST,
  		  G_STRUCT_OFFSET (ScimAnthyTableEditorClass, remove_entry),
  		  NULL, NULL,
  		  g_cclosure_marshal_VOID__VOID,
  		  G_TYPE_NONE, 0);

    gobject_class->dispose = scim_anthy_table_editor_dispose;
    klass->add_entry       = scim_anthy_table_editor_add_entry;
    klass->remove_entry    = scim_anthy_table_editor_remove_entry;
}

static void
scim_anthy_table_editor_init (ScimAnthyTableEditor *editor)
{
    gtk_dialog_add_buttons (GTK_DIALOG (editor),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                            NULL);

    gtk_window_set_default_size (GTK_WINDOW (editor), 350, 250);
    gtk_window_set_position (GTK_WINDOW (editor),
                             GTK_WIN_POS_CENTER_ON_PARENT);

    // edit area
    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (editor)->vbox), hbox,
                        TRUE, TRUE, 0);
    gtk_widget_show (hbox);

    // tree view area
    GtkWidget *scrwin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrwin),
                                         GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrwin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (hbox), scrwin, TRUE, TRUE, 0);
    gtk_widget_show (scrwin);

    GtkWidget *treeview = gtk_tree_view_new ();
    editor->treeview = treeview;
    gtk_container_add (GTK_CONTAINER (scrwin), treeview);
    gtk_widget_show (treeview);

    GtkTreeSelection *selection;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    g_signal_connect (G_OBJECT (selection), "changed",
                      G_CALLBACK (on_table_view_selection_changed), editor);

    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
    gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (treeview), TRUE);

    // button area
    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
    editor->button_area = vbox;
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 5);
    gtk_widget_show (vbox);

    editor->entries = NULL;
}

static void
scim_anthy_table_editor_dispose (GObject *object)
{
    ScimAnthyTableEditor *editor = SCIM_ANTHY_TABLE_EDITOR (object);

    if (editor->entries) {
        g_list_free (editor->entries);
        editor->entries = NULL;
    }

	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
}

GtkWidget *
scim_anthy_table_editor_new (void)
{
    return GTK_WIDGET(g_object_new (SCIM_ANTHY_TYPE_TABLE_EDITOR,
                                    NULL));
}

void
scim_anthy_table_editor_set_columns (ScimAnthyTableEditor *editor,
                                     const char          **titles)
{
    g_return_if_fail (SCIM_ANTHY_IS_TABLE_EDITOR (editor));

    if (!titles)
        return;

    gint n_cols;
    for (n_cols = 0; titles[n_cols]; n_cols++);
    if (n_cols <= 0)
        return;

    GType types[n_cols];
    for (gint i = 0; i < n_cols; i++)
        types[i] = G_TYPE_STRING;

    GtkListStore *store = gtk_list_store_newv (n_cols, types);
    gtk_tree_view_set_model (GTK_TREE_VIEW (editor->treeview),
                             GTK_TREE_MODEL (store));

    // columns
    for (int i = 0; i < n_cols; i++) {
        GtkCellRenderer *cell;
        GtkTreeViewColumn *column;
        cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (titles[i], cell,
                                                           "text", i,
                                                           NULL);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_fixed_width (column, 80);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(editor->treeview), column);

        gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store), i,
                                         compare_string,
                                         GINT_TO_POINTER (i), NULL);
        gtk_tree_view_column_set_sort_column_id (column, i);
    }

    // entries
    for (int i = 0; i < n_cols; i++) {
        GtkWidget *label = gtk_label_new_with_mnemonic (titles[i]);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (editor->button_area), label, FALSE, FALSE, 2);
        gtk_widget_show (label);

        GtkWidget *entry = gtk_entry_new ();
        gtk_box_pack_start (GTK_BOX (editor->button_area), entry,
                            FALSE, FALSE, 2);
        gtk_widget_set_size_request (entry, 80, -1);
        g_signal_connect (G_OBJECT (entry), "activate",
                          G_CALLBACK (on_entry_activate), editor);
        g_signal_connect (G_OBJECT (entry), "changed",
                          G_CALLBACK (on_entry_changed), editor);
        if (i == 0)
            g_signal_connect (G_OBJECT (entry), "insert-text",
                              G_CALLBACK (on_sequence_entry_insert_text),
                              editor);
        gtk_widget_show (entry);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

        editor->entries = g_list_append (editor->entries, entry);
    }

    // buttons
    GtkWidget *button = gtk_button_new_from_stock (GTK_STOCK_ADD);
    editor->add_button = button;
    gtk_box_pack_start (GTK_BOX (editor->button_area), button, FALSE, FALSE, 5);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_add_button_clicked), editor);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_widget_show (button);

    button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    editor->remove_button = button;
    gtk_box_pack_start (GTK_BOX (editor->button_area), button, FALSE, FALSE, 5);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_remove_button_clicked), editor);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_widget_show (button);

    // clean
    g_object_unref (store);
}

const char *
scim_anthy_table_editor_get_nth_text (ScimAnthyTableEditor *editor, guint nth)
{
    g_return_val_if_fail (SCIM_ANTHY_IS_TABLE_EDITOR (editor), "");

    GtkEntry *entry = GTK_ENTRY (g_list_nth_data (editor->entries, nth));
    if (!entry)
        return "";

    return gtk_entry_get_text (entry);
}

static void
scim_anthy_table_editor_add_entry (ScimAnthyTableEditor *editor)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (editor->treeview);
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkTreeIter iter;
 
    gboolean go_next;
    bool found = false;

    const gchar *sequence;
    sequence = scim_anthy_table_editor_get_nth_text (editor, 0);

    if (!sequence)
        return;

    for (go_next = gtk_tree_model_get_iter_first (model, &iter);
         go_next;
         go_next = gtk_tree_model_iter_next (model, &iter))
    {
        gchar *seq = NULL;
        gtk_tree_model_get (model, &iter,
                            0, &seq,
                            -1);
        if (seq && !strcmp (sequence, seq)) {
            found = true;
            g_free (seq);
            break;
        }
        g_free (seq);
    }

    if (!found)
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);

    GList *node;
    gint i;
    for (i = 0, node = editor->entries;
         node;
         i++, node = g_list_next (node))
    {
        const char *text = gtk_entry_get_text (GTK_ENTRY (node->data));
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            i, text,
                            -1);
    }

    GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
    gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
    gtk_tree_path_free (path);
}

static void
scim_anthy_table_editor_remove_entry (ScimAnthyTableEditor *editor)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (editor->treeview);
    GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
    GtkTreeModel *model = NULL;
    GtkTreeIter iter, next;
    gboolean selected, success;

    selected = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (!selected)
        return;

    gchar *sequence = NULL;
    gtk_tree_model_get (model, &iter,
                        0, &sequence,
                        -1);

    next = iter;
    success = gtk_tree_model_iter_next (model, &next);
    GtkTreePath *path = NULL;
    if (success) {
        path = gtk_tree_model_get_path (model, &next);
    } else {
        path = gtk_tree_model_get_path (model, &iter);
        if (path)
            success = gtk_tree_path_prev (path);
    }

    if (success && path)
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
    if (path)
        gtk_tree_path_free (path);

    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

    g_free (sequence);
}

static gint
compare_string (GtkTreeModel *model,
                GtkTreeIter *a,
                GtkTreeIter *b,
                gpointer user_data)
{
    gint n_cols, column, cur_column = GPOINTER_TO_INT (user_data);
    gint ret = 0;

    n_cols = gtk_tree_model_get_n_columns (model);

    if (cur_column < n_cols) {
        gchar *seq1 = NULL, *seq2 = NULL;
        gtk_tree_model_get (model, a,
                            cur_column, &seq1,
                            -1);
        gtk_tree_model_get (model, b,
                            cur_column, &seq2,
                            -1);
        if (!seq1 && seq2) {
            ret = -1;
        } else if (seq1 && !seq2) {
            ret = 1;
        } else if (seq1 && seq2) {
            ret = strcmp (seq1, seq2);
        } else {
            ret = 0;
        }
        g_free (seq1);
        g_free (seq2);
    }

    for (column = 0; ret == 0 && column < n_cols; column++) {
        gchar *seq1 = NULL, *seq2 = NULL;

        if (cur_column == column)
            continue;

        gtk_tree_model_get (model, a,
                            column, &seq1,
                            -1);
        gtk_tree_model_get (model, b,
                            column, &seq2,
                            -1);
        if (!seq1 && seq2) {
            ret = -1;
        } else if (seq1 && !seq2) {
            ret = 1;
        } else if (!seq1 && !seq2) {
            ret = strcmp (seq1, seq2);
        } else {
            ret = 0;
        }
        g_free (seq1);
        g_free (seq2);
    }

    return ret;
}

static void
on_table_view_selection_changed (GtkTreeSelection *selection, gpointer data)
{
    ScimAnthyTableEditor *editor = SCIM_ANTHY_TABLE_EDITOR (data);
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;

    gboolean selected;

    selected = gtk_tree_selection_get_selected (selection, &model, &iter);

    if (editor->remove_button) {
        if (selected) {
            gtk_widget_set_sensitive (editor->remove_button, true);
        } else {
            gtk_widget_set_sensitive (editor->remove_button, false);
        }
    }

    GList *node;

    if (selected) {
        gint i;
        for (i = 0, node = editor->entries;
             node;
             i++, node = g_list_next (node))
        {
            gchar *str = NULL;
            gtk_tree_model_get (model, &iter,
                                i, &str,
                                -1);
            gtk_entry_set_text (GTK_ENTRY (node->data), str);
            g_free (str);
        }
    } else {
        for (node = editor->entries; node; node = g_list_next (node))
            gtk_entry_set_text (GTK_ENTRY (node->data), "");
    }
}

static void
on_add_button_clicked (GtkButton *button, gpointer data)
{
    ScimAnthyTableEditor *editor = SCIM_ANTHY_TABLE_EDITOR (data);
    g_signal_emit (editor, editor_signals[ADD_ENTRY_SIGNAL], 0);
}

static void
on_remove_button_clicked (GtkButton *button, gpointer data)
{
    ScimAnthyTableEditor *editor = SCIM_ANTHY_TABLE_EDITOR (data);
    g_signal_emit (editor, editor_signals[REMOVE_ENTRY_SIGNAL], 0);
}

static void
on_entry_activate (GtkEntry *entry, gpointer data)
{
    ScimAnthyTableEditor *editor = SCIM_ANTHY_TABLE_EDITOR (data);
    g_signal_emit (editor, editor_signals[ADD_ENTRY_SIGNAL], 0);
}

static void
on_entry_changed (GtkEditable *editable, gpointer data)
{
    ScimAnthyTableEditor *editor = SCIM_ANTHY_TABLE_EDITOR (data);
    const char *seq;

    if (!editor->entries || !editor->entries->data)
        return;

    seq = gtk_entry_get_text (GTK_ENTRY (editor->entries->data));
    gtk_widget_set_sensitive (editor->add_button,
                              seq && *seq);
}

static void
on_sequence_entry_insert_text (GtkEditable *editable,
                               const gchar *text,
                               gint length,
                               gint *position,
                               gpointer data)
{
    for (int i = 0; i < length; i++) {
       if (!isascii (text[i]) || isspace (text[i])) {
            g_signal_stop_emission_by_name (editable, "insert_text");
            return;
        }
    }
}
