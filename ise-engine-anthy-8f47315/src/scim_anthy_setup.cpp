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

/*
 * Based on scim-hangul.
 * Copyright (c) 2004 James Su <suzhe@turbolinux.com.cn>
 */

#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#define Uses_SCIM_CONFIG_BASE
#define Uses_SCIM_EVENT
#define SCIM_ANTHY_USE_GTK
#include <scim.h>
#include <gtk/scimkeyselection.h>
#include "scim_anthy_intl.h"
#include "scim_anthy_style_file.h"
#include "scim_anthy_prefs.h"
#include "scim_anthy_default_tables.h"
#include "scim_anthy_setup.h"
#include "scim_anthy_setup_romaji.h"
#include "scim_anthy_setup_kana.h"
#include "scim_anthy_utils.h"
#include "scim_anthy_color_button.h"

using namespace scim;
using namespace scim_anthy;

#define scim_module_init anthy_imengine_setup_LTX_scim_module_init
#define scim_module_exit anthy_imengine_setup_LTX_scim_module_exit

#define scim_setup_module_create_ui       anthy_imengine_setup_LTX_scim_setup_module_create_ui
#define scim_setup_module_get_category    anthy_imengine_setup_LTX_scim_setup_module_get_category
#define scim_setup_module_get_name        anthy_imengine_setup_LTX_scim_setup_module_get_name
#define scim_setup_module_get_description anthy_imengine_setup_LTX_scim_setup_module_get_description
#define scim_setup_module_load_config     anthy_imengine_setup_LTX_scim_setup_module_load_config
#define scim_setup_module_save_config     anthy_imengine_setup_LTX_scim_setup_module_save_config
#define scim_setup_module_query_changed   anthy_imengine_setup_LTX_scim_setup_module_query_changed

#define DATA_POINTER_KEY "scim-anthy::ConfigPointer"
#define INDEX_KEY        "scim-anthy::Index"

namespace scim_anthy {

static GtkWidget * create_setup_window (void);
static void        load_style_files    (const char *dirname);
static void        load_config         (const ConfigPointer &config);
static void        save_config         (const ConfigPointer &config);
static bool        query_changed       (void);

//static StyleFiles __style_list;
//static StyleFile  __user_style_file;
StyleFiles __style_list;
StyleFile  __user_style_file;
const String __user_config_dir_name =
    scim_get_home_dir () +
    String (SCIM_PATH_DELIM_STRING
            ".scim"
            SCIM_PATH_DELIM_STRING
            "Anthy");
const String __user_style_dir_name =
    __user_config_dir_name +
    String (SCIM_PATH_DELIM_STRING
            "style");
const String __user_style_file_name =
    __user_config_dir_name +
    String (SCIM_PATH_DELIM_STRING
            "config.sty");

}

