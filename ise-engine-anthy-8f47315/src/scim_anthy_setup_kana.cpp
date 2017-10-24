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

#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#define Uses_SCIM_CONFIG_BASE
#define Uses_SCIM_EVENT
#define SCIM_ANTHY_USE_GTK
#include <scim.h>
//#include "scim_anthy_setup_kana.h"
#include "scim_anthy_intl.h"
#include "scim_anthy_style_file.h"
#include "scim_anthy_prefs.h"
#include "scim_anthy_default_tables.h"
#include "scim_anthy_setup.h"
#include "scim_anthy_table_editor.h"

using namespace scim;

namespace scim_anthy {

#define INDEX_KEY "scim-anthy::Index"
static const char * const __kana_fund_table   = "KanaTable/FundamentalTable";
static const char * const __nicola_fund_table = "NICOLATable/FundamentalTable";

static const int KANA_LAYOUT_INDEX_USER_DEFINED = 0;
static const int KANA_LAYOUT_INDEX_DEFAULT      = 1;

static const int NICOLA_LAYOUT_INDEX_USER_DEFINED = 0;
static const int NICOLA_LAYOUT_INDEX_DEFAULT      = 1;

// Internal data declaration.
static GtkWidget   * __widget_kana_layout_menu     = NULL;
static GtkWidget   * __widget_kana_layout_menu2    = NULL;
static GtkWidget   * __widget_nicola_layout_menu   = NULL;
static GtkWidget   * __widget_nicola_layout_menu2  = NULL;

static String __config_kana_layout_file   = SCIM_ANTHY_CONFIG_KANA_LAYOUT_FILE_DEFAULT;
static String __config_nicola_layout_file = SCIM_ANTHY_CONFIG_NICOLA_LAYOUT_FILE_DEFAULT;


static GtkWidget *create_kana_window                 (GtkWindow            *parent);
static GtkWidget *create_nicola_window               (GtkWindow            *parent);

static void     setup_kana_page                      (void);
static void     setup_kana_layout_menu               (GtkOptionMenu        *omenu);
static void     setup_nicola_layout_menu             (GtkOptionMenu        *omenu);
static void     setup_kana_window_value              (ScimAnthyTableEditor *editor);
static void     setup_nicola_window_value            (ScimAnthyTableEditor *editor);

static bool     load_kana_layout                     (void);
static bool     load_nicola_layout                   (void);

static void     on_kana_layout_menu_changed          (GtkOptionMenu        *omenu,
                                                      gpointer              user_data);
static void     on_kana_customize_button_clicked     (GtkWidget            *button,
                                                      gpointer              data);
static void     on_kana_table_editor_add_entry       (ScimAnthyTableEditor *editor,
                                                      gpointer              data);
static void     on_kana_table_editor_added_entry     (ScimAnthyTableEditor *editor,
                                                      gpointer              data);
static void     on_kana_table_editor_remove_entry    (ScimAnthyTableEditor *editor,
                                                      gpointer              data);
static void     on_kana_table_editor_removed_entry   (ScimAnthyTableEditor *editor,
                                                      gpointer              data);
static void     on_nicola_layout_menu_changed        (GtkOptionMenu        *omenu,
                                                      gpointer              user_data);
static void     on_nicola_customize_button_clicked   (GtkWidget            *button,
                                                      gpointer              data);
static void     on_nicola_table_editor_add_entry     (ScimAnthyTableEditor *editor,
                                                      gpointer              data);
static void     on_nicola_table_editor_added_entry   (ScimAnthyTableEditor *editor,
                                                      gpointer              data);
static void     on_nicola_table_editor_remove_entry  (ScimAnthyTableEditor *editor,
                                                      gpointer              data);
static void     on_nicola_table_editor_removed_entry (ScimAnthyTableEditor *editor,
                                                      gpointer              data);

GtkWidget *
kana_page_create_ui (void)
{
    GtkWidget *vbox;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    // JIS Kana Layout
    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
    gtk_widget_show (hbox);

    GtkWidget *label = gtk_label_new (_("<b>JIS Kana Layout</b>"));
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
    gtk_widget_show (label);

    GtkWidget *alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 8, 24, 0);
    gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
    gtk_widget_show (alignment);

    /* kana table */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_container_add (GTK_CONTAINER (alignment), hbox);
    gtk_widget_show(hbox);

