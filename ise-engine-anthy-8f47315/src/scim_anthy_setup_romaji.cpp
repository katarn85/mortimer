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
#include "scim_anthy_intl.h"
#include "scim_anthy_style_file.h"
#include "scim_anthy_prefs.h"
#include "scim_anthy_default_tables.h"
#include "scim_anthy_setup.h"
#include "scim_anthy_table_editor.h"

using namespace scim;

namespace scim_anthy {

#define INDEX_KEY "scim-anthy::Index"
static const char * const __romaji_fund_table = "RomajiTable/FundamentalTable";

static const int ROMAJI_THEME_INDEX_USER_DEFINED = 0;
static const int ROMAJI_THEME_INDEX_DEFAULT      = 1;

// Internal data declaration.
static GtkWidget   * __widget_romaji_theme_menu     = NULL;
static GtkWidget   * __widget_romaji_theme_menu2    = NULL;

static String __config_romaji_theme_file = SCIM_ANTHY_CONFIG_ROMAJI_THEME_FILE_DEFAULT;

static GtkWidget *create_romaji_window            (GtkWindow            *parent);

static void     setup_romaji_page                 (void);
static void     setup_romaji_theme_menu           (GtkOptionMenu        *omenu);
static void     setup_romaji_window_value         (ScimAnthyTableEditor *editor);

static bool     load_romaji_theme                 (void);

static void     on_romaji_pseudo_ascii_mode_toggled(GtkToggleButton *togglebutton,
                                                    gpointer         user_data);
static void     on_romaji_theme_menu_changed       (GtkOptionMenu        *omenu,
                                                    gpointer              user_data);
static void     on_romaji_customize_button_clicked (GtkWidget            *button,
                                                    gpointer              data);
static void     on_table_editor_add_entry          (ScimAnthyTableEditor *editor,
                                                    gpointer              data);
static void     on_table_editor_added_entry        (ScimAnthyTableEditor *editor,
                                                    gpointer              data);
static void     on_table_editor_remove_entry       (ScimAnthyTableEditor *editor,
                                                    gpointer              data);
static void     on_table_editor_removed_entry      (ScimAnthyTableEditor *editor,
                                                    gpointer              data);

GtkWidget *
romaji_page_create_ui (void)
{
    GtkWidget *vbox, *widget, *hbox, *label;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    /* romaji splitting */
    widget = create_check_button (SCIM_ANTHY_CONFIG_ROMAJI_ALLOW_SPLIT);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 4);

    /* symbol */
    widget = create_check_button (SCIM_ANTHY_CONFIG_ROMAJI_HALF_SYMBOL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 4);

    /* number */
    widget = create_check_button (SCIM_ANTHY_CONFIG_ROMAJI_HALF_NUMBER);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 4);

    /* pseudo ascii mode */
    widget = create_check_button (SCIM_ANTHY_CONFIG_ROMAJI_PSEUDO_ASCII_MODE);
    g_signal_connect ((gpointer) widget, "toggled",
                      G_CALLBACK (on_romaji_pseudo_ascii_mode_toggled),
                      NULL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 4);

    /* pseudo ascii: blank behavior */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
    gtk_widget_show (hbox);
    label = gtk_label_new ("    ");
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    widget = create_check_button (SCIM_ANTHY_CONFIG_ROMAJI_PSEUDO_ASCII_BLANK_BEHAVIOR);
    gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
    // set initial state
    on_romaji_pseudo_ascii_mode_toggled (GTK_TOGGLE_BUTTON (widget), NULL);

    /* romaji table */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    label = gtk_label_new_with_mnemonic (_("Romaji _table:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
    gtk_widget_show (label);

    GtkWidget *omenu = gtk_option_menu_new ();
    __widget_romaji_theme_menu = omenu;
    g_signal_connect (G_OBJECT (omenu), "changed",
                      G_CALLBACK (on_romaji_theme_menu_changed), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 2);
    gtk_widget_show (omenu);

    gtk_label_set_mnemonic_widget (GTK_LABEL(label), omenu);

    GtkWidget *button = gtk_button_new_with_mnemonic (_("_Customize..."));
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_romaji_customize_button_clicked), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);

    setup_romaji_page ();

    return vbox;
}

void
romaji_page_load_config (const ConfigPointer &config)
{
    __config_romaji_theme_file
        = config->read (String (SCIM_ANTHY_CONFIG_ROMAJI_THEME_FILE),
                        String (SCIM_ANTHY_CONFIG_ROMAJI_THEME_FILE_DEFAULT));
    setup_romaji_page ();
}

void
romaji_page_save_config (const ConfigPointer &config)
{
    __config_romaji_theme_file
        = config->write (String (SCIM_ANTHY_CONFIG_ROMAJI_THEME_FILE),
                         String (__config_romaji_theme_file));
}

bool
romaji_page_query_changed (void)
{
    return __config_changed || __style_changed;
}


