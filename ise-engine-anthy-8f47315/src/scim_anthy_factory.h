/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2004 Hiroyuki Ikezoe
 *  Copyright (C) 2004 Takuro Ashie
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
 * The original code is scim_uim_imengine.cpp in scim-uim-0.1.3. 
 * Copyright (C) 2004 James Su <suzhe@tsinghua.org.cn>
 */

#ifndef __SCIM_ANTHY_FACTORY_H__
#define __SCIM_ANTHY_FACTORY_H__

#define Uses_SCIM_IMENGINE
#define Uses_SCIM_CONFIG_BASE
#include <anthy/anthy.h>
#include <scim.h>
#include "scim_anthy_action.h"
#include "scim_anthy_style_file.h"
#include "scim_anthy_key2kana_table.h"

using namespace scim;
using namespace scim_anthy;

class AnthyFactory : public IMEngineFactoryBase
{
public:
    AnthyFactory (const String &lang,
                  const String &uuid,
                  const ConfigPointer &config);
    virtual ~AnthyFactory ();

    virtual WideString  get_name      () const;
    virtual WideString  get_authors   () const;
    virtual WideString  get_credits   () const;
    virtual WideString  get_help      () const;
    virtual String      get_uuid      () const;
    virtual String      get_icon_file () const;

    virtual IMEngineInstancePointer create_instance (const String& encoding,
                                                     int id = -1);

    virtual void append_config_listener (AnthyInstance *listener);
    virtual void remove_config_listener (AnthyInstance *listener);

private:
    void reload_config (const ConfigPointer &config);

private:
    String                      m_uuid;
    Connection                  m_reload_signal_connection;
    std::vector<AnthyInstance*> m_config_listeners;

public:
    /* config */
    ConfigPointer m_config;

    /* for preferece */
    String         m_input_mode;
    String         m_typing_method;
    String         m_conversion_mode;
    String         m_period_style;
    String         m_symbol_style;
    String         m_space_type;
    String         m_ten_key_type;
    String         m_behavior_on_period;
    String         m_behavior_on_focus_out;

    bool           m_show_candidates_label;
    bool           m_close_cand_win_on_select;
    int            m_cand_win_page_size;
    int            m_n_triggers_to_show_cand_win;

    bool           m_learn_on_manual_commit;
    bool           m_learn_on_auto_commit;

    bool           m_romaji_half_symbol;
    bool           m_romaji_half_number;
    bool           m_romaji_allow_split;

    bool           m_romaji_pseudo_ascii_mode;
    bool           m_romaji_pseudo_ascii_blank_behavior;

    int            m_nicola_time;
    KeyEventList   m_left_thumb_keys;
    KeyEventList   m_right_thumb_keys;

    String         m_dict_encoding;
    String         m_dict_admin_command;
    String         m_add_word_command;

    bool           m_predict_on_input;
    bool           m_use_direct_key_on_predict;

    bool           m_show_input_mode_label;
    bool           m_show_conv_mode_label;
    bool           m_show_typing_method_label;
    bool           m_show_period_style_label;
    bool           m_show_symbol_style_label;
    bool           m_show_dict_label;
    bool           m_show_dict_admin_label;
    bool           m_show_add_word_label;

    String         m_preedit_style;
    unsigned int   m_preedit_fg_color;
    unsigned int   m_preedit_bg_color;
    String         m_conversion_style;
    unsigned int   m_conversion_fg_color;
    unsigned int   m_conversion_bg_color;
    String         m_selected_segment_style;
    unsigned int   m_selected_segment_fg_color;
    unsigned int   m_selected_segment_bg_color;

    Key2KanaTable *m_custom_romaji_table;
    Key2KanaTable *m_custom_kana_table;
    Key2KanaTable *m_custom_nicola_table;

    String         m_kana_layout_ro_key;

    /* for key bindings */
    std::vector<Action> m_actions;
};

#endif /* __SCIM_ANTHY_FACTORY_H__ */
/*
vi:ts=4:nowrap:ai:expandtab
*/