    label = gtk_label_new_with_mnemonic (_("La_yout:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
    gtk_widget_show (label);

    GtkWidget *omenu = gtk_option_menu_new ();
    __widget_kana_layout_menu = omenu;
    g_signal_connect (G_OBJECT (omenu), "changed",
                      G_CALLBACK (on_kana_layout_menu_changed), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 2);
    gtk_widget_show (omenu);

    gtk_label_set_mnemonic_widget (GTK_LABEL(label), omenu);

    GtkWidget *button = gtk_button_new_with_mnemonic (_("_Customize..."));
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_kana_customize_button_clicked), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);


    // Thumb Shift Layout
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
    gtk_widget_show (hbox);

    label = gtk_label_new (_("<b>Thumb Shift Layout</b>"));
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
    gtk_widget_show (label);

    alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 24, 0);
    gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
    gtk_widget_show (alignment);

    GtkWidget *vbox2 = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (alignment), vbox2);
    gtk_widget_show (vbox2);

    /* nicola table */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    label = gtk_label_new_with_mnemonic (_("La_yout:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
    gtk_widget_show (label);

    omenu = gtk_option_menu_new ();
    __widget_nicola_layout_menu = omenu;
    g_signal_connect (G_OBJECT (omenu), "changed",
                      G_CALLBACK (on_nicola_layout_menu_changed), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 2);
    gtk_widget_show (omenu);

    gtk_label_set_mnemonic_widget (GTK_LABEL(label), omenu);

    button = gtk_button_new_with_mnemonic (_("_Customize..."));
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_nicola_customize_button_clicked), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);

    /* thumb shift keys */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    GtkWidget *table = gtk_table_new (2, 2, FALSE);
    gtk_box_pack_start (GTK_BOX (vbox2), table, FALSE, FALSE, 4);
    gtk_widget_show (table);

    // left
    GtkWidget *widget = create_entry (SCIM_ANTHY_CONFIG_LEFT_THUMB_SHIFT_KEY,
                                      GTK_TABLE (table), 0);
    gtk_entry_set_editable (GTK_ENTRY (widget), FALSE);
    widget = create_key_select_button (SCIM_ANTHY_CONFIG_LEFT_THUMB_SHIFT_KEY,
                                       GTK_TABLE (table), 0);

    // right
    widget = create_entry (SCIM_ANTHY_CONFIG_RIGHT_THUMB_SHIFT_KEY,
                           GTK_TABLE (table), 1);
    gtk_entry_set_editable (GTK_ENTRY (widget), FALSE);
    widget = create_key_select_button (SCIM_ANTHY_CONFIG_RIGHT_THUMB_SHIFT_KEY,
                                       GTK_TABLE (table), 1);

    /* NICOLA time */
    create_spin_button (SCIM_ANTHY_CONFIG_NICOLA_TIME,
                        GTK_TABLE (table), 3);

    // prepare
    setup_kana_page ();

    return vbox;
}

void
kana_page_load_config (const ConfigPointer &config)
{
    __config_kana_layout_file
        = config->read (String (SCIM_ANTHY_CONFIG_KANA_LAYOUT_FILE),
                        String (SCIM_ANTHY_CONFIG_KANA_LAYOUT_FILE_DEFAULT));
    __config_nicola_layout_file
        = config->read (String (SCIM_ANTHY_CONFIG_NICOLA_LAYOUT_FILE),
                        String (SCIM_ANTHY_CONFIG_NICOLA_LAYOUT_FILE_DEFAULT));
    setup_kana_page ();
}

void
kana_page_save_config (const ConfigPointer &config)
{
    __config_kana_layout_file
        = config->write (String (SCIM_ANTHY_CONFIG_KANA_LAYOUT_FILE),
                         String (__config_kana_layout_file));
    __config_nicola_layout_file
        = config->write (String (SCIM_ANTHY_CONFIG_NICOLA_LAYOUT_FILE),
                         String (__config_nicola_layout_file));
}

bool
kana_page_query_changed (void)
{
    return __config_changed || __style_changed;
}