// Module Interface.
extern "C" {
    void scim_module_init (void)
    {
        bindtextdomain (GETTEXT_PACKAGE, SCIM_ANTHY_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    }

    void scim_module_exit (void)
    {
    }

    GtkWidget * scim_setup_module_create_ui (void)
    {
        return create_setup_window ();
    }

    String scim_setup_module_get_category (void)
    {
        return String ("IMEngine");
    }

    String scim_setup_module_get_name (void)
    {
        return String (_("Anthy"));
    }

    String scim_setup_module_get_description (void)
    {
        return String (_("An Anthy IMEngine Module."));
    }

    void scim_setup_module_load_config (const ConfigPointer &config)
    {
        load_config (config);
    }

    void scim_setup_module_save_config (const ConfigPointer &config)
    {
        save_config (config);
    }

    bool scim_setup_module_query_changed ()
    {
        return query_changed ();
    }
} // extern "C"



namespace scim_anthy {

// Internal data structure
struct KeyboardConfigPage
{
    const char       *label;
    StringConfigData *data;
};

struct ComboConfigCandidate
{
    const char *label;
    const char *data;
};

enum {
    COLUMN_LABEL = 0,
    COLUMN_VALUE = 1,
    COLUMN_DESC  = 2,
    COLUMN_DATA  = 3,
    N_COLUMNS    = 4,
};

// Internal data declaration.
bool __config_changed = false;
bool __style_changed  = false;
static GtkWidget   * __widget_key_categories_menu   = NULL;
static GtkWidget   * __widget_key_filter            = NULL;
static GtkWidget   * __widget_key_filter_button     = NULL;
static GtkWidget   * __widget_key_theme_menu        = NULL;
static GtkWidget   * __widget_key_list_view         = NULL;
static GtkWidget   * __widget_choose_keys_button    = NULL;
static GtkTooltips * __widget_tooltips              = NULL;

static String __config_key_theme      = SCIM_ANTHY_CONFIG_KEY_THEME_DEFAULT;
static String __config_key_theme_file = SCIM_ANTHY_CONFIG_KEY_THEME_FILE_DEFAULT;

static struct KeyboardConfigPage __key_conf_pages[] =
{
    {N_("Mode keys"),          config_keyboards_mode},
    {N_("Edit keys"),          config_keyboards_edit},
    {N_("Caret keys"),         config_keyboards_caret},
    {N_("Segments keys"),      config_keyboards_segments},
    {N_("Candidates keys"),    config_keyboards_candidates},
    {N_("Direct select keys"), config_keyboards_direct_select},
    {N_("Convert keys"),       config_keyboards_converting},
    {N_("Dictionary keys"),    config_keyboards_dict},
};
static unsigned int __key_conf_pages_num = sizeof (__key_conf_pages) / sizeof (KeyboardConfigPage);

const int KEY_CATEGORY_INDEX_SEARCH_BY_KEY = __key_conf_pages_num;
const int KEY_CATEGORY_INDEX_ALL           = __key_conf_pages_num + 1;

const int KEY_THEME_INDEX_USER_DEFINED = 0;
const int KEY_THEME_INDEX_DEFAULT      = 1;

static ComboConfigCandidate input_modes[] =
{
    {N_("Hiragana"),            "Hiragana"},
    {N_("Katakana"),            "Katakana"},
    {N_("Half width katakana"), "HalfKatakana"},
    {N_("Latin"),               "Latin"},
    {N_("Wide latin"),          "WideLatin"},
    {NULL, NULL},
};

static ComboConfigCandidate typing_methods[] =
{
    {N_("Romaji typing method"),      "Romaji"},
    {N_("Kana typing method"),        "Kana"},
    {N_("Thumb shift typing method"), "NICOLA"},
    {NULL, NULL},
};

static ComboConfigCandidate conversion_modes[] =
{
    {N_("Multi segment"),                        "MultiSeg"},
    {N_("Single segment"),                       "SingleSeg"},
    {N_("Convert as you type (Multi segment)"),  "CAYT_MultiSeg"},
    {N_("Convert as you type (Single segment)"), "CAYT_SingleSeg"},
    {NULL, NULL},
};

static ComboConfigCandidate period_styles[] =
{
    {"\xE3\x80\x81\xE3\x80\x82", "Japanese"},
    {"\xEF\xBC\x8C\xE3\x80\x82", "WideLatin_Japanese"},
    {"\xEF\xBC\x8C\xEF\xBC\x8E", "WideLatin"},
    {",.",                       "Latin"},
    {NULL, NULL},
};

static ComboConfigCandidate symbol_styles[] =
{
    {"\xE3\x80\x8C\xE3\x80\x8D\xE3\x83\xBB", "Japanese"},
    {"\xE3\x80\x8C\xE3\x80\x8D\xEF\xBC\x8F", "CornerBracket_WideSlash"},
    {"\xEF\xBC\xBB\xEF\xBC\xBD\xE3\x83\xBB", "WideBracket_MiddleDot"},
    {"\xEF\xBC\xBB\xEF\xBC\xBD\xEF\xBC\x8F", "WideBracket_WideSlash"},
    {NULL, NULL},
};

static ComboConfigCandidate space_types[] =
{
    {N_("Follow input mode"), "FollowMode"},
    {N_("Wide"),              "Wide"},
    {N_("Half"),              "Half"},
    {NULL, NULL},
};

static ComboConfigCandidate ten_key_types[] =
{
    {N_("Follow input mode"), "FollowMode"},
    {N_("Wide"),              "Wide"},
    {N_("Half"),              "Half"},
    {NULL, NULL},
};

static ComboConfigCandidate behavior_on_period[] =
{
    {N_("Do nothing"),       "None"},
    {N_("Start conversion"), "Convert"},
    {N_("Commit"),           "Commit"},
    {NULL, NULL},
};

static ComboConfigCandidate behavior_on_focus_out[] =
{
    {N_("Commit"), "Commit"},
    {N_("Clear"),  "Clear"},
    {NULL, NULL},
};

static ComboConfigCandidate dict_encoding[] =
{
    {N_("UTF-8"),     "UTF-8"},
    {N_("EUC-JP"),    "EUC-JP"},
    {N_("EUC-JP-MS"), "EUC-JP-MS"},
    {NULL, NULL},
};

static ComboConfigCandidate preedit_style[] =
{
    {N_("No decoration"), "None"},
    {N_("Underline"),     "Underline"},
    {N_("Reverse"),       "Reverse"},
    {N_("Highlight"),     "Highlight"},
    {N_("String color"),  "FGColor"},
    {N_("BG color"),      "BGColor"},
    {N_("Both color"),    "Color"},
    {NULL, NULL},
};


static void     setup_key_theme_menu              (GtkOptionMenu *omenu);
static void     setup_widget_value                (void);

static void     on_default_editable_changed       (GtkEditable      *editable,
                                                   gpointer          user_data);
static void     on_default_toggle_button_toggled  (GtkToggleButton  *togglebutton,
                                                   gpointer          user_data);
static void     on_default_spin_button_changed    (GtkSpinButton    *spinbutton,
                                                   gpointer          user_data);
static void     on_default_key_selection_clicked  (GtkButton        *button,
                                                   gpointer          user_data);
static void     on_default_combo_changed          (GtkEditable      *editable,
                                                   gpointer          user_data);
static void     on_default_option_menu_changed    (GtkOptionMenu    *omenu,
                                                   gpointer          user_data);
static void     on_preedit_style_menu_changed     (GtkOptionMenu    *omenu,
                                                   gpointer          user_data);
static void     on_key_filter_selection_clicked   (GtkButton        *button,
                                                   gpointer          user_data);
static void     on_dict_menu_label_toggled        (GtkToggleButton  *togglebutton,
                                                   gpointer          user_data);
static void     on_key_category_menu_changed      (GtkOptionMenu    *omenu,
                                                   gpointer          user_data);
static gboolean on_key_list_view_key_press        (GtkWidget        *widget,
                                                   GdkEventKey      *event,
                                                   gpointer          user_data);
static gboolean on_key_list_view_button_press     (GtkWidget        *widget,
                                                   GdkEventButton   *event,
                                                   gpointer          user_data);
static void     on_key_theme_menu_changed         (GtkOptionMenu    *omenu,
                                                   gpointer          user_data);
static void     on_key_list_selection_changed     (GtkTreeSelection *selection,
                                                   gpointer          data);
static void     on_choose_keys_button_clicked     (GtkWidget        *button,
                                                   gpointer          data);
static void     on_dict_launch_button_clicked     (GtkButton        *button,
                                                   gpointer          user_data);
static void     on_color_button_changed           (ScimAnthyColorButton  *button,
                                                   gpointer               user_data);


static StringConfigData *
find_key_config_entry (const char *config_key)
{
    for (unsigned int j = 0; j < __key_conf_pages_num; j++) {
        for (unsigned int i = 0; __key_conf_pages[j].data[i].key; i++) {
            StringConfigData *entry = &__key_conf_pages[j].data[i];
            if (entry->key && !strcmp (entry->key, config_key))
                return entry;
        }
    }

    return NULL;
}

GtkWidget *
create_check_button (const char *config_key)
{
    BoolConfigData *entry = find_bool_config_entry (config_key);
    if (!entry)
        return NULL;

    entry->widget = gtk_check_button_new_with_mnemonic (_(entry->label));
    gtk_container_set_border_width (GTK_CONTAINER (entry->widget), 4);
    g_signal_connect (G_OBJECT (entry->widget), "toggled",
                      G_CALLBACK (on_default_toggle_button_toggled),
                      entry);
    gtk_widget_show (GTK_WIDGET (entry->widget));

    if (!__widget_tooltips)
        __widget_tooltips = gtk_tooltips_new();
    if (entry->tooltip)
        gtk_tooltips_set_tip (__widget_tooltips, GTK_WIDGET (entry->widget),
                              _(entry->tooltip), NULL);

    return GTK_WIDGET (entry->widget);
}

GtkWidget *
create_spin_button (const char *config_key, GtkTable *table, int idx)
{
    IntConfigData *entry = find_int_config_entry (config_key);
    if (!entry)
        return NULL;

    GtkWidget *label = gtk_label_new_with_mnemonic (_(entry->label));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_misc_set_padding (GTK_MISC (label), 4, 0);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, idx, idx + 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL),
                      4, 4);
    gtk_widget_show (GTK_WIDGET (label));

    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (hbox),
                      1, 2, idx, idx + 1,
                      (GtkAttachOptions) GTK_FILL,
                      (GtkAttachOptions) GTK_FILL,
                      4, 4);
    gtk_widget_show (hbox);

    entry->widget = gtk_spin_button_new_with_range (entry->min, entry->max,
                                                    entry->step);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                   GTK_WIDGET (entry->widget));
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (entry->widget),
                        FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (entry->widget), "value-changed",
                      G_CALLBACK (on_default_spin_button_changed),
                      entry);
    gtk_widget_show (GTK_WIDGET (entry->widget));

    if (entry->unit) {
        label = gtk_label_new_with_mnemonic (_(entry->unit));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_misc_set_padding (GTK_MISC (label), 4, 0);
        gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (label),
                            FALSE, FALSE, 0);
        gtk_widget_show (GTK_WIDGET (label));
    }

    if (!__widget_tooltips)
        __widget_tooltips = gtk_tooltips_new();
    if (entry->tooltip)
        gtk_tooltips_set_tip (__widget_tooltips, GTK_WIDGET (entry->widget),
                              _(entry->tooltip), NULL);

    return GTK_WIDGET (entry->widget);
}

GtkWidget *
create_entry (const char *config_key, GtkTable *table, int idx)
{
    StringConfigData *entry = find_string_config_entry (config_key);
    if (!entry)
        return NULL;

    GtkWidget *label = gtk_label_new (NULL);
    gtk_label_set_text_with_mnemonic (GTK_LABEL (label), _(entry->label));
    gtk_widget_show (label);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_misc_set_padding (GTK_MISC (label), 4, 0);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, idx, idx + 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 4, 4);
    (entry)->widget = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                   GTK_WIDGET (entry->widget));
    g_signal_connect ((gpointer) (entry)->widget, "changed",
                      G_CALLBACK (on_default_editable_changed),
                      entry);
    gtk_widget_show (GTK_WIDGET (entry->widget));
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (entry->widget),
                      1, 2, idx, idx + 1,
                      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
                      (GtkAttachOptions) (GTK_FILL), 4, 4);

    if (!__widget_tooltips)
        __widget_tooltips = gtk_tooltips_new();
    if (entry->tooltip)
        gtk_tooltips_set_tip (__widget_tooltips, GTK_WIDGET (entry->widget),
                              _(entry->tooltip), NULL);

    return GTK_WIDGET (entry->widget);
}