static GtkWidget *
create_romaji_window (GtkWindow *parent)
{
    GtkWidget *dialog = scim_anthy_table_editor_new ();
    const char *titles[3];
    titles[0] = _("Sequence");
    titles[1] = _("Result");
    titles[2] = NULL;
    scim_anthy_table_editor_set_columns (SCIM_ANTHY_TABLE_EDITOR (dialog),
                                         titles);
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (parent));
    gtk_window_set_title (GTK_WINDOW (dialog),
                          _("Customize romaji table"));

    // option menu area
    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
                        FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, 0);
    gtk_widget_show(hbox);

    GtkWidget *label = gtk_label_new_with_mnemonic (_("Romaji _table:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
    gtk_widget_show (label);

    GtkWidget *omenu = gtk_option_menu_new ();
    __widget_romaji_theme_menu2 = omenu;
    g_object_add_weak_pointer (G_OBJECT (omenu),
                               (gpointer*) &__widget_romaji_theme_menu2);
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 2);
    setup_romaji_theme_menu (GTK_OPTION_MENU (omenu));
    gtk_option_menu_set_history
        (GTK_OPTION_MENU (omenu),
         gtk_option_menu_get_history (
             GTK_OPTION_MENU (__widget_romaji_theme_menu)));
    gtk_widget_show (omenu);

    gtk_label_set_mnemonic_widget (GTK_LABEL(label), omenu);

#if 0
    GtkWidget *button = gtk_button_new_with_mnemonic ("Save _as...");
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);
#endif

    // set data and connect signals
    setup_romaji_window_value (SCIM_ANTHY_TABLE_EDITOR (dialog));
    g_signal_connect (G_OBJECT (omenu), "changed",
                      G_CALLBACK (on_romaji_theme_menu_changed),
                      dialog);
    g_signal_connect (G_OBJECT (dialog), "add-entry",
                      G_CALLBACK (on_table_editor_add_entry),
                      NULL);
    g_signal_connect (G_OBJECT (dialog), "remove-entry",
                      G_CALLBACK (on_table_editor_remove_entry),
                      NULL);
    g_signal_connect_after (G_OBJECT (dialog), "add-entry",
                            G_CALLBACK (on_table_editor_added_entry),
                            NULL);
    g_signal_connect_after (G_OBJECT (dialog), "remove-entry",
                            G_CALLBACK (on_table_editor_removed_entry),
                            NULL);

    return dialog;
}

static void
setup_romaji_page (void)
{
    setup_romaji_theme_menu (GTK_OPTION_MENU (__widget_romaji_theme_menu));
}

static void
setup_romaji_theme_menu (GtkOptionMenu *omenu)
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
        if (!it->get_entry_list (section, __romaji_fund_table))
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
        (gpointer) (on_romaji_theme_menu_changed),
        NULL);

    gtk_option_menu_set_history (GTK_OPTION_MENU (omenu),
                                 ROMAJI_THEME_INDEX_DEFAULT);

    if (__config_romaji_theme_file == __user_style_file.get_file_name ()) {
        gtk_option_menu_set_history (GTK_OPTION_MENU (omenu),
                                     ROMAJI_THEME_INDEX_USER_DEFINED);

    } else {
        GList *node, *list = gtk_container_get_children (GTK_CONTAINER (menu));
        for (i = 2, node = g_list_next (g_list_next (list));
             node;
             i++, node = g_list_next (node))
        {
            gint idx = GPOINTER_TO_INT (
                g_object_get_data (G_OBJECT (node->data), INDEX_KEY));
            if (__style_list[idx].get_file_name () == __config_romaji_theme_file) {
                gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), i);
                break;
            }
        }
    }

    g_signal_handlers_unblock_by_func (
        G_OBJECT (omenu),
        (gpointer) (on_romaji_theme_menu_changed),
        NULL);
}

static void
setup_romaji_window_value (ScimAnthyTableEditor *editor)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (editor->treeview);
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkListStore *store = GTK_LIST_STORE (model);

    gtk_list_store_clear (store);

    std::vector<String> keys;
    __user_style_file.get_key_list (keys, __romaji_fund_table);
    if (keys.empty ()) {
        load_romaji_theme ();
        __user_style_file.get_key_list (keys, __romaji_fund_table);
    }

    std::vector<String>::iterator it;
    for (it = keys.begin (); it != keys.end (); it++) {
        std::vector<WideString> value;
        __user_style_file.get_string_array (value, __romaji_fund_table, *it);
        String result, cont;
        if (value.size () > 0) result = utf8_wcstombs(value[0]);
        if (value.size () > 1) cont   = utf8_wcstombs(value[1]);
        GtkTreeIter iter;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, it->c_str (),
                            1, result.c_str (),
                            -1);
    }
}

static void
setup_default_romaji_table (void)
{
    __user_style_file.delete_section (__romaji_fund_table);
    ConvRule *table = scim_anthy_romaji_typing_rule;
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
        __user_style_file.set_string_array (__romaji_fund_table,
                                            table[i].string,
                                            value);
    }
}