static GtkWidget *
create_kana_window (GtkWindow *parent)
{
    GtkWidget *dialog = scim_anthy_table_editor_new ();
    const char *titles[3];
    titles[0] = _("Key");
    titles[1] = _("Result");
    titles[2] = NULL;
    scim_anthy_table_editor_set_columns (SCIM_ANTHY_TABLE_EDITOR (dialog),
                                         titles);
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (parent));
    gtk_window_set_title (GTK_WINDOW (dialog),
                          _("Customize kana layout table"));

    // option menu area
    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
                        FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, 0);
    gtk_widget_show(hbox);

    GtkWidget *label = gtk_label_new_with_mnemonic (_("Layout _table:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
    gtk_widget_show (label);

    GtkWidget *omenu = gtk_option_menu_new ();
    __widget_kana_layout_menu2 = omenu;
    g_object_add_weak_pointer (G_OBJECT (omenu),
                               (gpointer*) &__widget_kana_layout_menu2);
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 2);
    setup_kana_layout_menu (GTK_OPTION_MENU (omenu));
    gtk_option_menu_set_history
        (GTK_OPTION_MENU (omenu),
         gtk_option_menu_get_history (
             GTK_OPTION_MENU (__widget_kana_layout_menu)));
    gtk_widget_show (omenu);

    gtk_label_set_mnemonic_widget (GTK_LABEL(label), omenu);

#if 0
    GtkWidget *button = gtk_button_new_with_mnemonic ("Save _as...");
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);
#endif

    // set data and connect signals
    setup_kana_window_value (SCIM_ANTHY_TABLE_EDITOR (dialog));
    g_signal_connect (G_OBJECT (omenu), "changed",
                      G_CALLBACK (on_kana_layout_menu_changed),
                      dialog);
    g_signal_connect (G_OBJECT (dialog), "add-entry",
                      G_CALLBACK (on_kana_table_editor_add_entry),
                      NULL);
    g_signal_connect (G_OBJECT (dialog), "remove-entry",
                      G_CALLBACK (on_kana_table_editor_remove_entry),
                      NULL);
    g_signal_connect_after (G_OBJECT (dialog), "add-entry",
                            G_CALLBACK (on_kana_table_editor_added_entry),
                            NULL);
    g_signal_connect_after (G_OBJECT (dialog), "remove-entry",
                            G_CALLBACK (on_kana_table_editor_removed_entry),
                            NULL);

    return dialog;
}


static GtkWidget *
create_nicola_window (GtkWindow *parent)
{
    GtkWidget *dialog = scim_anthy_table_editor_new ();
    gtk_window_set_default_size (GTK_WINDOW (dialog), 450, 350);
    const char *titles[5];
    titles[0] = _("Key");
    titles[1] = _("Single press");
    titles[2] = _("Left thumb shift");
    titles[3] = _("Right thumb shift");
    titles[4] = NULL;
    scim_anthy_table_editor_set_columns (SCIM_ANTHY_TABLE_EDITOR (dialog),
                                         titles);
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (parent));
    gtk_window_set_title (GTK_WINDOW (dialog),
                          _("Customize thumb shift layout table"));

    // option menu area
    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
                        FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, 0);
    gtk_widget_show(hbox);

    GtkWidget *label = gtk_label_new_with_mnemonic (_("Layout _table:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
    gtk_widget_show (label);

    GtkWidget *omenu = gtk_option_menu_new ();
    __widget_nicola_layout_menu2 = omenu;
    g_object_add_weak_pointer (G_OBJECT (omenu),
                               (gpointer*) &__widget_nicola_layout_menu2);
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 2);
    setup_nicola_layout_menu (GTK_OPTION_MENU (omenu));
    gtk_option_menu_set_history
        (GTK_OPTION_MENU (omenu),
         gtk_option_menu_get_history (
             GTK_OPTION_MENU (__widget_nicola_layout_menu)));
    gtk_widget_show (omenu);

    gtk_label_set_mnemonic_widget (GTK_LABEL(label), omenu);

#if 0
    GtkWidget *button = gtk_button_new_with_mnemonic ("Save _as...");
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);
#endif

    // set data and connect signals
    setup_nicola_window_value (SCIM_ANTHY_TABLE_EDITOR (dialog));
    g_signal_connect (G_OBJECT (omenu), "changed",
                      G_CALLBACK (on_nicola_layout_menu_changed),
                      dialog);
    g_signal_connect (G_OBJECT (dialog), "add-entry",
                      G_CALLBACK (on_nicola_table_editor_add_entry),
                      NULL);
    g_signal_connect (G_OBJECT (dialog), "remove-entry",
                      G_CALLBACK (on_nicola_table_editor_remove_entry),
                      NULL);
    g_signal_connect_after (G_OBJECT (dialog), "add-entry",
                            G_CALLBACK (on_nicola_table_editor_added_entry),
                            NULL);
    g_signal_connect_after (G_OBJECT (dialog), "remove-entry",
                            G_CALLBACK (on_nicola_table_editor_removed_entry),
                            NULL);

    return dialog;
}