GtkWidget *
create_combo (const char *config_key, gpointer candidates_p,
              GtkTable *table, gint idx)
{
    StringConfigData *entry = find_string_config_entry (config_key);
    if (!entry)
        return NULL;

    GtkWidget *label;

    label = gtk_label_new_with_mnemonic (_(entry->label));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_misc_set_padding (GTK_MISC (label), 4, 0);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, idx, idx + 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 4, 4);
    gtk_widget_show (label);

    entry->widget = gtk_combo_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                   GTK_COMBO (entry->widget)->entry);
    gtk_combo_set_value_in_list (GTK_COMBO (entry->widget), TRUE, FALSE);
    gtk_combo_set_case_sensitive (GTK_COMBO (entry->widget), TRUE);
    gtk_entry_set_editable (GTK_ENTRY (GTK_COMBO (entry->widget)->entry),
                            FALSE);
    gtk_widget_show (GTK_WIDGET (entry->widget));
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (entry->widget),
                      1, 2, idx, idx + 1,
                      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
                      (GtkAttachOptions) (GTK_FILL), 4, 4);
    g_object_set_data (G_OBJECT (GTK_COMBO (entry->widget)->entry),
                       DATA_POINTER_KEY,
                       (gpointer) candidates_p);

    g_signal_connect ((gpointer) GTK_COMBO (entry->widget)->entry, "changed",
                      G_CALLBACK (on_default_combo_changed),
                      entry);

    if (!__widget_tooltips)
        __widget_tooltips = gtk_tooltips_new();
    if (entry->tooltip)
        gtk_tooltips_set_tip (__widget_tooltips,
                              GTK_WIDGET (GTK_COMBO (entry->widget)->entry),
                              _(entry->tooltip), NULL);

    return GTK_WIDGET (entry->widget);
}

GtkWidget *
create_option_menu (const char *config_key, gpointer candidates_p,
                    GtkTable *table, gint idx)
{
    StringConfigData *entry = find_string_config_entry (config_key);
    ComboConfigCandidate *data = static_cast<ComboConfigCandidate*> (candidates_p);
    if (!entry)
        return NULL;

    GtkWidget *label;
    label = gtk_label_new_with_mnemonic (_(entry->label));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_misc_set_padding (GTK_MISC (label), 4, 0);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, idx, idx + 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 4, 4);
    gtk_widget_show (label);

    entry->widget = gtk_option_menu_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                   GTK_WIDGET (entry->widget));
    gtk_widget_show (GTK_WIDGET (entry->widget));
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (entry->widget),
                      1, 2, idx, idx + 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 4, 4);
    g_object_set_data (G_OBJECT (entry->widget),
                       DATA_POINTER_KEY,
                       (gpointer) candidates_p);

    GtkWidget *menu = gtk_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (entry->widget), menu);
    gtk_widget_show (menu);

    for (unsigned int i = 0; data[i].label; i++) {
        GtkWidget *menuitem = gtk_menu_item_new_with_label (_(data[i].label));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        gtk_widget_show (menuitem);
    }

    gtk_option_menu_set_history (GTK_OPTION_MENU (entry->widget), 0);

    g_signal_connect ((gpointer) GTK_OPTION_MENU (entry->widget), "changed",
                      G_CALLBACK (on_default_option_menu_changed),
                      entry);

    if (!__widget_tooltips)
        __widget_tooltips = gtk_tooltips_new();
    if (entry->tooltip)
        gtk_tooltips_set_tip (__widget_tooltips, GTK_WIDGET (entry->widget),
                              _(entry->tooltip), NULL);

    return GTK_WIDGET (entry->widget);
}

GtkWidget *
create_key_select_button (const char *config_key, GtkTable *table, int idx)
{
    StringConfigData *entry = find_string_config_entry (config_key);
    if (!entry)
        return NULL;

    GtkWidget *button = gtk_button_new_with_label ("...");
    gtk_widget_show (button);
    gtk_table_attach (GTK_TABLE (table), button, 2, 3, idx, idx + 1,
                      GTK_FILL, GTK_FILL, 4, 4);
    g_signal_connect ((gpointer) button, "clicked",
                      G_CALLBACK (on_default_key_selection_clicked),
                      entry);

    return button;
}

GtkWidget *
create_color_button (const char *config_key)
{
    ColorConfigData *entry = find_color_config_entry (config_key);
    if (!entry)
        return NULL;

    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_widget_show (hbox);

    GtkWidget *label = NULL;
    if (entry->label) {
        label = gtk_label_new_with_mnemonic (_(entry->label));
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
        gtk_widget_show (label);
    }

    entry->widget = scim_anthy_color_button_new ();
    gtk_widget_set_size_request (GTK_WIDGET (entry->widget), 32, 24);
    g_signal_connect (G_OBJECT (entry->widget), "color-changed",
                      G_CALLBACK (on_color_button_changed),
                      entry);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (entry->widget),
                        FALSE, FALSE, 2);
    gtk_widget_show (GTK_WIDGET (entry->widget));

    if (label)
        gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                       GTK_WIDGET (entry->widget));

    if (!__widget_tooltips)
        __widget_tooltips = gtk_tooltips_new();
    if (entry->tooltip)
        gtk_tooltips_set_tip (__widget_tooltips, GTK_WIDGET (entry->widget),
                              _(entry->tooltip), NULL);

    return hbox;
}

static void
append_key_bindings (GtkTreeView *treeview, gint idx, const gchar *filter)
{
    GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (treeview));
    KeyEventList keys1, keys2;
    
    if (filter && *filter)
        scim_string_to_key_list (keys1, filter);

    if (idx < 0 || idx >= (gint) __key_conf_pages_num)
        return;

    for (unsigned int i = 0; __key_conf_pages[idx].data[i].key; i++) {
        if (filter && *filter) {
            scim_string_to_key_list (
                keys2,
                __key_conf_pages[idx].data[i].value.c_str());
            KeyEventList::const_iterator kit;
            bool found = true;
            for (kit = keys1.begin (); kit != keys1.end (); kit++) {
                if (!util_match_key_event (keys2, *kit)) {
                    found = false;
                    break;
                }
            }
            if (!found)
                continue;
        }

        GtkTreeIter iter;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (
            store, &iter,
            COLUMN_LABEL, _(__key_conf_pages[idx].data[i].label),
            COLUMN_VALUE, __key_conf_pages[idx].data[i].value.c_str (),
            COLUMN_DESC,  _(__key_conf_pages[idx].data[i].tooltip),
            COLUMN_DATA, &__key_conf_pages[idx].data[i],
            -1);
    }
}

static void
key_list_view_popup_key_selection (GtkTreeView *treeview)
{
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkTreePath *treepath = NULL;
    GtkTreeIter iter;

    gtk_tree_view_get_cursor (treeview, &treepath, NULL);
    if (!treepath) return;
    gtk_tree_model_get_iter (model, &iter, treepath);
    gtk_tree_path_free (treepath);

    StringConfigData *data;
    gtk_tree_model_get (model, &iter,
                        COLUMN_DATA, &data,
                        -1);
    if (data) {
        GtkWidget *dialog = scim_key_selection_dialog_new (_(data->title));
        gint result;

        scim_key_selection_dialog_set_keys
            (SCIM_KEY_SELECTION_DIALOG (dialog),
             data->value.c_str());

        result = gtk_dialog_run (GTK_DIALOG (dialog));

        if (result == GTK_RESPONSE_OK) {
            const gchar *keys = scim_key_selection_dialog_get_keys
                (SCIM_KEY_SELECTION_DIALOG (dialog));

            if (!keys) keys = "";

            if (strcmp (keys, data->value.c_str())) {
                data->value = keys;
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                    COLUMN_VALUE, data->value.c_str(),
                                    -1);
                gtk_option_menu_set_history (
                    GTK_OPTION_MENU (__widget_key_theme_menu),
                    KEY_THEME_INDEX_USER_DEFINED);
                data->changed = true;
                __config_changed = true;
            }
        }

        gtk_widget_destroy (dialog);
    }
}