static bool
load_romaji_theme (void)
{
    GtkOptionMenu *omenu = GTK_OPTION_MENU (__widget_romaji_theme_menu);
    gint idx = gtk_option_menu_get_history (omenu);
    GtkWidget *menu = gtk_option_menu_get_menu (omenu);
    GList *list = gtk_container_get_children (GTK_CONTAINER (menu));
    GtkWidget *menuitem = GTK_WIDGET (g_list_nth_data (list, idx));

    if (!menuitem)
        return false;

    gint theme_idx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                         INDEX_KEY));

    // set new romaji table
    if (idx == ROMAJI_THEME_INDEX_USER_DEFINED) {
        // User defined table
        __config_romaji_theme_file = __user_style_file.get_file_name ();
        StyleLines lines;
        bool success = __user_style_file.get_entry_list
                           (lines, __romaji_fund_table);
        if (!success || lines.empty ())
            setup_default_romaji_table ();

        return true;

    } else if (idx == ROMAJI_THEME_INDEX_DEFAULT) {
        // Default table
        __config_romaji_theme_file = "";
        setup_default_romaji_table ();

        return true;

    } else if (theme_idx >= 0 && theme_idx < (int) __style_list.size ()) {
        // Tables defined in system theme files
        __config_romaji_theme_file = __style_list[theme_idx].get_file_name ();
        __user_style_file.delete_section (__romaji_fund_table);

        std::vector<String> keys;
        bool success = __style_list[theme_idx].get_key_list
                           (keys, __romaji_fund_table);
        if (success) {
            std::vector<String>::iterator it;
            for (it = keys.begin (); it != keys.end (); it++) {
                std::vector<WideString> value;
                __style_list[theme_idx].get_string_array
                    (value, __romaji_fund_table, *it);
                __user_style_file.set_string_array (__romaji_fund_table,
                                                    *it, value);
            }
        }
        return true;

    } else {
        // error
        return false;
    }
}

static void
on_romaji_pseudo_ascii_mode_toggled (GtkToggleButton *togglebutton,
                                     gpointer         user_data)
{
    gboolean active = gtk_toggle_button_get_active (togglebutton);
    BoolConfigData *entry;

    entry = find_bool_config_entry (SCIM_ANTHY_CONFIG_ROMAJI_PSEUDO_ASCII_BLANK_BEHAVIOR);
    if (entry->widget)
        gtk_widget_set_sensitive (GTK_WIDGET (entry->widget), active);
}

static void
on_romaji_theme_menu_changed (GtkOptionMenu *omenu, gpointer user_data)
{
    bool success;

    if (__widget_romaji_theme_menu != GTK_WIDGET (omenu)) {
        g_signal_handlers_block_by_func (
            G_OBJECT (__widget_romaji_theme_menu),
            (gpointer) (on_romaji_theme_menu_changed),
            NULL);
        gtk_option_menu_set_history (
            GTK_OPTION_MENU (__widget_romaji_theme_menu),
            gtk_option_menu_get_history (omenu));
        g_signal_handlers_unblock_by_func (
            G_OBJECT (__widget_romaji_theme_menu),
            (gpointer) (on_romaji_theme_menu_changed),
            NULL);

        success = load_romaji_theme ();

        setup_romaji_window_value (SCIM_ANTHY_TABLE_EDITOR (user_data));
    } else {
        success = load_romaji_theme ();
    }

    if (success) {
        // sync widgets
        __style_changed  = true;
        __config_changed = true;
    }
}

static void
on_romaji_customize_button_clicked (GtkWidget *button, gpointer data)
{
    GtkWidget *widget = create_romaji_window (
        GTK_WINDOW (gtk_widget_get_toplevel (button)));
    gtk_dialog_run (GTK_DIALOG (widget));
    gtk_widget_destroy (widget);
}

static void
on_table_editor_add_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    const gchar *sequence, *result;
    sequence = scim_anthy_table_editor_get_nth_text (editor, 0);
    result   = scim_anthy_table_editor_get_nth_text (editor, 1);

    // real add
    __user_style_file.set_string (__romaji_fund_table, sequence, result);
}

static void
on_table_editor_added_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    // change menu item to "User defined"
    gtk_option_menu_set_history (
        GTK_OPTION_MENU (__widget_romaji_theme_menu2),
        ROMAJI_THEME_INDEX_USER_DEFINED);

    __style_changed = true;
}

static void
on_table_editor_remove_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    const gchar *sequence;
    sequence = scim_anthy_table_editor_get_nth_text (editor, 0);

    // real remove
    __user_style_file.delete_key (__romaji_fund_table, sequence);

    __style_changed = true;
}

static void
on_table_editor_removed_entry (ScimAnthyTableEditor *editor, gpointer data)
{
    // change menu item to "User deined"
    gtk_option_menu_set_history (
        GTK_OPTION_MENU (__widget_romaji_theme_menu2),
        ROMAJI_THEME_INDEX_USER_DEFINED);

    __style_changed = true;
}

}