static void
setup_kana_page (void)
{
    setup_kana_layout_menu (GTK_OPTION_MENU (__widget_kana_layout_menu));
    setup_nicola_layout_menu (GTK_OPTION_MENU (__widget_nicola_layout_menu));
}

static void
setup_kana_layout_menu (GtkOptionMenu *omenu)
{
    GtkWidget *menu = gtk_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu),
                              menu);
    gtk_widget_show (menu);

    // create menu items
    GtkWidget *menuitem = gtk_menu_item_new_with_label (_("User defined"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    menuitem = gtk_menu_item_new_with_label (_("Default"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);

    StyleFiles::iterator it;
    unsigned int i;
    for (i = 0, it = __style_list.begin ();
         it != __style_list.end ();
         i++, it++)
    {
        StyleLines section;
        if (!it->get_entry_list (section, __kana_fund_table))
            continue;

        menuitem = gtk_menu_item_new_with_label (_(it->get_title().c_str()));
        g_object_set_data (G_OBJECT (menuitem),
                           INDEX_KEY, GINT_TO_POINTER (i));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        gtk_widget_show (menuitem);
    }

    // set default value
    g_signal_handlers_block_by_func (
        G_OBJECT (omenu),
        (gpointer) (on_kana_layout_menu_changed),
        NULL);

    gtk_option_menu_set_history (GTK_OPTION_MENU (omenu),
                                 KANA_LAYOUT_INDEX_DEFAULT);

    if (__config_kana_layout_file == __user_style_file.get_file_name ()) {
        gtk_option_menu_set_history (GTK_OPTION_MENU (omenu),
                                     KANA_LAYOUT_INDEX_USER_DEFINED);

    } else {
        GList *node, *list = gtk_container_get_children (GTK_CONTAINER (menu));
        for (i = 2, node = g_list_next (g_list_next (list));
             node;
             i++, node = g_list_next (node))
        {
            gint idx = GPOINTER_TO_INT (
                g_object_get_data (G_OBJECT (node->data), INDEX_KEY));
            if (__style_list[idx].get_file_name () == __config_kana_layout_file) {
                gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), i);
                break;
            }
        }
    }

    g_signal_handlers_unblock_by_func (
        G_OBJECT (omenu),
        (gpointer) (on_kana_layout_menu_changed),
        NULL);
}

static void
setup_nicola_layout_menu (GtkOptionMenu *omenu)
{
    GtkWidget *menu = gtk_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu),
                              menu);
    gtk_widget_show (menu);

    // create menu items
    GtkWidget *menuitem = gtk_menu_item_new_with_label (_("User defined"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    menuitem = gtk_menu_item_new_with_label (_("Default"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);

    StyleFiles::iterator it;
    unsigned int i;
    for (i = 0, it = __style_list.begin ();
         it != __style_list.end ();
         i++, it++)
    {
        StyleLines section;
        if (!it->get_entry_list (section, __nicola_fund_table))
            continue;

        menuitem = gtk_menu_item_new_with_label (_(it->get_title().c_str()));
        g_object_set_data (G_OBJECT (menuitem),
                           INDEX_KEY, GINT_TO_POINTER (i));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        gtk_widget_show (menuitem);
    }

    // set default value
    g_signal_handlers_block_by_func (
        G_OBJECT (omenu),
        (gpointer) (on_nicola_layout_menu_changed),
        NULL);

    gtk_option_menu_set_history (GTK_OPTION_MENU (omenu),
                                 NICOLA_LAYOUT_INDEX_DEFAULT);

    if (__config_nicola_layout_file == __user_style_file.get_file_name ()) {
        gtk_option_menu_set_history (GTK_OPTION_MENU (omenu),
                                     NICOLA_LAYOUT_INDEX_USER_DEFINED);

    } else {
        GList *node, *list = gtk_container_get_children (GTK_CONTAINER (menu));
        for (i = 2, node = g_list_next (g_list_next (list));
             node;
             i++, node = g_list_next (node))
        {
            gint idx = GPOINTER_TO_INT (
                g_object_get_data (G_OBJECT (node->data), INDEX_KEY));
            if (__style_list[idx].get_file_name () == __config_nicola_layout_file) {
                gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), i);
                break;
            }
        }
    }

    g_signal_handlers_unblock_by_func (
        G_OBJECT (omenu),
        (gpointer) (on_nicola_layout_menu_changed),
        NULL);
}