static GtkWidget *
create_common_page (void)
{
    GtkWidget *vbox, *table, *widget;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    table = gtk_table_new (7, 2, FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
    gtk_widget_show (table);

    /* input mode */
    widget = create_combo (SCIM_ANTHY_CONFIG_INPUT_MODE,
                           (gpointer) &input_modes,
                           GTK_TABLE (table), 0);

    /* typing method */
    widget = create_combo (SCIM_ANTHY_CONFIG_TYPING_METHOD,
                           (gpointer) &typing_methods,
                           GTK_TABLE (table), 1);

    /* conversion mode */
    widget = create_combo (SCIM_ANTHY_CONFIG_CONVERSION_MODE,
                           (gpointer) &conversion_modes,
                           GTK_TABLE (table), 2);

    /* behavior on focus out */
    widget = create_combo (SCIM_ANTHY_CONFIG_BEHAVIOR_ON_FOCUS_OUT,
                           (gpointer) &behavior_on_focus_out,
                           GTK_TABLE (table), 3);

    return vbox;
}

static GtkWidget *
create_symbols_page (void)
{
    GtkWidget *vbox, *table, *widget;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    table = gtk_table_new (7, 2, FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
    gtk_widget_show (table);

    /* period style */
    widget = create_combo (SCIM_ANTHY_CONFIG_PERIOD_STYLE,
                           (gpointer) &period_styles,
                           GTK_TABLE (table), 3);

    /* symbol style */
    widget = create_combo (SCIM_ANTHY_CONFIG_SYMBOL_STYLE,
                           (gpointer) &symbol_styles,
                           GTK_TABLE (table), 4);

    /* space_style */
    widget = create_combo (SCIM_ANTHY_CONFIG_SPACE_TYPE,
                           (gpointer) &space_types,
                           GTK_TABLE (table), 5);

    /* ten key_style */
    widget = create_combo (SCIM_ANTHY_CONFIG_TEN_KEY_TYPE,
                           (gpointer) &ten_key_types,
                           GTK_TABLE (table), 6);

    /* behavior on period */
    widget = create_combo (SCIM_ANTHY_CONFIG_BEHAVIOR_ON_PERIOD,
                           (gpointer) &behavior_on_period,
                           GTK_TABLE (table), 7);

    return vbox;
}

static GtkWidget *
create_keyboard_page (void)
{
    GtkWidget *vbox, *hbox;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    // category menu
    GtkWidget *label = gtk_label_new_with_mnemonic (_("_Group:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
    gtk_widget_show (label);

    GtkWidget *omenu = gtk_option_menu_new ();
    __widget_key_categories_menu = omenu;
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 2);
    gtk_widget_show (omenu);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label), omenu);

    GtkWidget *menu = gtk_menu_new ();

    GtkWidget *menuitem;

    for (unsigned int i = 0; i < __key_conf_pages_num; i++) {
        menuitem = gtk_menu_item_new_with_label (_(__key_conf_pages[i].label));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        gtk_widget_show (menuitem);
    }

    menuitem = gtk_menu_item_new_with_label (_("Search by key"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);

    menuitem = gtk_menu_item_new_with_label (_("all"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);

    gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
    gtk_widget_show (menu);

    GtkWidget *entry = gtk_entry_new ();
    __widget_key_filter = entry;
    gtk_entry_set_editable (GTK_ENTRY (entry), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 2);
    gtk_widget_show(entry);

    GtkWidget *button = gtk_button_new_with_label ("...");
    __widget_key_filter_button = button;
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_key_filter_selection_clicked), entry);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    gtk_widget_show (button);

    // key bindings view
    GtkWidget *scrwin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrwin),
                                         GTK_SHADOW_IN);
    gtk_container_set_border_width (GTK_CONTAINER (scrwin), 4);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (vbox), scrwin, TRUE, TRUE, 2);
    gtk_widget_show (scrwin);

    GtkListStore *store = gtk_list_store_new (N_COLUMNS,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_POINTER);
    GtkWidget *treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    __widget_key_list_view = treeview;
    gtk_container_add (GTK_CONTAINER (scrwin), treeview);
    gtk_widget_show (treeview);

    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Feature"), cell,
                                                       "text", COLUMN_LABEL,
                                                       NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width (column, 120);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Key bindings"), cell,
                                                       "text", COLUMN_VALUE,
                                                       NULL);
    gtk_tree_view_column_set_fixed_width (column, 200);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Description"), cell,
                                                       "text", COLUMN_DESC,
                                                       NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    GtkTreeSelection *selection;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    // connect signals
    g_signal_connect (G_OBJECT (omenu), "changed",
                      G_CALLBACK (on_key_category_menu_changed), treeview);
    g_signal_connect (G_OBJECT (treeview), "key-press-event",
                      G_CALLBACK (on_key_list_view_key_press), treeview);
    g_signal_connect (G_OBJECT (treeview), "button-press-event",
                      G_CALLBACK (on_key_list_view_button_press), treeview);
    g_signal_connect (G_OBJECT (selection), "changed",
                      G_CALLBACK (on_key_list_selection_changed), treeview);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    // for key bind theme
    label = gtk_label_new_with_mnemonic (_("Key bindings _theme:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
    gtk_widget_show (label);

    omenu = gtk_option_menu_new ();
    __widget_key_theme_menu = omenu;
    g_signal_connect (G_OBJECT (omenu), "changed",
                      G_CALLBACK (on_key_theme_menu_changed), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 2);
    gtk_widget_show (omenu);

    gtk_label_set_mnemonic_widget (GTK_LABEL(label), omenu);

    // edit button
    button = gtk_button_new_with_mnemonic (_("_Choose keys..."));
    __widget_choose_keys_button = button;
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_choose_keys_button_clicked), treeview);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 2);
    gtk_widget_set_sensitive (button, false);
    gtk_widget_show (button);

    // clean
    g_object_unref (G_OBJECT (store));

    return vbox;
}

static GtkWidget *
create_learning_page ()
{
    GtkWidget *vbox, *vbox2, *hbox, *alignment, *table;
    GtkWidget *widget, *label;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
    gtk_widget_show (hbox);

    label = gtk_label_new (_("<b>Enable/Disable learning</b>"));
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
    gtk_widget_show (label);

    alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 24, 0);
    gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
    gtk_widget_show (alignment);

    vbox2 = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (alignment), vbox2);
    gtk_widget_show (vbox2);

    /* maual commit */
    widget = create_check_button (SCIM_ANTHY_CONFIG_LEARN_ON_MANUAL_COMMIT);
    gtk_box_pack_start (GTK_BOX (vbox2), widget, FALSE, FALSE, 4);

    /* auto commit */
    widget = create_check_button (SCIM_ANTHY_CONFIG_LEARN_ON_AUTO_COMMIT);
    gtk_box_pack_start (GTK_BOX (vbox2), widget, FALSE, FALSE, 4);

    /* key preference */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
    gtk_widget_show (hbox);

    label = gtk_label_new (_("<b>Key preferences to commit "
                             "with reversing learning preference</b>"));
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
    gtk_widget_show (label);

    alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 24, 0);
    gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
    gtk_widget_show (alignment);

    table = gtk_table_new (3, 3, FALSE);
    gtk_container_add (GTK_CONTAINER (alignment), table);
    gtk_widget_show (table);

    StringConfigData *entries[3];
    entries [0] = find_string_config_entry (
        SCIM_ANTHY_CONFIG_COMMIT_REVERSE_LEARN_KEY);
    entries [1] = find_string_config_entry (
        SCIM_ANTHY_CONFIG_COMMIT_FIRST_SEGMENT_REVERSE_LEARN_KEY);
    entries [2] = find_string_config_entry (
        SCIM_ANTHY_CONFIG_COMMIT_SELECTED_SEGMENT_REVERSE_LEARN_KEY);

    for (unsigned int i = 0;
         i < sizeof (entries) / sizeof (StringConfigData*);
         i++)
    {
        StringConfigData *entry = entries[i];
        widget = create_entry (entry->key, GTK_TABLE (table), i);
        gtk_entry_set_editable (GTK_ENTRY (widget), FALSE);
        create_key_select_button (entry->key, GTK_TABLE (table), i);
    }

    return vbox;
}

static GtkWidget *
create_prediction_page ()
{
    GtkWidget *vbox, *widget;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    /* predict while inputting */
    widget = create_check_button (SCIM_ANTHY_CONFIG_PREDICT_ON_INPUT);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 4);

    /* use direct select keys */
    widget = create_check_button (SCIM_ANTHY_CONFIG_USE_DIRECT_KEY_ON_PREDICT);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 4);

    return vbox;
}