static void
setup_kana_window_value (ScimAnthyTableEditor *editor)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (editor->treeview);
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkListStore *store = GTK_LIST_STORE (model);

    gtk_list_store_clear (store);

    std::vector<String> keys;
    __user_style_file.get_key_list (keys, __kana_fund_table);
    if (keys.empty ()) {
        load_kana_layout ();
        __user_style_file.get_key_list (keys, __kana_fund_table);
    }

    std::vector<String>::iterator it;
    for (it = keys.begin (); it != keys.end (); it++) {
        std::vector<WideString> value;
        __user_style_file.get_string_array (value, __kana_fund_table, *it);
        String result, cont;
        if (value.size () > 0) result = utf8_wcstombs(value[0]);
        if (value.size () > 1) result = utf8_wcstombs(value[1]);
        GtkTreeIter iter;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, it->c_str (),
                            1, result.c_str (),
                            -1);
    }
}

static void
setup_nicola_window_value (ScimAnthyTableEditor *editor)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (editor->treeview);
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkListStore *store = GTK_LIST_STORE (model);

    gtk_list_store_clear (store);

    std::vector<String> keys;
    __user_style_file.get_key_list (keys, __nicola_fund_table);
    if (keys.empty ()) {
        load_nicola_layout ();
        __user_style_file.get_key_list (keys, __nicola_fund_table);
    }

    std::vector<String>::iterator it;
    for (it = keys.begin (); it != keys.end (); it++) {
        std::vector<WideString> value;
        __user_style_file.get_string_array (value, __nicola_fund_table, *it);
        String single, left, right;
        if (value.size () > 0) single = utf8_wcstombs(value[0]);
        if (value.size () > 1) left   = utf8_wcstombs(value[1]);
        if (value.size () > 2) right  = utf8_wcstombs(value[2]);
        GtkTreeIter iter;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, it->c_str (),
                            1, single.c_str (),
                            2, left.c_str (),
                            3, right.c_str (),
                            -1);
    }
}

static void
setup_default_kana_table (void)
{
    __user_style_file.delete_section (__kana_fund_table);
    ConvRule *table = scim_anthy_kana_typing_rule;
    for (unsigned int i = 0; table[i].string; i++) {
        std::vector<String> value;
        if ((table[i].result && *(table[i].result)) ||
            (table[i].cont && *(table[i].cont)))
        {
            const char *result = table[i].result ? table[i].result : "";
            value.push_back (result);
        }
        if (table[i].cont && *(table[i].cont)) {
            value.push_back (table[i].cont);
        }
        __user_style_file.set_string_array (__kana_fund_table,
                                            table[i].string,
                                            value);
    }
}

static void
setup_default_nicola_table (void)
{
    __user_style_file.delete_section (__nicola_fund_table);
    NicolaRule *table = scim_anthy_nicola_table;
    for (unsigned int i = 0; table[i].key; i++) {
        std::vector<String> value;
        const char *str;
        str = table[i].single ? table[i].single : "";
        value.push_back (str);
        str = table[i].left_shift ? table[i].left_shift : "";
        value.push_back (str);
        str = table[i].right_shift ? table[i].right_shift : "";
        value.push_back (str);
        __user_style_file.set_string_array (__nicola_fund_table,
                                            table[i].key,
                                            value);
    }
}