static GtkWidget *
create_dict_page (void)
{
    GtkWidget *table, *button;
    StringConfigData *entry;

    table = gtk_table_new (3, 3, FALSE);
    gtk_widget_show (table);

    // encoding of dictionary
    create_combo (SCIM_ANTHY_CONFIG_DICT_ENCODING,
                  (gpointer) &dict_encoding,
                  GTK_TABLE (table), 0);

    // dict admin command
    create_entry (SCIM_ANTHY_CONFIG_DICT_ADMIN_COMMAND,
                  GTK_TABLE (table), 1);
    entry = find_string_config_entry (SCIM_ANTHY_CONFIG_DICT_ADMIN_COMMAND);

    button = gtk_button_new_with_mnemonic (_("_Launch"));
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (button),
                      2, 3, 1, 2,
                      (GtkAttachOptions) 0,
                      (GtkAttachOptions) 0, 4, 4);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_dict_launch_button_clicked), entry);
    gtk_widget_show (button);

    // add word command
    create_entry (SCIM_ANTHY_CONFIG_ADD_WORD_COMMAND,
                  GTK_TABLE (table), 2);
    entry = find_string_config_entry (SCIM_ANTHY_CONFIG_ADD_WORD_COMMAND);

    button = gtk_button_new_with_mnemonic (_("_Launch"));
    gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (button),
                      2, 3, 2, 3,
                      (GtkAttachOptions) 0,
                      (GtkAttachOptions) 0, 4, 4);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_dict_launch_button_clicked), entry);
    gtk_widget_show (button);

    return table;
}

static GtkWidget *
create_candidates_window_page (void)
{
    GtkWidget *vbox, *widget, *table;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    /* show candidates label */
    widget = create_check_button (SCIM_ANTHY_CONFIG_SHOW_CANDIDATES_LABEL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 2);

    /* close candidate window on select */
    widget = create_check_button (SCIM_ANTHY_CONFIG_CLOSE_CAND_WIN_ON_SELECT);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 2);

    table = gtk_table_new (2, 2, FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
    gtk_widget_show (table);

    /* number of triggers until show candidates window */
    create_spin_button (SCIM_ANTHY_CONFIG_CAND_WIN_PAGE_SIZE,
                        GTK_TABLE (table), 0);

    /* number of triggers until show candidates window */
    create_spin_button (SCIM_ANTHY_CONFIG_N_TRIGGERS_TO_SHOW_CAND_WIN,
                        GTK_TABLE (table), 1);

    return vbox;
}

static GtkWidget *
create_toolbar_page (void)
{
    GtkWidget *vbox, *hbox, *label, *widget;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    /* show/hide toolbar label */
    widget = create_check_button (SCIM_ANTHY_CONFIG_SHOW_INPUT_MODE_LABEL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 2);

    widget = create_check_button (SCIM_ANTHY_CONFIG_SHOW_TYPING_METHOD_LABEL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 2);

    widget = create_check_button (SCIM_ANTHY_CONFIG_SHOW_CONVERSION_MODE_LABEL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 2);

    widget = create_check_button (SCIM_ANTHY_CONFIG_SHOW_PERIOD_STYLE_LABEL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 2);

    widget = create_check_button (SCIM_ANTHY_CONFIG_SHOW_SYMBOL_STYLE_LABEL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 2);

    /* dictionary menu */
    widget = create_check_button (SCIM_ANTHY_CONFIG_SHOW_DICT_LABEL);
    g_signal_connect ((gpointer) widget, "toggled",
                      G_CALLBACK (on_dict_menu_label_toggled),
                      NULL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 2);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 2);
    gtk_widget_show (hbox);
    label = gtk_label_new ("    ");
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    gtk_widget_show (label);
    widget = create_check_button (SCIM_ANTHY_CONFIG_SHOW_DICT_ADMIN_LABEL);
    gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

    gtk_widget_show (label);
    widget = create_check_button (SCIM_ANTHY_CONFIG_SHOW_ADD_WORD_LABEL);
    gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

    // set initial state
    on_dict_menu_label_toggled (GTK_TOGGLE_BUTTON (widget), NULL);

    return vbox;
}

static GtkWidget *
create_appearance_page (void)
{
    GtkWidget *vbox, *table, *omenu, *widget, *hbox; 

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    table = gtk_table_new (2, 3, FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
    gtk_widget_show (table);

    /* preedit style */
    omenu = create_option_menu (SCIM_ANTHY_CONFIG_PREEDIT_STYLE,
                                &preedit_style, GTK_TABLE (table), 0);

    /* preedit color */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox);
    widget = create_color_button (SCIM_ANTHY_CONFIG_PREEDIT_FG_COLOR);
    gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
    gtk_table_attach (GTK_TABLE (table), hbox, 2, 3, 0, 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 4, 4);
    gtk_widget_set_sensitive (hbox, FALSE);

    g_signal_connect ((gpointer) GTK_OPTION_MENU (omenu), "changed",
                      G_CALLBACK (on_preedit_style_menu_changed), hbox);

    /* conversion style */
    omenu = create_option_menu (SCIM_ANTHY_CONFIG_CONVERSION_STYLE,
                                &preedit_style, GTK_TABLE (table), 1);

    /* conversion color */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox);
    widget = create_color_button (SCIM_ANTHY_CONFIG_CONVERSION_FG_COLOR);
    gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
    gtk_table_attach (GTK_TABLE (table), hbox, 2, 3, 1, 2,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 4, 4);
    gtk_widget_set_sensitive (hbox, FALSE);

    g_signal_connect ((gpointer) GTK_OPTION_MENU (omenu), "changed",
                      G_CALLBACK (on_preedit_style_menu_changed), hbox);

    /* selected segment style */
    omenu = create_option_menu (SCIM_ANTHY_CONFIG_SELECTED_SEGMENT_STYLE,
                                &preedit_style, GTK_TABLE (table), 2);

    /* selected segment color */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox);
    widget = create_color_button (SCIM_ANTHY_CONFIG_SELECTED_SEGMENT_FG_COLOR);
    gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
    gtk_table_attach (GTK_TABLE (table), hbox, 2, 3, 2, 3,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 4, 4);
    gtk_widget_set_sensitive (hbox, FALSE);

    g_signal_connect ((gpointer) GTK_OPTION_MENU (omenu), "changed",
                      G_CALLBACK (on_preedit_style_menu_changed), hbox);

    return vbox;
}

static GtkWidget *
create_about_page ()
{
    GtkWidget *vbox, *label;
    gchar str[256];

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    g_snprintf (
        str, 256,
        _("<span size=\"20000\">"
          "%s-%s"
          "</span>\n\n"

          "<span size=\"16000\" style=\"italic\">"
          "A Japanese input method module\nfor SCIM using Anthy"
          "</span>\n\n\n\n"

          "<span size=\"12000\">"
          "Copyright 2005-2006, Takuro Ashie &lt;ashie@homa.ne.jp&gt;"
          "</span>"),
        PACKAGE, PACKAGE_VERSION);

    label = gtk_label_new (NULL);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_label_set_markup (GTK_LABEL (label), str);
    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
    gtk_widget_show (label);

    return vbox;
}