static bool
load_kana_layout (void)
{
    GtkOptionMenu *omenu = GTK_OPTION_MENU (__widget_kana_layout_menu);
    gint idx = gtk_option_menu_get_history (omenu);
    GtkWidget *menu = gtk_option_menu_get_menu (omenu);
    GList *list = gtk_container_get_children (GTK_CONTAINER (menu));
    GtkWidget *menuitem = GTK_WIDGET (g_list_nth_data (list, idx));

    if (!menuitem)
        return false;

    gint theme_idx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                         INDEX_KEY));

    // set new kana table
    if (idx == KANA_LAYOUT_INDEX_USER_DEFINED) {
        // User defined table
        __config_kana_layout_file = __user_style_file.get_file_name ();
        StyleLines lines;
        bool success = __user_style_file.get_entry_list
                           (lines, __kana_fund_table);
        if (!success || lines.empty ())
            setup_default_kana_table ();

        return true;

    } else if (idx == KANA_LAYOUT_INDEX_DEFAULT) {
        // Default table
        __config_kana_layout_file = "";
        setup_default_kana_table ();

        return true;

    } else if (theme_idx >= 0 && theme_idx < (int) __style_list.size ()) {
        // Tables defined in system theme files
        __config_kana_layout_file = __style_list[theme_idx].get_file_name ();
        __user_style_file.delete_section (__kana_fund_table);

        std::vector<String> keys;
        bool success = __style_list[theme_idx].get_key_list
                           (keys, __kana_fund_table);
        if (success) {
            std::vector<String>::iterator it;
            for (it = keys.begin (); it != keys.end (); it++) {
                std::vector<WideString> value;
                __style_list[theme_idx].get_string_array
                    (value, __kana_fund_table, *it);
                __user_style_file.set_string_array (__kana_fund_table,
                                                    *it, value);
            }
        }
        return true;

    } else {
        // error
        return false;
    }
}

static bool
load_nicola_layout (void)
{
    GtkOptionMenu *omenu = GTK_OPTION_MENU (__widget_nicola_layout_menu);
    gint idx = gtk_option_menu_get_history (omenu);
    GtkWidget *menu = gtk_option_menu_get_menu (omenu);
    GList *list = gtk_container_get_children (GTK_CONTAINER (menu));
    GtkWidget *menuitem = GTK_WIDGET (g_list_nth_data (list, idx));

    if (!menuitem)
        return false;

    gint theme_idx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                         INDEX_KEY));

    // set new NICOLA table
    if (idx == NICOLA_LAYOUT_INDEX_USER_DEFINED) {
        // User defined table
        __config_nicola_layout_file = __user_style_file.get_file_name ();
        StyleLines lines;
        bool success = __user_style_file.get_entry_list
                           (lines, __nicola_fund_table);
        if (!success || lines.empty ())
            setup_default_nicola_table ();

        return true;

    } else if (idx == NICOLA_LAYOUT_INDEX_DEFAULT) {
        // Default table
        __config_nicola_layout_file = "";
        setup_default_nicola_table ();

        return true;

    } else if (theme_idx >= 0 && theme_idx < (int) __style_list.size ()) {
        // Tables defined in system theme files
        __config_nicola_layout_file = __style_list[theme_idx].get_file_name ();
        __user_style_file.delete_section (__nicola_fund_table);

        std::vector<String> keys;
        bool success = __style_list[theme_idx].get_key_list
                           (keys, __nicola_fund_table);
        if (success) {
            std::vector<String>::iterator it;
            for (it = keys.begin (); it != keys.end (); it++) {
                std::vector<WideString> value;
                __style_list[theme_idx].get_string_array
                    (value, __nicola_fund_table, *it);
                __user_style_file.set_string_array (__nicola_fund_table,
                                                    *it, value);
            }
        }
        return true;

    } else {
        // error
        return false;
    }
}

static bool
has_voiced_consonant (String str)
{
    ConvRule *table = scim_anthy_kana_voiced_consonant_rule;

    WideString str1_wide = utf8_mbstowcs (str);
    if (str1_wide.length () <= 0)
        return false;

    for (unsigned int i = 0; table[i].string; i++) {
        WideString str2_wide = utf8_mbstowcs (table[i].string);
        if (str2_wide.length () <= 0)
            continue;
        if (str1_wide[0] == str2_wide[0])
            return true;
    }

    return false;
}

static void
on_kana_layout_menu_changed (GtkOptionMenu *omenu, gpointer user_data)
{
    bool success;

    if (__widget_kana_layout_menu != GTK_WIDGET (omenu)) {
        g_signal_handlers_block_by_func (
            G_OBJECT (__widget_kana_layout_menu),
            (gpointer) (on_kana_layout_menu_changed),
            NULL);
        gtk_option_menu_set_history (
            GTK_OPTION_MENU (__widget_kana_layout_menu),
            gtk_option_menu_get_history (omenu));
        g_signal_handlers_unblock_by_func (
            G_OBJECT (__widget_kana_layout_menu),
            (gpointer) (on_kana_layout_menu_changed),
            NULL);

        success = load_kana_layout ();

        setup_kana_window_value (SCIM_ANTHY_TABLE_EDITOR (user_data));
    } else {
        success = load_kana_layout ();
    }

    if (success) {
        // sync widgets
        __style_changed  = true;
        __config_changed = true;
    }
}