static GtkWidget *
create_setup_window (void)
{
    static GtkWidget *window = NULL;

    if (!window) {
        GtkWidget *notebook = gtk_notebook_new();
        gtk_notebook_popup_enable(GTK_NOTEBOOK(notebook));
        gtk_widget_show (notebook);
        window = notebook;
        gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);

        // Create the common page.
        GtkWidget *page = create_common_page ();
        GtkWidget *label = gtk_label_new (_("Common"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the symbols page.
        page = create_symbols_page ();
        label = gtk_label_new (_("Symbols"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the key bind page.
        page = create_keyboard_page ();
        label = gtk_label_new (_("Key bindings"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the romaji page.
        page = romaji_page_create_ui ();
        label = gtk_label_new (_("Romaji typing"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the kana page.
        page = kana_page_create_ui ();
        label = gtk_label_new (_("Kana typing"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the learning page.
        page = create_prediction_page ();
        label = gtk_label_new (_("Prediction"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the learning page.
        page = create_learning_page ();
        label = gtk_label_new (_("Learning"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the dictionary page.
        page = create_dict_page ();
        label = gtk_label_new (_("Dictionary"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the candidates widnow page.
        page = create_candidates_window_page ();
        label = gtk_label_new (_("Candidates window"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the toolbar page.
        page = create_toolbar_page ();
        label = gtk_label_new (_("Toolbar"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the appearance  page.
        page = create_appearance_page ();
        label = gtk_label_new (_("Appearance"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the appearance  page.
        page = create_about_page ();
        label = gtk_label_new (_("About"));
        gtk_widget_show (label);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // for preventing enabling left arrow.
        gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 1);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);

        setup_widget_value ();
    }

    return window;
}

static void
setup_combo_value (GtkCombo *combo, const String & str)
{
    GList *list = NULL;
    const char *defval = NULL;

    ComboConfigCandidate *data
        = static_cast<ComboConfigCandidate*>
        (g_object_get_data (G_OBJECT (GTK_COMBO(combo)->entry),
                            DATA_POINTER_KEY));

    for (unsigned int i = 0; data[i].label; i++) {
        list = g_list_append (list, (gpointer) _(data[i].label));
        if (!strcmp (data[i].data, str.c_str ()))
            defval = _(data[i].label);
    }

    gtk_combo_set_popdown_strings (combo, list);
    g_list_free (list);

    if (defval)
        gtk_entry_set_text (GTK_ENTRY (combo->entry), defval);
}

static void
setup_option_menu_value (GtkOptionMenu *omenu, const String & str)
{
    ComboConfigCandidate *data
        = static_cast<ComboConfigCandidate*>
        (g_object_get_data (G_OBJECT (omenu), DATA_POINTER_KEY));

    for (unsigned int i = 0; data[i].label; i++) {
        if (!strcmp (data[i].data, str.c_str ())) {
            gtk_option_menu_set_history (omenu, i);
            return;
        }
    }
}

static void
setup_key_theme_menu (GtkOptionMenu *omenu)
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
        const char *section_name = "KeyBindings";
        StyleLines section;
        if (!it->get_entry_list (section, section_name))
            continue;

        menuitem = gtk_menu_item_new_with_label (_(it->get_title().c_str()));
        g_object_set_data (G_OBJECT (menuitem),
                           INDEX_KEY, GINT_TO_POINTER (i));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        gtk_widget_show (menuitem);
    }

    // set default value
    g_signal_handlers_block_by_func (G_OBJECT (omenu),
                                     (gpointer) (on_key_theme_menu_changed),
                                     NULL);

    gtk_option_menu_set_history (GTK_OPTION_MENU (omenu),
                                 KEY_THEME_INDEX_DEFAULT);

    if (__config_key_theme_file == __user_style_file.get_file_name () ||
        __config_key_theme      == __user_style_file.get_title ())
    {
        gtk_option_menu_set_history (GTK_OPTION_MENU (omenu),
                                     KEY_THEME_INDEX_USER_DEFINED);

    } else {
        GList *node, *list = gtk_container_get_children (GTK_CONTAINER (menu));
        for (i = 2, node = g_list_next (g_list_next (list));
             node;
             i++, node = g_list_next (node))
        {
            gint idx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (node->data),
                                                           INDEX_KEY));
            if (__style_list[idx].get_file_name () == __config_key_theme_file) {
                gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), i);
                break;
            }
        }
    }

    g_signal_handlers_unblock_by_func (G_OBJECT (omenu),
                                       (gpointer) (on_key_theme_menu_changed),
                                       NULL);
}

static void
setup_widget_value (void)
{
    for (unsigned int i = 0; config_bool_common[i].key; i++) {
        BoolConfigData &entry = config_bool_common[i];
        if (entry.widget)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (entry.widget),
                                          entry.value);
    }

    for (unsigned int i = 0; config_int_common[i].key; i++) {
        IntConfigData &entry = config_int_common[i];
        if (entry.widget)
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (entry.widget),
                                       entry.value);
    }

    for (unsigned int i = 0; config_string_common[i].key; i++) {
        StringConfigData &entry = config_string_common[i];
        if (entry.widget && GTK_IS_OPTION_MENU (entry.widget)) {
            setup_option_menu_value (GTK_OPTION_MENU (entry.widget),
                                     entry.value);
        } else if (entry.widget && GTK_IS_COMBO (entry.widget)) {
            setup_combo_value (GTK_COMBO (entry.widget), entry.value);
        } else if (entry.widget && GTK_IS_ENTRY (entry.widget)) {
            gtk_entry_set_text (GTK_ENTRY (entry.widget),
                                entry.value.c_str ());
        }
    }

    for (unsigned int j = 0; j < __key_conf_pages_num; j++) {
        for (unsigned int i = 0; __key_conf_pages[j].data[i].key; i++) {
            if (__key_conf_pages[j].data[i].widget) {
                gtk_entry_set_text (
                    GTK_ENTRY (__key_conf_pages[j].data[i].widget),
                    __key_conf_pages[j].data[i].value.c_str ());
            }
        }
    }
    
    for (unsigned int i = 0; config_color_common[i].fg_key; i++) {
        ColorConfigData &entry = config_color_common[i];
        if (entry.widget) {
            scim_anthy_color_button_set_colors (SCIM_ANTHY_COLOR_BUTTON (entry.widget),
                                                entry.fg_value, entry.bg_value);
        }
    }

    gtk_option_menu_set_history
        (GTK_OPTION_MENU (__widget_key_categories_menu),
         KEY_CATEGORY_INDEX_ALL);
    GtkTreeView *treeview = GTK_TREE_VIEW (__widget_key_list_view);
    GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (treeview));
    gtk_list_store_clear (store);
    for (unsigned int i = 0; i < __key_conf_pages_num; i++)
        append_key_bindings (treeview, i, NULL);
    gtk_widget_set_sensitive (__widget_key_filter, FALSE);
    gtk_widget_set_sensitive (__widget_key_filter_button, FALSE);

    // setup option menu
    setup_key_theme_menu (GTK_OPTION_MENU (__widget_key_theme_menu));
}

bool operator < (const StyleFile &left, const StyleFile &right)
{
    if (const_cast<StyleFile&>(left).get_title() <
        const_cast<StyleFile&>(right).get_title())
    {
        return true;
    }
    return false;
}

bool operator > (const StyleFile &left, const StyleFile &right)
{
    if (const_cast<StyleFile&>(left).get_title() >
        const_cast<StyleFile&>(right).get_title())
    {
        return true;
    }
    return false;
}

static void
load_style_files (const char *dirname)
{
    GDir *dir;
    GError *error = NULL;
    const gchar *entry;

    // load system wide style files
    dir = g_dir_open (dirname, 0, &error);
    if (error)
    {
        //g_warning ("%s", error->message);
        g_error_free (error);
    }

    if (dir) {
        while ((entry = g_dir_read_name (dir)))
        {
            String file = dirname;
            file += SCIM_PATH_DELIM_STRING;
            file += entry;

            // FIXME! check duplicates
            __style_list.push_back (StyleFile ());
            StyleFile &style = __style_list.back ();
            bool success = style.load (file.c_str ());
            if (!success)
                __style_list.pop_back ();
        }
        g_dir_close (dir);
    }
    std::sort(__style_list.begin(), __style_list.end());
}

static void
load_config (const ConfigPointer &config)
{
    if (config.null ())
        return;

    __style_list.clear ();

    load_style_files (SCIM_ANTHY_STYLEDIR);
    load_style_files (__user_style_dir_name.c_str ());

    __user_style_file.load (__user_style_file_name.c_str ());

    __config_key_theme
        = config->read (String (SCIM_ANTHY_CONFIG_KEY_THEME),
                        String (SCIM_ANTHY_CONFIG_KEY_THEME_DEFAULT));
    __config_key_theme_file
        = config->read (String (SCIM_ANTHY_CONFIG_KEY_THEME_FILE),
                        String (SCIM_ANTHY_CONFIG_KEY_THEME_FILE_DEFAULT));

    for (unsigned int i = 0; config_bool_common[i].key; i++) {
        BoolConfigData &entry = config_bool_common[i];
        entry.value = config->read (String (entry.key), entry.value);
    }

    for (unsigned int i = 0; config_int_common[i].key; i++) {
        IntConfigData &entry = config_int_common[i];
        entry.value = config->read (String (entry.key), entry.value);
    }

    for (unsigned int i = 0; config_string_common[i].key; i++) {
        StringConfigData &entry = config_string_common[i];
        entry.value = config->read (String (entry.key), entry.value);
    }

    for (unsigned int j = 0; j < __key_conf_pages_num; j++) {
        for (unsigned int i = 0; __key_conf_pages[j].data[i].key; i++) {
            __key_conf_pages[j].data[i].value =
                config->read (String (__key_conf_pages[j].data[i].key),
                              __key_conf_pages[j].data[i].value);
        }
    }

    for (unsigned int i = 0; config_color_common[i].fg_key; i++) {
        ColorConfigData &entry = config_color_common[i];
        entry.fg_value = config->read (String (entry.fg_key), entry.fg_value);
        entry.bg_value = config->read (String (entry.bg_key), entry.bg_value);
    }

    romaji_page_load_config (config);
    kana_page_load_config (config);

    setup_widget_value ();

    for (unsigned int i = 0; config_bool_common[i].key; i++)
        config_bool_common[i].changed = false;

    for (unsigned int i = 0; config_int_common[i].key; i++)
        config_int_common[i].changed = false;

    for (unsigned int i = 0; config_string_common[i].key; i++)
        config_string_common[i].changed = false;

    for (unsigned int j = 0; j < __key_conf_pages_num; j++) {
        for (unsigned int i = 0; __key_conf_pages[j].data[i].key; i++)
            __key_conf_pages[j].data[i].changed = false;
    }

    for (unsigned int i = 0; config_color_common[i].fg_key; i++)
        config_color_common[i].changed = false;

    __config_changed = false;
}

static void
save_config (const ConfigPointer &config)
{
    if (config.null ())
        return;

    __config_key_theme
        = config->write (String (SCIM_ANTHY_CONFIG_KEY_THEME),
                         String (__config_key_theme));
    __config_key_theme_file
        = config->write (String (SCIM_ANTHY_CONFIG_KEY_THEME_FILE),
                         String (__config_key_theme_file));

    for (unsigned int i = 0; config_bool_common[i].key; i++) {
        BoolConfigData &entry = config_bool_common[i];
        if (entry.changed)
            entry.value = config->write (String (entry.key), entry.value);
        entry.changed = false;
    }

    for (unsigned int i = 0; config_int_common[i].key; i++) {
        IntConfigData &entry = config_int_common[i];
        if (entry.changed)
            entry.value = config->write (String (entry.key), entry.value);
        entry.changed = false;
    }

    for (unsigned int i = 0; config_string_common[i].key; i++) {
        StringConfigData &entry = config_string_common[i];
        if (entry.changed)
            entry.value = config->write (String (entry.key), entry.value);
        entry.changed = false;
    }

    for (unsigned int j = 0; j < __key_conf_pages_num; j++) {
        for (unsigned int i = 0; __key_conf_pages[j].data[i].key; i++) {
            if (__key_conf_pages[j].data[i].changed)
                config->write (String (__key_conf_pages[j].data[i].key),
                               String (__key_conf_pages[j].data[i].value));
            __key_conf_pages[j].data[i].changed = false;
        }
    }

    for (unsigned int i = 0; config_color_common[i].fg_key; i++) {
        ColorConfigData &entry = config_color_common[i];
        if (entry.changed) {
            entry.fg_value = config->write (String (entry.fg_key),
                                            entry.fg_value);
            entry.bg_value = config->write (String (entry.bg_key),
                                            entry.bg_value);
        }
        entry.changed = false;
    }

    __config_changed = false;

    if (__style_changed) {
        scim_make_dir (__user_config_dir_name);
        __user_style_file.save (__user_style_file_name.c_str ());
        __style_changed = false;
    }

    romaji_page_save_config (config);
    kana_page_save_config (config);
}

static bool
query_changed (void)
{
    return
        __config_changed ||
        __style_changed ||
        romaji_page_query_changed () ||
        kana_page_query_changed ();
}


static void
on_default_toggle_button_toggled (GtkToggleButton *togglebutton,
                                  gpointer         user_data)
{
    BoolConfigData *entry = static_cast<BoolConfigData*> (user_data);

    if (entry) {
        entry->value = gtk_toggle_button_get_active (togglebutton);
        entry->changed = true;
        __config_changed = true;
    }
}

static void
on_default_spin_button_changed (GtkSpinButton *spinbutton, gpointer user_data)
{
    IntConfigData *entry = static_cast<IntConfigData*> (user_data);

    if (entry) {
        entry->value = static_cast<int> (gtk_spin_button_get_value (spinbutton));
        entry->changed = true;
        __config_changed = true;
    }
}

static void
on_default_editable_changed (GtkEditable *editable,
                             gpointer     user_data)
{
    StringConfigData *entry = static_cast <StringConfigData*> (user_data);

    if (entry) {
        entry->value = String (gtk_entry_get_text (GTK_ENTRY (editable)));
        entry->changed = true;
        __config_changed = true;
    }
}

static void
on_default_key_selection_clicked (GtkButton *button,
                                  gpointer   user_data)
{
    StringConfigData *data = static_cast <StringConfigData*> (user_data);

    if (data) {
        GtkWidget *dialog = scim_key_selection_dialog_new (_(data->title));
        gint result;

        scim_key_selection_dialog_set_keys (
            SCIM_KEY_SELECTION_DIALOG (dialog),
            gtk_entry_get_text (GTK_ENTRY (data->widget)));

        result = gtk_dialog_run (GTK_DIALOG (dialog));

        if (result == GTK_RESPONSE_OK) {
            const gchar *keys = scim_key_selection_dialog_get_keys (
                            SCIM_KEY_SELECTION_DIALOG (dialog));

            if (!keys) keys = "";

            if (strcmp (keys, gtk_entry_get_text (GTK_ENTRY (data->widget))))
                gtk_entry_set_text (GTK_ENTRY (data->widget), keys);
        }

        gtk_widget_destroy (dialog);
    }
}

static void
on_default_combo_changed (GtkEditable *editable,
                          gpointer user_data)
{
    StringConfigData *entry = static_cast<StringConfigData*> (user_data);
    ComboConfigCandidate *data = static_cast<ComboConfigCandidate*>
        (g_object_get_data (G_OBJECT (editable),
                            DATA_POINTER_KEY));

    if (!entry) return;
    if (!data) return;

    const char *label =  gtk_entry_get_text (GTK_ENTRY (editable));

    for (unsigned int i = 0; data[i].label; i++) {
        if (label && !strcmp (_(data[i].label), label)) {
            entry->value     = data[i].data;
            entry->changed   = true;
            __config_changed = true;
            break;
        }
    }
}

static void
on_default_option_menu_changed (GtkOptionMenu *omenu, gpointer user_data)
{
    StringConfigData *entry = static_cast<StringConfigData*> (user_data);
    ComboConfigCandidate *data = static_cast<ComboConfigCandidate*>
        (g_object_get_data (G_OBJECT (omenu),
                            DATA_POINTER_KEY));

    if (!entry) return;
    if (!data) return;

    for (int i = 0; data[i].label; i++) {
        if (i == gtk_option_menu_get_history (omenu)) {
            entry->value     = data[i].data;
            entry->changed   = true;
            __config_changed = true;
            break;
        }
    }
}

static void
on_preedit_style_menu_changed (GtkOptionMenu *omenu, gpointer user_data)
{
    GtkWidget *widget = GTK_WIDGET (user_data);

    gint idx = gtk_option_menu_get_history (omenu);

    for (int i = 0; preedit_style[i].data && i <= idx; i++) {
        if (i == idx &&
            (!strcmp (preedit_style[i].data, "Color") ||
             !strcmp (preedit_style[i].data, "FGColor") ||
             !strcmp (preedit_style[i].data, "BGColor")))
        {
            gtk_widget_set_sensitive (widget, TRUE);
            return;
        }
    }

    gtk_widget_set_sensitive (widget, FALSE);
}

static void
on_dict_menu_label_toggled (GtkToggleButton *togglebutton,
                            gpointer         user_data)
{
    bool active = gtk_toggle_button_get_active (togglebutton);

    BoolConfigData *entry;
    entry = find_bool_config_entry (SCIM_ANTHY_CONFIG_SHOW_DICT_ADMIN_LABEL);
    if (entry->widget)
        gtk_widget_set_sensitive (GTK_WIDGET (entry->widget), active);
    entry = find_bool_config_entry (SCIM_ANTHY_CONFIG_SHOW_ADD_WORD_LABEL);
    if (entry->widget)
        gtk_widget_set_sensitive (GTK_WIDGET (entry->widget), active);
}

static void
on_key_category_menu_changed (GtkOptionMenu *omenu, gpointer user_data)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (user_data);
    GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (treeview));

    gtk_list_store_clear (store);

    gint idx = gtk_option_menu_get_history (omenu);

    bool use_filter = false;

    if (idx >= 0 && idx < (gint) __key_conf_pages_num) {
        append_key_bindings (treeview, idx, NULL);

    } else if (idx == KEY_CATEGORY_INDEX_SEARCH_BY_KEY) {
        // search by key
        use_filter = true;
        const char *str = gtk_entry_get_text (GTK_ENTRY (__widget_key_filter));
        for (unsigned int i = 0; i < __key_conf_pages_num; i++)
            append_key_bindings (treeview, i, str);

    } else if (idx == KEY_CATEGORY_INDEX_ALL) {
        // all
        for (unsigned int i = 0; i < __key_conf_pages_num; i++)
            append_key_bindings (treeview, i, NULL);
    }

    gtk_widget_set_sensitive (__widget_key_filter,        use_filter);
    gtk_widget_set_sensitive (__widget_key_filter_button, use_filter);
}

static void
on_key_theme_menu_changed (GtkOptionMenu *omenu, gpointer user_data)
{
    gint idx = gtk_option_menu_get_history (omenu);
    GtkWidget *menu = gtk_option_menu_get_menu (omenu);
    GList *list = gtk_container_get_children (GTK_CONTAINER (menu));
    GtkWidget *menuitem = GTK_WIDGET (g_list_nth_data (list, idx));

    if (!menuitem)
        return;

    gint theme_idx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                         INDEX_KEY));

    // clear all key bindings
    if (idx != 0) {
        for (unsigned int j = 0; j < __key_conf_pages_num; j++) {
            for (unsigned int i = 0; __key_conf_pages[j].data[i].key; i++) {
                __key_conf_pages[j].data[i].value = "";
                __key_conf_pages[j].data[i].changed = true;
            }
        }
    }

    // set new key bindings
    if (idx == KEY_THEME_INDEX_USER_DEFINED) {
        __config_key_theme      = String ("User defined");
        __config_key_theme_file = String (""); //__user_style_file.get_file_name ();

    } else if (idx == KEY_THEME_INDEX_DEFAULT) {
        for (unsigned int j = 0; j < __key_conf_pages_num; j++) {
            for (unsigned int i = 0; __key_conf_pages[j].data[i].key; i++) {
                __key_conf_pages[j].data[i].value
                    = __key_conf_pages[j].data[i].default_value;
            }
        }
        __config_key_theme      = String ("Default");
        __config_key_theme_file = String ("");

    } else if (theme_idx >= 0) {
        // reset key bindings
        StyleLines lines;
        StyleLines::iterator it;
        __style_list[theme_idx].get_entry_list (lines, "KeyBindings");
        for (it = lines.begin (); it != lines.end (); it++) {
            if (it->get_type () != SCIM_ANTHY_STYLE_LINE_KEY)
                continue;
            String key, fullkey;
            it->get_key (key);
            fullkey = String ("/IMEngine/Anthy/") + key;
            StringConfigData *entry = find_key_config_entry (fullkey.c_str ());
            if (entry) {
                it->get_value (entry->value);
                entry->changed = true;
            } else {
                std::cerr << "No entry for : " << key << std::endl;
            }
        }
        __config_key_theme      = __style_list[theme_idx].get_title ();
        __config_key_theme_file = __style_list[theme_idx].get_file_name ();
    }

    // sync widgets
    if (idx != KEY_THEME_INDEX_USER_DEFINED) {
        gtk_option_menu_set_history
            (GTK_OPTION_MENU (__widget_key_categories_menu),
             KEY_CATEGORY_INDEX_ALL);
        gtk_widget_set_sensitive (__widget_key_filter, FALSE);
        gtk_widget_set_sensitive (__widget_key_filter_button, FALSE);
        GtkTreeModel *model;
        model = gtk_tree_view_get_model (
            GTK_TREE_VIEW (__widget_key_list_view));
        gtk_list_store_clear (GTK_LIST_STORE (model));
#if 0
        append_key_bindings (GTK_TREE_VIEW (__widget_key_list_view),
                             KEY_CATEGORY_INDEX_ALL, NULL);
#else
        for (unsigned int i = 0; i < __key_conf_pages_num; i++)
            append_key_bindings (GTK_TREE_VIEW (__widget_key_list_view),
                                 i, NULL);
#endif
    }

    __config_changed = true;
}

static void
on_key_filter_selection_clicked (GtkButton *button,
                                 gpointer   user_data)
{
    GtkEntry *entry = static_cast <GtkEntry*> (user_data);

    if (entry) {
        GtkWidget *dialog = scim_key_selection_dialog_new (_("Set key filter"));
        gint result;

        scim_key_selection_dialog_set_keys (
            SCIM_KEY_SELECTION_DIALOG (dialog),
            gtk_entry_get_text (entry));

        result = gtk_dialog_run (GTK_DIALOG (dialog));

        if (result == GTK_RESPONSE_OK) {
            const gchar *keys = scim_key_selection_dialog_get_keys (
                SCIM_KEY_SELECTION_DIALOG (dialog));

            if (!keys) keys = "";

            if (strcmp (keys, gtk_entry_get_text (entry)))
                gtk_entry_set_text (entry, keys);

            GtkTreeModel *model;
            model = gtk_tree_view_get_model (
                GTK_TREE_VIEW (__widget_key_list_view));
            gtk_list_store_clear (GTK_LIST_STORE (model));
            for (unsigned int i = 0; i < __key_conf_pages_num; i++)
                append_key_bindings (GTK_TREE_VIEW (__widget_key_list_view),
                                     i, keys);
        }

        gtk_widget_destroy (dialog);
    }
}

static gboolean
on_key_list_view_key_press (GtkWidget *widget, GdkEventKey *event,
                            gpointer user_data)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (widget);

    switch (event->keyval) {
    case GDK_Return:
    case GDK_KP_Enter:
        key_list_view_popup_key_selection (treeview);
        break;
    }

    return FALSE;
}

static gboolean
on_key_list_view_button_press (GtkWidget *widget, GdkEventButton *event,
                               gpointer user_data)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (widget);

    if (event->type == GDK_2BUTTON_PRESS) {
        key_list_view_popup_key_selection (treeview);
        return TRUE;
    }

    return FALSE;
}

static void
on_key_list_selection_changed (GtkTreeSelection *selection, gpointer data)
{
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;

    gboolean selected;

    selected = gtk_tree_selection_get_selected (selection, &model, &iter);

    if (__widget_choose_keys_button) {
        if (selected) {
            gtk_widget_set_sensitive (__widget_choose_keys_button, true);
        } else {
            gtk_widget_set_sensitive (__widget_choose_keys_button, false);
        }
    }
}

static void
on_choose_keys_button_clicked (GtkWidget *button, gpointer data)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (data);
    key_list_view_popup_key_selection (treeview);
}

static void
on_dict_launch_button_clicked (GtkButton *button, gpointer user_data)
{
    StringConfigData *entry = static_cast <StringConfigData*> (user_data);

    if (entry->widget) {
        const char *command = gtk_entry_get_text (GTK_ENTRY (entry->widget));
        if (command && *command)
            util_launch_program (command);
    }
}

static void
on_color_button_changed (ScimAnthyColorButton *button,
                         gpointer         user_data)
{
    ColorConfigData *entry = static_cast <ColorConfigData*> (user_data);

    if (entry->widget) {
        scim_anthy_color_button_get_colors (button, &entry->fg_value, &entry->bg_value);
        entry->changed = true;
        __config_changed = true;
    }
}

}
/*
vi:ts=4:nowrap:ai:expandtab
*/