static void
on_kana_customize_button_clicked (GtkWidget *button, gpointer data)
{
    GtkWidget *widget = create_kana_window (
        GTK_WINDOW (gtk_widget_get_toplevel (button)));
    gtk_dialog_run (GTK_DIALOG (widget));
    gtk_widget_destroy (widget);
}

static void
on_kana_table_editor_add_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    const gchar *sequence, *result;
    sequence = scim_anthy_table_editor_get_nth_text (editor, 0);
    result   = scim_anthy_table_editor_get_nth_text (editor, 1);

    // real add
    std::vector<String> value;
    if (has_voiced_consonant (result))
        value.push_back ("");
    value.push_back (result);
    __user_style_file.set_string_array (__kana_fund_table, sequence, value);
}

static void
on_kana_table_editor_added_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    // change menu item to "User defined"
    gtk_option_menu_set_history (
        GTK_OPTION_MENU (__widget_kana_layout_menu2),
        KANA_LAYOUT_INDEX_USER_DEFINED);

    __style_changed = true;
}

static void
on_kana_table_editor_remove_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    const gchar *sequence;
    sequence = scim_anthy_table_editor_get_nth_text (editor, 0);

    // real remove
    __user_style_file.delete_key (__kana_fund_table, sequence);
}

static void
on_kana_table_editor_removed_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    // change menu item to "User deined"
    gtk_option_menu_set_history (
        GTK_OPTION_MENU (__widget_kana_layout_menu2),
        KANA_LAYOUT_INDEX_USER_DEFINED);

    __style_changed = true;
}

static void
on_nicola_layout_menu_changed (GtkOptionMenu *omenu, gpointer user_data)
{
    bool success;

    if (__widget_nicola_layout_menu != GTK_WIDGET (omenu)) {
        g_signal_handlers_block_by_func (
            G_OBJECT (__widget_nicola_layout_menu),
            (gpointer) (on_nicola_layout_menu_changed),
            NULL);
        gtk_option_menu_set_history (
            GTK_OPTION_MENU (__widget_nicola_layout_menu),
            gtk_option_menu_get_history (omenu));
        g_signal_handlers_unblock_by_func (
            G_OBJECT (__widget_nicola_layout_menu),
            (gpointer) (on_nicola_layout_menu_changed),
            NULL);

        success = load_nicola_layout ();

        setup_nicola_window_value (SCIM_ANTHY_TABLE_EDITOR (user_data));
    } else {
        success = load_nicola_layout ();
    }

    if (success) {
        // sync widgets
        __style_changed  = true;
        __config_changed = true;
    }
}

static void
on_nicola_customize_button_clicked (GtkWidget *button, gpointer data)
{
    GtkWidget *widget = create_nicola_window (
        GTK_WINDOW (gtk_widget_get_toplevel (button)));
    gtk_dialog_run (GTK_DIALOG (widget));
    gtk_widget_destroy (widget);
}

static void
on_nicola_table_editor_add_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    const gchar *key = scim_anthy_table_editor_get_nth_text (editor, 0);
    std::vector<String> value;
    value.push_back (scim_anthy_table_editor_get_nth_text (editor, 1));
    value.push_back (scim_anthy_table_editor_get_nth_text (editor, 2));
    value.push_back (scim_anthy_table_editor_get_nth_text (editor, 3));
    __user_style_file.set_string_array (__nicola_fund_table, key, value);
}

static void
on_nicola_table_editor_added_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    // change menu item to "User defined"
    gtk_option_menu_set_history (
        GTK_OPTION_MENU (__widget_nicola_layout_menu2),
        NICOLA_LAYOUT_INDEX_USER_DEFINED);

    __style_changed = true;
}

static void
on_nicola_table_editor_remove_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    const gchar *sequence;
    sequence = scim_anthy_table_editor_get_nth_text (editor, 0);

    // real remove
    __user_style_file.delete_key (__nicola_fund_table, sequence);
}

static void
on_nicola_table_editor_removed_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    // change menu item to "User deined"
    gtk_option_menu_set_history (
        GTK_OPTION_MENU (__widget_nicola_layout_menu2),
        NICOLA_LAYOUT_INDEX_USER_DEFINED);

    __style_changed = true;
}

}
