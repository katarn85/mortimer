/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2004 - 2005 Hiroyuki Ikezoe <poincare@ikezoe.net>
 *  Copyright (C) 2004 - 2005 Takuro Ashie <ashie@homa.ne.jp>
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

#define Uses_SCIM_UTILITY
#define Uses_SCIM_IMENGINE
#define Uses_SCIM_LOOKUP_TABLE
#define Uses_SCIM_CONFIG_BASE

#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include <scim.h>
#include <Ecore_IMF.h>
#include "scim_anthy_factory.h"
#include "scim_anthy_imengine.h"
#include "scim_anthy_prefs.h"
#include "scim_anthy_intl.h"
#include "scim_anthy_utils.h"
#include "scim_anthy_helper.h"

#define SCIM_PROP_PREFIX                     "/IMEngine/Anthy"
#define SCIM_PROP_INPUT_MODE                 "/IMEngine/Anthy/InputMode"
#define SCIM_PROP_INPUT_MODE_HIRAGANA        "/IMEngine/Anthy/InputMode/Hiragana"
#define SCIM_PROP_INPUT_MODE_KATAKANA        "/IMEngine/Anthy/InputMode/Katakana"
#define SCIM_PROP_INPUT_MODE_HALF_KATAKANA   "/IMEngine/Anthy/InputMode/HalfKatakana"
#define SCIM_PROP_INPUT_MODE_LATIN           "/IMEngine/Anthy/InputMode/Latin"
#define SCIM_PROP_INPUT_MODE_WIDE_LATIN      "/IMEngine/Anthy/InputMode/WideLatin"

#define SCIM_PROP_CONV_MODE                  "/IMEngine/Anthy/ConvMode"
#define SCIM_PROP_CONV_MODE_MULTI_SEG        "/IMEngine/Anthy/ConvMode/MultiSegment"
#define SCIM_PROP_CONV_MODE_SINGLE_SEG       "/IMEngine/Anthy/ConvMode/SingleSegment"
#define SCIM_PROP_CONV_MODE_MULTI_REAL_TIME  "/IMEngine/Anthy/ConvMode/MultiRealTime"
#define SCIM_PROP_CONV_MODE_SINGLE_REAL_TIME "/IMEngine/Anthy/ConvMode/SingleRealTime"

#define SCIM_PROP_TYPING_METHOD              "/IMEngine/Anthy/TypingMethod"
#define SCIM_PROP_TYPING_METHOD_ROMAJI       "/IMEngine/Anthy/TypingMethod/RomaKana"
#define SCIM_PROP_TYPING_METHOD_KANA         "/IMEngine/Anthy/TypingMethod/Kana"
#define SCIM_PROP_TYPING_METHOD_NICOLA       "/IMEngine/Anthy/TypingMethod/NICOLA"

#define SCIM_PROP_PERIOD_STYLE               "/IMEngine/Anthy/PeriodType"
#define SCIM_PROP_PERIOD_STYLE_JAPANESE      "/IMEngine/Anthy/PeriodType/Japanese"
#define SCIM_PROP_PERIOD_STYLE_WIDE_LATIN    "/IMEngine/Anthy/PeriodType/WideRatin"
#define SCIM_PROP_PERIOD_STYLE_LATIN         "/IMEngine/Anthy/PeriodType/Ratin"
#define SCIM_PROP_PERIOD_STYLE_WIDE_LATIN_JAPANESE \
                                             "/IMEngine/Anthy/PeriodType/WideRatin_Japanese"

#define SCIM_PROP_SYMBOL_STYLE               "/IMEngine/Anthy/SymbolType"
#define SCIM_PROP_SYMBOL_STYLE_JAPANESE      "/IMEngine/Anthy/SymbolType/Japanese"
#define SCIM_PROP_SYMBOL_STYLE_BRACKET_SLASH "/IMEngine/Anthy/SymbolType/WideBracket_WideSlash"
#define SCIM_PROP_SYMBOL_STYLE_CORNER_BRACKET_SLASH \
                                             "/IMEngine/Anthy/SymbolType/CornerBracket_WideSlash"
#define SCIM_PROP_SYMBOL_STYLE_BRACKET_MIDDLE_DOT \
                                             "/IMEngine/Anthy/SymbolType/WideBracket_MiddleDot"

#define SCIM_PROP_DICT                       "/IMEngine/Anthy/Dictionary"
#define SCIM_PROP_DICT_ADD_WORD              "/IMEngine/Anthy/Dictionary/AddWord"
#define SCIM_PROP_DICT_LAUNCH_ADMIN_TOOL     "/IMEngine/Anthy/Dictionary/LaunchAdminTool"

#define UTF8_BRACKET_CORNER_BEGIN "\xE3\x80\x8C"
#define UTF8_BRACKET_CORNER_END   "\xE3\x80\x8D"
#define UTF8_BRACKET_WIDE_BEGIN   "\xEF\xBC\xBB"
#define UTF8_BRACKET_WIDE_END     "\xEF\xBC\xBD"
#define UTF8_MIDDLE_DOT           "\xE3\x83\xBB"
#define UTF8_SLASH_WIDE           "\xEF\xBC\x8F"

AnthyInstance::AnthyInstance (AnthyFactory   *factory,
                              const String   &encoding,
                              int             id)
    : IMEngineInstanceBase     (factory, encoding, id),
      m_factory                (factory),
      m_on_init                (true),
      m_preedit                (*this),
      m_preedit_string_visible (false),
      m_lookup_table_visible   (false),
      m_n_conv_key_pressed     (0),
      m_prev_input_mode        (SCIM_ANTHY_MODE_HIRAGANA),
      m_conv_mode              (SCIM_ANTHY_CONVERSION_MULTI_SEGMENT),
      m_helper_started         (false),
      m_timeout_id_seq         (0),
      m_layout                 (ECORE_IMF_INPUT_PANEL_LAYOUT_NORMAL)
{
    SCIM_DEBUG_IMENGINE(1) << "Create Anthy Instance : ";

    reload_config (m_factory->m_config);
    m_factory->append_config_listener (this);
    m_on_init = false;
}

AnthyInstance::~AnthyInstance ()
{
    if (m_helper_started)
        stop_helper (String (SCIM_ANTHY_HELPER_UUID));

    m_factory->remove_config_listener (this);
}

// FIXME!
bool
AnthyInstance::is_nicola_thumb_shift_key (const KeyEvent &key)
{
    if (get_typing_method () != SCIM_ANTHY_TYPING_METHOD_NICOLA)
        return false;

    if (util_match_key_event (m_factory->m_left_thumb_keys, key, 0xFFFF) ||
        util_match_key_event (m_factory->m_right_thumb_keys, key, 0xFFFF))
    {
        return true;
    }

    return false;
}

bool
AnthyInstance::process_key_event_input (const KeyEvent &key)
{
    // prediction while typing
    if (m_factory->m_predict_on_input && key.is_key_release () &&
        m_preedit.is_preediting () && !m_preedit.is_converting ())
    {
        CommonLookupTable table;
        m_preedit.predict ();
        m_preedit.get_candidates (table);
        if (table.number_of_candidates () > 0) {
            table.show_cursor (false);
            update_lookup_table (table);
            show_lookup_table ();
        } else {
            hide_lookup_table ();
        }
    }

    if (!m_preedit.can_process_key_event (key)) {
        return false;
    }

    if (m_preedit.is_converting ()) {
        if (is_realtime_conversion ()) {
            action_revert ();
        } else if (!is_nicola_thumb_shift_key (key)) {
            action_commit (m_factory->m_learn_on_auto_commit);
        }
    }

    bool need_commit = m_preedit.process_key_event (key);

    if (need_commit) {
        if (is_realtime_conversion () &&
            get_input_mode () != SCIM_ANTHY_MODE_LATIN &&
            get_input_mode () != SCIM_ANTHY_MODE_WIDE_LATIN)
        {
            m_preedit.convert (SCIM_ANTHY_CANDIDATE_DEFAULT,
                               is_single_segment ());
        }
        action_commit (m_factory->m_learn_on_auto_commit);
    } else {
        if (is_realtime_conversion ()) {
            m_preedit.convert (SCIM_ANTHY_CANDIDATE_DEFAULT,
                               is_single_segment ());
            m_preedit.select_segment (-1);
        }
        show_preedit_string ();
        m_preedit_string_visible = true;
        set_preedition ();
    }

    return true;
}

bool
AnthyInstance::process_key_event_lookup_keybind (const KeyEvent& key)
{
    std::vector<Action>::iterator it;

    m_last_key = key;

    /* try to find a "insert a blank" action to be not stolen a blank key
     * when entering the pseudo ascii mode.
     */
    if (get_pseudo_ascii_mode () != 0 &&
        m_factory->m_romaji_pseudo_ascii_blank_behavior &&
        m_preedit.is_pseudo_ascii_mode ()) {
        for (it  = m_factory->m_actions.begin();
             it != m_factory->m_actions.end();
             it++) {
            if (it->match_action_name ("INSERT_SPACE") &&
                it->perform (this, key)) {
                return true;
            }
        }
    }
    for (it  = m_factory->m_actions.begin();
         it != m_factory->m_actions.end();
         it++)
    {
        if (it->perform (this, key)) {
            m_last_key = KeyEvent ();
            return true;
        }
    }

    m_last_key = KeyEvent ();

    return false;
}

bool
AnthyInstance::process_key_event_latin_mode (const KeyEvent &key)
{
    if (key.is_key_release ())
        return false;

    if (util_key_is_keypad (key)) {
        String str;
        WideString wide;
        util_keypad_to_string (str, key);
        if (m_factory->m_ten_key_type == "Wide")
            util_convert_to_wide (wide, str);
        else
            wide = utf8_mbstowcs (str);
        if (wide.length () > 0) {
            commit_string (wide);
            return true;
        } else {
            return false;
        }
    } else {
        // for Multi/Dead key
        return false;
    }
}

bool
AnthyInstance::process_key_event_wide_latin_mode (const KeyEvent &key)
{
    if (key.is_key_release ())
        return false;

    String str;
    WideString wide;
    util_keypad_to_string (str, key);
    if (util_key_is_keypad (key) && m_factory->m_ten_key_type == "Half")
        wide = utf8_mbstowcs (str);
    else
        util_convert_to_wide (wide, str);
    if (wide.length () > 0) {
        commit_string (wide);
        return true;
    }

    return false;
}

bool
AnthyInstance::process_key_event (const KeyEvent& key)
{
    SCIM_DEBUG_IMENGINE(2) << "process_key_event.\n";

    switch (m_layout) {
        case ECORE_IMF_INPUT_PANEL_LAYOUT_PHONENUMBER:
        case ECORE_IMF_INPUT_PANEL_LAYOUT_IP:
        case ECORE_IMF_INPUT_PANEL_LAYOUT_MONTH:
        case ECORE_IMF_INPUT_PANEL_LAYOUT_NUMBERONLY:
        case ECORE_IMF_INPUT_PANEL_LAYOUT_DATETIME:
            return false;
        default:
            break;
    }

    // FIXME!
    // for NICOLA thumb shift key
    if (get_typing_method () == SCIM_ANTHY_TYPING_METHOD_NICOLA &&
        is_nicola_thumb_shift_key (key))
    {
        if (process_key_event_input (key))
            return true;
    }

    // lookup user defined key bindings
    if (process_key_event_lookup_keybind (key))
        return true;

    // for Latin mode
    if (m_preedit.get_input_mode () == SCIM_ANTHY_MODE_LATIN)
        return process_key_event_latin_mode (key);

    // for wide Latin mode
    if (m_preedit.get_input_mode () == SCIM_ANTHY_MODE_WIDE_LATIN)
        return process_key_event_wide_latin_mode (key);

    // for other mode
    if (get_typing_method () != SCIM_ANTHY_TYPING_METHOD_NICOLA ||
        !is_nicola_thumb_shift_key (key))
    {
        if (process_key_event_input (key))
            return true;
    }

    if (m_preedit.is_preediting ())
        return true;
    else
        return false;
}

void
AnthyInstance::move_preedit_caret (unsigned int pos)
{
    m_preedit.set_caret_pos (pos);
    update_preedit_caret (m_preedit.get_caret_pos());
}

void
AnthyInstance::select_candidate_no_direct (unsigned int item)
{
    SCIM_DEBUG_IMENGINE(2) << "select_candidate_no_direct.\n";

    if (m_preedit.is_predicting () && !m_preedit.is_converting ())
        action_predict ();

    if (!is_selecting_candidates ())
        return;

    // update lookup table
    m_lookup_table.set_cursor_pos_in_current_page (item);
    update_lookup_table (m_lookup_table);

    // update preedit
    m_preedit.select_candidate (m_lookup_table.get_cursor_pos ());
    set_preedition ();

    // update aux string
    if (m_factory->m_show_candidates_label)
        set_aux_string ();
}

void
AnthyInstance::select_candidate (unsigned int item)
{
    SCIM_DEBUG_IMENGINE(2) << "select_candidate.\n";

    select_candidate_no_direct (item);

    if (m_factory->m_close_cand_win_on_select) {
        unset_lookup_table ();
        action_select_next_segment();
    }
}

void
AnthyInstance::update_lookup_table_page_size (unsigned int page_size)
{
    SCIM_DEBUG_IMENGINE(2) << "update_lookup_table_page_size.\n";

    m_lookup_table.set_page_size (page_size);
}

void
AnthyInstance::lookup_table_page_up ()
{
    if (!is_selecting_candidates () ||
        !m_lookup_table.get_current_page_start ())
    {
        return;
    }

    SCIM_DEBUG_IMENGINE(2) << "lookup_table_page_up.\n";

    m_lookup_table.page_up ();

    update_lookup_table (m_lookup_table);
}

void
AnthyInstance::lookup_table_page_down ()
{
    int page_start = m_lookup_table.get_current_page_start ();
    int page_size = m_lookup_table.get_current_page_size ();
    int num = m_lookup_table.number_of_candidates ();

    if (!is_selecting_candidates () || page_start + page_size >= num) 
        return;

    SCIM_DEBUG_IMENGINE(2) << "lookup_table_page_down.\n";

    m_lookup_table.page_down ();

    update_lookup_table (m_lookup_table);
}

void
AnthyInstance::reset ()
{
    SCIM_DEBUG_IMENGINE(2) <<m_factory->m_behavior_on_focus_out<< "reset..\n";

    if (m_preedit.is_preediting ()) {
        if (m_factory->m_behavior_on_focus_out == "Clear")
            flush ();
        else if (m_factory->m_behavior_on_focus_out == "Commit")
            action_commit (m_factory->m_learn_on_auto_commit);
        else
            action_commit (m_factory->m_learn_on_auto_commit);
    }
}

void
AnthyInstance::flush ()
{
    SCIM_DEBUG_IMENGINE(3) << "flush ()\n";
    m_preedit.clear ();
    m_lookup_table.clear ();
    unset_lookup_table ();

    hide_preedit_string ();
    m_preedit_string_visible = false;
    set_preedition ();
}

void
AnthyInstance::focus_in ()
{
    SCIM_DEBUG_IMENGINE(2) << "focus_in.\n";

    if (m_preedit_string_visible) {
        set_preedition ();
        show_preedit_string ();
    } else {
        hide_preedit_string ();
    }

    if (m_lookup_table_visible && is_selecting_candidates ()) {
        if (m_factory->m_show_candidates_label &&
            m_lookup_table.number_of_candidates() > 0)
        {
            set_aux_string ();
            show_aux_string ();
        } else {
            hide_aux_string ();
        }
        update_lookup_table (m_lookup_table);
        show_lookup_table ();
    } else {
        hide_aux_string ();
        hide_lookup_table ();
    }

    install_properties ();

    if (!m_helper_started)
        start_helper (String (SCIM_ANTHY_HELPER_UUID));

    Transaction send;
    send.put_command (SCIM_TRANS_CMD_REQUEST);
    send.put_command (SCIM_TRANS_CMD_FOCUS_IN);
    send_helper_event (String (SCIM_ANTHY_HELPER_UUID), send);
}

void
AnthyInstance::focus_out ()
{
    SCIM_DEBUG_IMENGINE(2) << "focus_out.\n";



    Transaction send;
    send.put_command (SCIM_TRANS_CMD_REQUEST);
    send.put_command (SCIM_TRANS_CMD_FOCUS_OUT);
    send_helper_event (String (SCIM_ANTHY_HELPER_UUID), send);
}

void
AnthyInstance::set_layout (unsigned int layout)
{
    m_layout = layout;
}

void
AnthyInstance::set_preedition (void)
{
    update_preedit_string (m_preedit.get_string (),
                           m_preedit.get_attribute_list ());
    update_preedit_caret (m_preedit.get_caret_pos());
}

void
AnthyInstance::set_aux_string (void)
{
    char buf[256];
    sprintf (buf, _("Candidates (%d/%d)"),
             m_lookup_table.get_cursor_pos () + 1,
             m_lookup_table.number_of_candidates ());
    update_aux_string (utf8_mbstowcs (buf));
}

void
AnthyInstance::set_lookup_table (void)
{
    m_n_conv_key_pressed++;

    if (!is_selecting_candidates ()) {
        if (is_realtime_conversion () &&
            m_preedit.get_selected_segment () < 0)
        {
            // select latest segment
            int n = m_preedit.get_nr_segments ();
            if (n < 1)
                return;
            m_preedit.select_segment (n - 1);
        }

        // prepare candidates
        m_preedit.get_candidates (m_lookup_table);

        if (m_lookup_table.number_of_candidates () == 0)
            return;

        // set position
        update_lookup_table (m_lookup_table);

        // update preedit
        m_preedit.select_candidate (m_lookup_table.get_cursor_pos ());
        set_preedition ();

    }

    bool beyond_threshold =
        m_factory->m_n_triggers_to_show_cand_win > 0 &&
        (int) m_n_conv_key_pressed >= m_factory->m_n_triggers_to_show_cand_win;

    if (!m_lookup_table_visible &&
        (m_preedit.is_predicting () || beyond_threshold))
    {
        show_lookup_table ();
        m_lookup_table_visible = true;
        m_n_conv_key_pressed = 0;

        if (m_factory->m_show_candidates_label) {
            set_aux_string ();
            show_aux_string ();
        }
    } else if (!m_lookup_table_visible) {
        hide_lookup_table ();
    }
}

void
AnthyInstance::unset_lookup_table (void)
{
    m_lookup_table.clear ();
    hide_lookup_table ();
    m_lookup_table_visible = false;
    m_n_conv_key_pressed = 0;

    update_aux_string (utf8_mbstowcs (""));
    hide_aux_string ();
}

void
AnthyInstance::install_properties (void)
{
    if (m_properties.size () <= 0) {
        Property prop;

        if (m_factory->m_show_input_mode_label) {
            prop = Property (SCIM_PROP_INPUT_MODE,
                             "\xE3\x81\x82", String (""), _("Input mode"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_INPUT_MODE_HIRAGANA,
                             _("Hiragana"), String (""), _("Hiragana"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_INPUT_MODE_KATAKANA,
                             _("Katakana"), String (""), _("Katakana"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_INPUT_MODE_HALF_KATAKANA,
                             _("Half width katakana"), String (""),
                             _("Half width katakana"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_INPUT_MODE_LATIN,
                             _("Latin"), String (""), _("Direct input"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_INPUT_MODE_WIDE_LATIN,
                             _("Wide latin"), String (""), _("Wide latin"));
            m_properties.push_back (prop);
        }

        if (m_factory->m_show_typing_method_label) {
            prop = Property (SCIM_PROP_TYPING_METHOD,
                             "\xEF\xBC\xB2", String (""), _("Typing method"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_TYPING_METHOD_ROMAJI,
                             _("Romaji"), String (""), _("Romaji"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_TYPING_METHOD_KANA,
                             _("Kana"), String (""), _("Kana"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_TYPING_METHOD_NICOLA,
                             _("Thumb shift"), String (""), _("Thumb shift"));
            m_properties.push_back (prop);
        }

        if (m_factory->m_show_conv_mode_label) {
            prop = Property (SCIM_PROP_CONV_MODE,
                             "\xE9\x80\xA3", String (""),
                             _("Conversion mode"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_CONV_MODE_MULTI_SEG,
                             _("Multi segment"), String (""),
                             _("Multi segment"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_CONV_MODE_SINGLE_SEG,
                             _("Single segment"), String (""),
                             _("Single segment"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_CONV_MODE_MULTI_REAL_TIME,
                             _("Convert as you type (Multi segment)"),
                             String (""),
                             _("Convert as you type (Multi segment)"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_CONV_MODE_SINGLE_REAL_TIME,
                             _("Convert as you type (Single segment)"),
                             String (""),
                             _("Convert as you type (Single segment)"));
            m_properties.push_back (prop);
        }

        if (m_factory->m_show_period_style_label) {
            prop = Property (SCIM_PROP_PERIOD_STYLE,
                             "\xE3\x80\x81\xE3\x80\x82", String (""),
                             _("Period style"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_PERIOD_STYLE_JAPANESE,
                             "\xE3\x80\x81\xE3\x80\x82", String (""),
                             "\xE3\x80\x81\xE3\x80\x82");
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_PERIOD_STYLE_WIDE_LATIN_JAPANESE,
                             "\xEF\xBC\x8C\xE3\x80\x82", String (""),
                             "\xEF\xBC\x8C\xE3\x80\x82");
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_PERIOD_STYLE_WIDE_LATIN,
                             "\xEF\xBC\x8C\xEF\xBC\x8E", String (""),
                             "\xEF\xBC\x8C\xEF\xBC\x8E");
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_PERIOD_STYLE_LATIN,
                             ",.", String (""), ",.");
            m_properties.push_back (prop);
        }

        if (m_factory->m_show_symbol_style_label) {
            prop = Property (SCIM_PROP_SYMBOL_STYLE,
                             UTF8_BRACKET_CORNER_BEGIN
                             UTF8_BRACKET_CORNER_END
                             UTF8_MIDDLE_DOT,
                             String (""),
                             _("Symbol style"));
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_SYMBOL_STYLE_JAPANESE,
                             UTF8_BRACKET_CORNER_BEGIN
                             UTF8_BRACKET_CORNER_END
                             UTF8_MIDDLE_DOT,
                             String (""),
                             UTF8_BRACKET_CORNER_BEGIN
                             UTF8_BRACKET_CORNER_END
                             UTF8_MIDDLE_DOT);
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_SYMBOL_STYLE_CORNER_BRACKET_SLASH,
                             UTF8_BRACKET_CORNER_BEGIN
                             UTF8_BRACKET_CORNER_END
                             UTF8_SLASH_WIDE,
                             String (""),
                             UTF8_BRACKET_CORNER_BEGIN
                             UTF8_BRACKET_CORNER_END
                             UTF8_SLASH_WIDE);
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_SYMBOL_STYLE_BRACKET_MIDDLE_DOT,
                             UTF8_BRACKET_WIDE_BEGIN
                             UTF8_BRACKET_WIDE_END
                             UTF8_MIDDLE_DOT,
                             String (""),
                             UTF8_BRACKET_WIDE_BEGIN
                             UTF8_BRACKET_WIDE_END
                             UTF8_MIDDLE_DOT);
            m_properties.push_back (prop);

            prop = Property (SCIM_PROP_SYMBOL_STYLE_BRACKET_SLASH,
                             UTF8_BRACKET_WIDE_BEGIN
                             UTF8_BRACKET_WIDE_END
                             UTF8_SLASH_WIDE,
                             String (""),
                             UTF8_BRACKET_WIDE_BEGIN
                             UTF8_BRACKET_WIDE_END
                             UTF8_SLASH_WIDE);
            m_properties.push_back (prop);
        }

        if (m_factory->m_show_dict_label) {
            prop = Property (SCIM_PROP_DICT,
                             String(""), //_("Dictionary"),
                             String (SCIM_ICONDIR "/" "scim-anthy-dict.png"),
                             _("Dictionary menu"));
            m_properties.push_back (prop);

            if (m_factory->m_show_dict_admin_label) {
                prop = Property (SCIM_PROP_DICT_LAUNCH_ADMIN_TOOL,
                                 _("Edit the dictionary"),
                                 String (SCIM_ICONDIR "/" "scim-anthy-dict.png"),
                                 _("Launch the dictionary administration tool."));
                m_properties.push_back (prop);
            }

            if (m_factory->m_show_add_word_label) {
                prop = Property (SCIM_PROP_DICT_ADD_WORD,
                                 _("Add a word"),
                                 String (SCIM_ICONDIR "/" "scim-anthy-dict.png"),
                                 _("Add a word to the dictionary."));
                m_properties.push_back (prop);
            }
        }
    }

    set_input_mode(get_input_mode ());
    set_conversion_mode (m_conv_mode);
    set_typing_method (get_typing_method ());
    set_period_style (m_preedit.get_period_style (),
                      m_preedit.get_comma_style ());
    set_symbol_style (m_preedit.get_bracket_style (),
                      m_preedit.get_slash_style ());

    register_properties (m_properties);
}

void
AnthyInstance::set_input_mode (InputMode mode)
{
    const char *label = "";

    switch (mode) {
    case SCIM_ANTHY_MODE_HIRAGANA:
        label = "\xE3\x81\x82";
        break;
    case SCIM_ANTHY_MODE_KATAKANA:
        label = "\xE3\x82\xA2";
        break;
    case SCIM_ANTHY_MODE_HALF_KATAKANA:
        label = "_\xEF\xBD\xB1";
        break;
    case SCIM_ANTHY_MODE_LATIN:
        label = "_A";
        break;
    case SCIM_ANTHY_MODE_WIDE_LATIN:
        label = "\xEF\xBC\xA1";
        break;
    default:
        break; 
    }

    if (label && *label && m_factory->m_show_input_mode_label) {
        PropertyList::iterator it = std::find (m_properties.begin (),
                                               m_properties.end (),
                                               SCIM_PROP_INPUT_MODE);
        if (it != m_properties.end ()) {
            it->set_label (label);
            update_property (*it);
        }
    }

    if (mode != get_input_mode ()) {
        m_preedit.set_input_mode (mode);
        set_preedition ();
    }
}

void
AnthyInstance::set_conversion_mode (ConversionMode mode)
{
    const char *label = "";

    switch (mode) {
    case SCIM_ANTHY_CONVERSION_MULTI_SEGMENT:
        label = "\xE9\x80\xA3";
        break;
    case SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT:
        label = "\xE5\x8D\x98";
        break;
    case SCIM_ANTHY_CONVERSION_MULTI_SEGMENT_IMMEDIATE:
        label = "\xE9\x80\x90 \xE9\x80\xA3";
        break;
    case SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT_IMMEDIATE:
        label = "\xE9\x80\x90 \xE5\x8D\x98";
        break;
    default:
        break; 
    }

    if (label && *label /*&& m_factory->m_show_input_mode_label*/) {
        PropertyList::iterator it = std::find (m_properties.begin (),
                                               m_properties.end (),
                                               SCIM_PROP_CONV_MODE);
        if (it != m_properties.end ()) {
            it->set_label (label);
            update_property (*it);
        }
    }

    m_conv_mode = mode;
}

void
AnthyInstance::set_typing_method (TypingMethod method)
{
    const char *label = "";

    switch (method) {
    case SCIM_ANTHY_TYPING_METHOD_ROMAJI:
        label = "\xEF\xBC\xB2";
        break;
    case SCIM_ANTHY_TYPING_METHOD_KANA:
        label = "\xE3\x81\x8B";
        break;
    case SCIM_ANTHY_TYPING_METHOD_NICOLA:
        label = "\xE8\xA6\xAA";
        break;
    default:
        break;
    }

    if (label && *label && m_factory->m_show_typing_method_label) {
        PropertyList::iterator it = std::find (m_properties.begin (),
                                               m_properties.end (),
                                               SCIM_PROP_TYPING_METHOD);
        if (it != m_properties.end ()) {
            it->set_label (label);
            update_property (*it);
        }
    }

    if (method != get_typing_method ()) {
        Key2KanaTable *fundamental_table = NULL;

        if (method == SCIM_ANTHY_TYPING_METHOD_ROMAJI) {
            fundamental_table = m_factory->m_custom_romaji_table;
        } else if (method == SCIM_ANTHY_TYPING_METHOD_KANA) {
            fundamental_table = m_factory->m_custom_kana_table;
        }
        m_preedit.set_typing_method (method);
        m_preedit.set_pseudo_ascii_mode (get_pseudo_ascii_mode ());
    }
}

void
AnthyInstance::set_period_style (PeriodStyle period,
                                 CommaStyle  comma)
{
    String label;

    switch (comma) {
    case SCIM_ANTHY_COMMA_JAPANESE:
        label = "\xE3\x80\x81";
        break;
    case SCIM_ANTHY_COMMA_WIDE:
        label = "\xEF\xBC\x8C";
        break;
    case SCIM_ANTHY_COMMA_HALF:
        label = ",";
        break;
    default:
        break;
    }

    switch (period) {
    case SCIM_ANTHY_PERIOD_JAPANESE:
        label += "\xE3\x80\x82";
        break;
    case SCIM_ANTHY_PERIOD_WIDE:
        label += "\xEF\xBC\x8E";
        break;
    case SCIM_ANTHY_PERIOD_HALF:
        label += ".";
        break;
    default:
        break;
    }

    if (label.length () > 0) {
        PropertyList::iterator it = std::find (m_properties.begin (),
                                               m_properties.end (),
                                               SCIM_PROP_PERIOD_STYLE);
        if (it != m_properties.end ()) {
            it->set_label (label.c_str ());
            update_property (*it);
        }
    }

    if (period != m_preedit.get_period_style ())
        m_preedit.set_period_style (period);
    if (comma != m_preedit.get_comma_style ())
        m_preedit.set_comma_style (comma);
}

void
AnthyInstance::set_symbol_style (BracketStyle bracket,
                                 SlashStyle   slash)
{
    String label;

    switch (bracket) {
    case SCIM_ANTHY_BRACKET_JAPANESE:
        label = UTF8_BRACKET_CORNER_BEGIN UTF8_BRACKET_CORNER_END;
        break;
    case SCIM_ANTHY_BRACKET_WIDE:
        label = UTF8_BRACKET_WIDE_BEGIN UTF8_BRACKET_WIDE_END;
        break;
    default:
        break;
    }

    switch (slash) {
    case SCIM_ANTHY_SLASH_JAPANESE:
        label += UTF8_MIDDLE_DOT;
        break;
    case SCIM_ANTHY_SLASH_WIDE:
        label += UTF8_SLASH_WIDE;
        break;
    default:
        break;
    }

    if (label.length () > 0) {
        PropertyList::iterator it = std::find (m_properties.begin (),
                                               m_properties.end (),
                                               SCIM_PROP_SYMBOL_STYLE);
        if (it != m_properties.end ()) {
            it->set_label (label.c_str ());
            update_property (*it);
        }
    }

    if (bracket != m_preedit.get_bracket_style ())
        m_preedit.set_bracket_style (bracket);
    if (slash != m_preedit.get_slash_style ())
        m_preedit.set_slash_style (slash);
}

bool
AnthyInstance::is_selecting_candidates (void)
{
    if (m_lookup_table.number_of_candidates ())
        return true;
    else
        return false;
}

bool
AnthyInstance::action_do_nothing (void)
{
    return true;
}

bool
AnthyInstance::action_convert (void)
{
    if (!m_preedit.is_preediting ())
        return false;

    if (!m_preedit.is_converting ()) {
        // show conversion string
        m_preedit.finish ();
        m_preedit.convert (SCIM_ANTHY_CANDIDATE_DEFAULT,
                           is_single_segment ());
        set_preedition ();
        set_lookup_table ();
        return true;
    }

    return false;
}

bool
AnthyInstance::action_predict (void)
{
    if (!m_preedit.is_preediting ())
        return false;

    if (m_preedit.is_converting ())
        return false;

    if (!m_preedit.is_predicting ())
        m_preedit.predict ();

    m_preedit.select_candidate (0);
    set_preedition ();
    set_lookup_table ();
    select_candidate_no_direct (0);

    return true;
}


bool
AnthyInstance::action_revert (void)
{
    if (m_preedit.is_reconverting ()) {
        m_preedit.revert ();
        commit_string (m_preedit.get_string ());
        flush ();
        return true;
    }

    if (!m_preedit.is_preediting ())
        return false;

    if (!m_preedit.is_converting ()) {
        flush ();
        return true;
    }

    if (is_selecting_candidates ()) {
        m_lookup_table.clear ();
        if (m_lookup_table_visible) {
            unset_lookup_table ();
            return true;
        }
    }

    unset_lookup_table ();
    m_preedit.revert ();
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_cancel_all (void)
{
    if (!m_preedit.is_preediting ())
        return false;

    flush ();
    return true;
}

bool
AnthyInstance::action_commit (bool learn)
{
    if (!m_preedit.is_preediting ())
        return false;

    if (m_preedit.is_converting ()) {
        commit_string (m_preedit.get_string ());
        if (learn)
            m_preedit.commit ();
    } else {
        m_preedit.finish ();
        commit_string (m_preedit.get_string ());
    }

    flush ();

    return true;
}

bool
AnthyInstance::action_commit_follow_preference (void)
{
    return action_commit (m_factory->m_learn_on_manual_commit);
}

bool
AnthyInstance::action_commit_reverse_preference (void)
{
    return action_commit (!m_factory->m_learn_on_manual_commit);
}

bool
AnthyInstance::action_back (void)
{
    if (!m_preedit.is_preediting ())
        return false;

    if (m_preedit.is_converting ()) {
        action_revert ();
        if (!is_realtime_conversion ())
            return true;
    }

    m_preedit.erase ();

    if (m_preedit.get_length () > 0) {
        if (is_realtime_conversion ()) {
            m_preedit.convert (SCIM_ANTHY_CANDIDATE_DEFAULT,
                               is_single_segment ());
            m_preedit.select_segment (-1);
        }
        set_preedition ();
    } else {
        flush ();
    }

    return true;
}

bool
AnthyInstance::action_delete (void)
{
    if (!m_preedit.is_preediting ())
        return false;

    if (m_preedit.is_converting ()) {
        action_revert ();
        if (!is_realtime_conversion ())
            return true;
    }

    m_preedit.erase (false);

    if (m_preedit.get_length () > 0) {
        if (is_realtime_conversion ()) {
            m_preedit.convert (SCIM_ANTHY_CANDIDATE_DEFAULT,
                               is_single_segment ());
            m_preedit.select_segment (-1);
        }
        set_preedition ();
    } else {
        flush ();
    }

    return true;
}

bool
AnthyInstance::action_insert_space (void)
{
    String str;
    bool is_wide = false, retval = false;

    if (m_preedit.is_preediting () && !m_factory->m_romaji_pseudo_ascii_blank_behavior)
        return false;

    if (m_factory->m_space_type == "FollowMode") {
        InputMode mode = get_input_mode ();
        if (mode == SCIM_ANTHY_MODE_LATIN ||
            mode == SCIM_ANTHY_MODE_HALF_KATAKANA ||
            m_preedit.is_pseudo_ascii_mode ())
        {
            is_wide = false;
        } else {
            is_wide = true;
        }
    } else if (m_factory->m_space_type == "Wide") {
        is_wide = true;
    }

    if (is_wide) {
        str = "\xE3\x80\x80";
        retval = true;
    } else if (get_typing_method () == SCIM_ANTHY_TYPING_METHOD_NICOLA || // FIXME! it's a ad-hoc solution.
               m_preedit.is_pseudo_ascii_mode () ||
               (m_last_key.code != SCIM_KEY_space &&
                m_last_key.code != SCIM_KEY_KP_Space))
    {
        str = " ";
        retval = true;
    }

    if (retval) {
        if (m_preedit.is_pseudo_ascii_mode ()) {
            m_preedit.append (m_last_key, str);
            show_preedit_string ();
            m_preedit_string_visible = true;
            set_preedition ();
        } else {
            commit_string (utf8_mbstowcs (str));
        }
    }

    return retval;
}

bool 
AnthyInstance::action_insert_alternative_space (void)
{
    bool is_wide = false;

    if (m_preedit.is_preediting ())
        return false;

    if (m_factory->m_space_type == "FollowMode") {
        InputMode mode = get_input_mode ();
        if (mode == SCIM_ANTHY_MODE_LATIN ||
            mode == SCIM_ANTHY_MODE_HALF_KATAKANA)
        {
            is_wide = true;
        } else {
            is_wide = false;
        }
    } else if (m_factory->m_space_type != "Wide") {
        is_wide = true;
    }

    if (is_wide) {
        commit_string (utf8_mbstowcs ("\xE3\x80\x80"));
        return true;
    } else if (get_typing_method () == SCIM_ANTHY_TYPING_METHOD_NICOLA || // FIXME! it's a ad-hoc solution.
               (m_last_key.code != SCIM_KEY_space &&
                m_last_key.code != SCIM_KEY_KP_Space))
    {
        commit_string (utf8_mbstowcs (" "));
        return true;
    }

    return false;
}

bool
AnthyInstance::action_insert_half_space (void)
{
    if (m_preedit.is_preediting ())
        return false;

    if (m_last_key.code != SCIM_KEY_space &&
        m_last_key.code != SCIM_KEY_KP_Space)
    {
        commit_string (utf8_mbstowcs (" "));
        return true;
    }

    return false;
}

bool
AnthyInstance::action_insert_wide_space (void)
{
    if (m_preedit.is_preediting ())
        return false;

    commit_string (utf8_mbstowcs ("\xE3\x80\x80"));

    return true;
}

bool
AnthyInstance::action_move_caret_backward (void)
{
    if (!m_preedit.is_preediting ())
        return false;
    if (m_preedit.is_converting ())
        return false;

    m_preedit.move_caret(-1);
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_move_caret_forward (void)
{
    if (!m_preedit.is_preediting ())
        return false;
    if (m_preedit.is_converting ())
        return false;

    m_preedit.move_caret(1);
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_move_caret_first (void)
{
    if (!m_preedit.is_preediting ())
        return false;
    if (m_preedit.is_converting ())
        return false;

    m_preedit.set_caret_pos (0);
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_move_caret_last (void)
{
    if (!m_preedit.is_preediting ())
        return false;
    if (m_preedit.is_converting ())
        return false;

    m_preedit.set_caret_pos (m_preedit.get_length ());
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_select_prev_segment (void)
{
    if (!m_preedit.is_converting ())
        return false;

    unset_lookup_table ();

    int idx = m_preedit.get_selected_segment ();
    if (idx - 1 < 0) {
        int n = m_preedit.get_nr_segments ();
        if (n <= 0) return false;
        m_preedit.select_segment (n - 1);
    } else {
        m_preedit.select_segment (idx - 1);
    }
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_select_next_segment (void)
{
    if (!m_preedit.is_converting ())
        return false;

    unset_lookup_table ();

    int idx = m_preedit.get_selected_segment ();
    if (idx < 0) {
        m_preedit.select_segment(0);
    } else {
        int n = m_preedit.get_nr_segments ();
        if (n <= 0)
            return false;
        if (idx + 1 >= n)
            m_preedit.select_segment(0);
        else
            m_preedit.select_segment(idx + 1);
    }
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_select_first_segment (void)
{
    if (!m_preedit.is_converting ())
        return false;

    unset_lookup_table ();

    m_preedit.select_segment(0);
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_select_last_segment (void)
{
    if (!m_preedit.is_converting ())
        return false;

    int n = m_preedit.get_nr_segments ();
    if (n <= 0) return false;

    unset_lookup_table ();

    m_preedit.select_segment(n - 1);
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_shrink_segment (void)
{
    if (!m_preedit.is_converting ())
        return false;

    unset_lookup_table ();

    m_preedit.resize_segment (-1);
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_expand_segment (void)
{
    if (!m_preedit.is_converting ())
        return false;

    unset_lookup_table ();

    m_preedit.resize_segment (1);
    set_preedition ();

    return true;
}

bool
AnthyInstance::action_commit_first_segment (void)
{
    if (!m_preedit.is_converting ()) {
        if (m_preedit.is_preediting ()) {
            return action_commit (m_factory->m_learn_on_manual_commit);
        } else {
            return false;
        }
    }

    unset_lookup_table ();

    commit_string (m_preedit.get_segment_string (0));
    if (m_factory->m_learn_on_manual_commit)
        m_preedit.commit (0);
    else
        m_preedit.clear (0);

    set_preedition ();

    return true;
}

bool
AnthyInstance::action_commit_selected_segment (void)
{
    if (!m_preedit.is_converting ()) {
        if (m_preedit.is_preediting ()) {
            return action_commit (m_factory->m_learn_on_manual_commit);
        } else {
            return false;
        }
    }

    unset_lookup_table ();

    for (int i = 0; i <= m_preedit.get_selected_segment (); i++)
        commit_string (m_preedit.get_segment_string (i));
    if (m_factory->m_learn_on_manual_commit)
        m_preedit.commit (m_preedit.get_selected_segment ());
    else
        m_preedit.clear (m_preedit.get_selected_segment ());

    set_preedition ();

    return true;
}

bool
AnthyInstance::action_commit_first_segment_reverse_preference (void)
{
    if (!m_preedit.is_converting ()) {
        if (m_preedit.is_preediting ()) {
            return action_commit (!m_factory->m_learn_on_manual_commit);
        } else {
            return false;
        }
    }

    unset_lookup_table ();

    commit_string (m_preedit.get_segment_string (0));
    if (!m_factory->m_learn_on_manual_commit)
        m_preedit.commit (0);
    else
        m_preedit.clear (0);

    set_preedition ();

    return true;
}

bool
AnthyInstance::action_commit_selected_segment_reverse_preference (void)
{
    if (!m_preedit.is_converting ()) {
        if (m_preedit.is_preediting ()) {
            return action_commit (!m_factory->m_learn_on_manual_commit);
        } else {
            return false;
        }
    }

    unset_lookup_table ();

    for (int i = 0; i <= m_preedit.get_selected_segment (); i++)
        commit_string (m_preedit.get_segment_string (i));
    if (!m_factory->m_learn_on_manual_commit)
        m_preedit.commit (m_preedit.get_selected_segment ());
    else
        m_preedit.clear (m_preedit.get_selected_segment ());

    set_preedition ();

    return true;
}

bool
AnthyInstance::action_select_next_candidate (void)
{
    if (!m_preedit.is_converting ())
        return false;

    //if (!is_selecting_candidates ())
        set_lookup_table ();

    int end = m_lookup_table.number_of_candidates () - 1;
    if (m_lookup_table.get_cursor_pos () == end) {
        m_lookup_table.set_cursor_pos (0);
    } else {
        m_lookup_table.cursor_down ();
    }

    int pos_in_page = m_lookup_table.get_cursor_pos_in_current_page ();
    select_candidate_no_direct (pos_in_page);

    return true;
}

bool
AnthyInstance::action_select_prev_candidate (void)
{
    if (!m_preedit.is_converting ()) return false;

    //if (!is_selecting_candidates ())
        set_lookup_table ();

    int end = m_lookup_table.number_of_candidates () - 1;
    if (m_lookup_table.get_cursor_pos () == 0)
        m_lookup_table.set_cursor_pos (end);
    else
        m_lookup_table.cursor_up ();

    int pos_in_page = m_lookup_table.get_cursor_pos_in_current_page ();
    select_candidate_no_direct (pos_in_page);

    return true;
}

bool
AnthyInstance::action_select_first_candidate (void)
{
    if (!m_preedit.is_converting ()) return false;
    if (!is_selecting_candidates ()) return false;

    m_lookup_table.set_cursor_pos (0);

    int pos_in_page = m_lookup_table.get_cursor_pos_in_current_page ();
    select_candidate_no_direct (pos_in_page);

    return true;
}

bool
AnthyInstance::action_select_last_candidate (void)
{
    if (!m_preedit.is_converting ()) return false;
    if (!is_selecting_candidates ()) return false;

    int end = m_lookup_table.number_of_candidates () - 1;
    m_lookup_table.set_cursor_pos (end);

    int pos_in_page = m_lookup_table.get_cursor_pos_in_current_page ();
    select_candidate_no_direct (pos_in_page);

    return true;
}

bool
AnthyInstance::action_candidates_page_up(void)
{
    if (!m_preedit.is_converting ()) return false;
    if (!is_selecting_candidates ()) return false;
    if (!m_lookup_table_visible) return false;

    m_lookup_table.page_up ();

    int pos_in_page = m_lookup_table.get_cursor_pos_in_current_page ();
    select_candidate_no_direct (pos_in_page);

    return true;
}

bool
AnthyInstance::action_candidates_page_down (void)
{
    if (!m_preedit.is_converting ()) return false;
    if (!is_selecting_candidates ()) return false;
    if (!m_lookup_table_visible) return false;

    m_lookup_table.page_down ();

    int pos_in_page = m_lookup_table.get_cursor_pos_in_current_page ();
    select_candidate_no_direct (pos_in_page);

    return true;
}

bool
AnthyInstance::action_select_candidate (unsigned int i)
{
    // FIXME! m_lookup_table_visible should be set as true also on predicting
    if (!m_lookup_table_visible && !m_preedit.is_predicting ())
        return false;

    if (m_preedit.is_predicting () && !m_preedit.is_converting () &&
        m_factory->m_use_direct_key_on_predict)
    {
        CommonLookupTable table;
        m_preedit.get_candidates (table);
        if (i < table.number_of_candidates ()) {
            select_candidate (i);
            return true;
        }
    } else if (m_preedit.is_converting () && is_selecting_candidates ()) {
        select_candidate (i);
        return true;
    }

    return false;
}

bool
AnthyInstance::action_select_candidate_1 (void)
{
    return action_select_candidate (0);
}

bool
AnthyInstance::action_select_candidate_2 (void)
{
    return action_select_candidate (1);
}

bool
AnthyInstance::action_select_candidate_3 (void)
{
    return action_select_candidate (2);
}

bool
AnthyInstance::action_select_candidate_4 (void)
{
    return action_select_candidate (3);
}

bool
AnthyInstance::action_select_candidate_5 (void)
{
    return action_select_candidate (4);
}

bool
AnthyInstance::action_select_candidate_6 (void)
{
    return action_select_candidate (5);
}

bool
AnthyInstance::action_select_candidate_7 (void)
{
    return action_select_candidate (6);
}


bool
AnthyInstance::action_select_candidate_8 (void)
{
    return action_select_candidate (7);
}

bool
AnthyInstance::action_select_candidate_9 (void)
{
    return action_select_candidate (8);
}

bool
AnthyInstance::action_select_candidate_10 (void)
{
    return action_select_candidate (9);
}

bool
AnthyInstance::action_circle_input_mode (void)
{
    InputMode mode = get_input_mode ();

    switch (mode) {
    case SCIM_ANTHY_MODE_HIRAGANA:
        mode = SCIM_ANTHY_MODE_KATAKANA;
        break;
    case SCIM_ANTHY_MODE_KATAKANA:
        mode = SCIM_ANTHY_MODE_HALF_KATAKANA;
        break;
    case SCIM_ANTHY_MODE_HALF_KATAKANA:
        mode = SCIM_ANTHY_MODE_LATIN;
        break;
    case SCIM_ANTHY_MODE_LATIN:
        mode = SCIM_ANTHY_MODE_WIDE_LATIN;
        break;
    case SCIM_ANTHY_MODE_WIDE_LATIN:
        mode = SCIM_ANTHY_MODE_HIRAGANA;
        break;
    default:
        mode = SCIM_ANTHY_MODE_HIRAGANA;
        break; 
    }

    set_input_mode (mode);

    return true;
}

bool
AnthyInstance::action_circle_typing_method (void)
{
    TypingMethod method;

    method = get_typing_method ();
    if (method == SCIM_ANTHY_TYPING_METHOD_NICOLA)
        method = SCIM_ANTHY_TYPING_METHOD_ROMAJI;
    else if (method == SCIM_ANTHY_TYPING_METHOD_KANA)
        method = SCIM_ANTHY_TYPING_METHOD_NICOLA;
    else
        method = SCIM_ANTHY_TYPING_METHOD_KANA;

    set_typing_method (method);

    return true;
}

bool
AnthyInstance::action_circle_kana_mode (void)
{
    InputMode mode;

    if (get_input_mode () == SCIM_ANTHY_MODE_LATIN ||
        get_input_mode () == SCIM_ANTHY_MODE_WIDE_LATIN)
    {
        mode = SCIM_ANTHY_MODE_HIRAGANA;
    } else {
        switch (get_input_mode ()) {
        case SCIM_ANTHY_MODE_HIRAGANA:
            mode = SCIM_ANTHY_MODE_KATAKANA;
            break;
        case SCIM_ANTHY_MODE_KATAKANA:
            mode = SCIM_ANTHY_MODE_HALF_KATAKANA;
            break;
        case SCIM_ANTHY_MODE_HALF_KATAKANA:
        default:
            mode = SCIM_ANTHY_MODE_HIRAGANA;
            break;
        }
    }

    set_input_mode (mode);

    return true;
}

bool
AnthyInstance::action_on_off (void)
{
    if (get_input_mode () == SCIM_ANTHY_MODE_LATIN ||
        get_input_mode () == SCIM_ANTHY_MODE_WIDE_LATIN)
    {
        set_input_mode (m_prev_input_mode);
        m_preedit.set_input_mode (m_prev_input_mode);
    } else {
        m_prev_input_mode = get_input_mode ();
        set_input_mode (SCIM_ANTHY_MODE_LATIN);
        m_preedit.set_input_mode (SCIM_ANTHY_MODE_LATIN);
    }

    return true;
}

bool
AnthyInstance::action_latin_mode (void)
{
    set_input_mode (SCIM_ANTHY_MODE_LATIN);
    return true;
}

bool
AnthyInstance::action_wide_latin_mode (void)
{
    set_input_mode (SCIM_ANTHY_MODE_WIDE_LATIN);
    return true;
}

bool
AnthyInstance::action_hiragana_mode (void)
{
    set_input_mode (SCIM_ANTHY_MODE_HIRAGANA);
    return true;
}

bool
AnthyInstance::action_katakana_mode (void)
{
    set_input_mode (SCIM_ANTHY_MODE_KATAKANA);
    return true;
}

bool
AnthyInstance::action_half_katakana_mode (void)
{
    set_input_mode (SCIM_ANTHY_MODE_HALF_KATAKANA);
    return true;
}

bool
AnthyInstance::action_cancel_pseudo_ascii_mode (void)
{
    if (!m_preedit.is_preediting ())
        return false;
    if (!m_preedit.is_pseudo_ascii_mode ())
        return false;

    m_preedit.reset_pseudo_ascii_mode ();

    return true;
}

bool
AnthyInstance::convert_kana (CandidateType type)
{
    if (!m_preedit.is_preediting ())
        return false;

    if (m_preedit.is_reconverting ())
        return false;

    unset_lookup_table ();

    if (m_preedit.is_converting ()) {
        int idx = m_preedit.get_selected_segment ();
        if (idx < 0) {
            action_revert ();
            m_preedit.finish ();
            m_preedit.convert (type, true);
        } else {
            m_preedit.select_candidate (type);
        }
    } else {
        m_preedit.finish ();
        m_preedit.convert (type, true);
    }

    set_preedition ();

    return true;
}

bool
AnthyInstance::action_convert_to_hiragana (void)
{
    return convert_kana (SCIM_ANTHY_CANDIDATE_HIRAGANA);
}

bool
AnthyInstance::action_convert_to_katakana (void)
{
    return convert_kana (SCIM_ANTHY_CANDIDATE_KATAKANA);
}

bool
AnthyInstance::action_convert_to_half (void)
{
    return convert_kana (SCIM_ANTHY_CANDIDATE_HALF);
}

bool
AnthyInstance::action_convert_to_half_katakana (void)
{
    return convert_kana (SCIM_ANTHY_CANDIDATE_HALF_KATAKANA);
}

bool
AnthyInstance::action_convert_to_latin (void)
{
    return convert_kana (SCIM_ANTHY_CANDIDATE_LATIN);
}

bool
AnthyInstance::action_convert_to_wide_latin (void)
{
    return convert_kana (SCIM_ANTHY_CANDIDATE_WIDE_LATIN);
}

bool
AnthyInstance::action_convert_char_type_forward (void)
{
    if (!m_preedit.is_preediting ())
        return false;

    unset_lookup_table ();

    if (m_preedit.is_converting ()) {
        int idx = m_preedit.get_selected_segment ();
        if (idx < 0) {
            action_revert ();
            m_preedit.finish ();
            m_preedit.convert (SCIM_ANTHY_CANDIDATE_HIRAGANA, true);
        } else {
            int cand = m_preedit.get_selected_candidate ();
            switch (cand)
            {
            case SCIM_ANTHY_CANDIDATE_HIRAGANA:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_KATAKANA);
                break;
            case SCIM_ANTHY_CANDIDATE_KATAKANA:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_HALF_KATAKANA);
                break;
            case SCIM_ANTHY_CANDIDATE_HALF_KATAKANA:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_WIDE_LATIN);
                break;
            case SCIM_ANTHY_CANDIDATE_WIDE_LATIN:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_LATIN);
                break;
            case SCIM_ANTHY_CANDIDATE_LATIN:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_HIRAGANA);
                break;
            default:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_HIRAGANA);
                break;
            }
        }
    } else {
        m_preedit.finish ();
        m_preedit.convert (SCIM_ANTHY_CANDIDATE_HIRAGANA, true);
    }

    set_preedition ();

    return true;
}

bool
AnthyInstance::action_convert_char_type_backward (void)
{
    if (!m_preedit.is_preediting ())
        return false;

    unset_lookup_table ();

    if (m_preedit.is_converting ()) {
        int idx = m_preedit.get_selected_segment ();
        if (idx < 0) {
            action_revert ();
            m_preedit.finish ();
            m_preedit.convert (SCIM_ANTHY_CANDIDATE_HIRAGANA, true);
        } else {
            int cand = m_preedit.get_selected_candidate ();
            switch (cand)
            {
            case SCIM_ANTHY_CANDIDATE_HIRAGANA:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_LATIN);
                break;
            case SCIM_ANTHY_CANDIDATE_KATAKANA:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_HIRAGANA);
                break;
            case SCIM_ANTHY_CANDIDATE_HALF_KATAKANA:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_KATAKANA);
                break;
            case SCIM_ANTHY_CANDIDATE_WIDE_LATIN:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_HALF_KATAKANA);
                break;
            case SCIM_ANTHY_CANDIDATE_LATIN:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_WIDE_LATIN);
                break;
            default:
                m_preedit.select_candidate (SCIM_ANTHY_CANDIDATE_HIRAGANA);
                break;
            }
        }
    } else {
        m_preedit.finish ();
        m_preedit.convert (SCIM_ANTHY_CANDIDATE_HIRAGANA, true);
    }

    set_preedition ();

    return true;
}

bool
AnthyInstance::action_reconvert (void)
{
    if (m_preedit.is_preediting ())
        return false;

    Transaction send;
    send.put_command (SCIM_ANTHY_TRANS_CMD_GET_SELECTION);
    send_helper_event (String (SCIM_ANTHY_HELPER_UUID), send);

    return true;
}

bool
AnthyInstance::action_add_word (void)
{
    util_launch_program (m_factory->m_add_word_command.c_str ());

    return true;
}

bool
AnthyInstance::action_launch_dict_admin_tool (void)
{
    util_launch_program (m_factory->m_dict_admin_command.c_str ());

    return true;
}

#if 0
void
AnthyInstance::action_regist_word (void)
{
}
#endif

AnthyFactory *
AnthyInstance::get_factory (void)
{
    return m_factory;
}

TypingMethod
AnthyInstance::get_typing_method (void)
{
    return m_preedit.get_typing_method ();
}

InputMode
AnthyInstance::get_input_mode (void)
{
    return m_preedit.get_input_mode ();
}

int
AnthyInstance::timeout_add (uint32 time_msec, timeout_func timeout_fn,
                            void *data, delete_func delete_fn)
{
    uint32 id = ++m_timeout_id_seq;
    m_closures[id] = TimeoutClosure (time_msec, timeout_fn, data, delete_fn);
    /*
     * FIXME! Obsoleted closures should be removed at somewhere.
     * Currenly only NICOLA related timer uses this feature and it will be
     * removed each time on key press event so memory leaks doesn't exist.
     */

    Transaction send;
    send.put_command (SCIM_ANTHY_TRANS_CMD_TIMEOUT_ADD);
    send.put_data (id);
    send.put_data (time_msec);
    send_helper_event (String (SCIM_ANTHY_HELPER_UUID), send);

    return id;
}

void
AnthyInstance::timeout_remove (uint32 id)
{
    if (m_closures.find (id) == m_closures.end ())
        return;

    m_closures.erase (id);

    Transaction send;
    send.put_command (SCIM_ANTHY_TRANS_CMD_TIMEOUT_REMOVE);
    send.put_data (id);
    send_helper_event (String (SCIM_ANTHY_HELPER_UUID), send);
}

void
AnthyInstance::trigger_property (const String &property)
{
    String anthy_prop = property.substr (property.find_last_of ('/') + 1);

    SCIM_DEBUG_IMENGINE(2)
        << "trigger_property : " << property << " - " << anthy_prop << "\n";

    // input mode
    if (property == SCIM_PROP_INPUT_MODE_HIRAGANA) {
        set_input_mode (SCIM_ANTHY_MODE_HIRAGANA);
    } else if (property == SCIM_PROP_INPUT_MODE_KATAKANA) {
        set_input_mode (SCIM_ANTHY_MODE_KATAKANA);
    } else if (property == SCIM_PROP_INPUT_MODE_HALF_KATAKANA) {
        set_input_mode (SCIM_ANTHY_MODE_HALF_KATAKANA);
    } else if (property == SCIM_PROP_INPUT_MODE_LATIN) {
        set_input_mode (SCIM_ANTHY_MODE_LATIN);
    } else if (property == SCIM_PROP_INPUT_MODE_WIDE_LATIN) {
        set_input_mode (SCIM_ANTHY_MODE_WIDE_LATIN);

    // conversion mode
    } else if (property == SCIM_PROP_CONV_MODE_MULTI_SEG) {
        set_conversion_mode (SCIM_ANTHY_CONVERSION_MULTI_SEGMENT);
    } else if (property == SCIM_PROP_CONV_MODE_SINGLE_SEG) {
        set_conversion_mode (SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT);
    } else if (property == SCIM_PROP_CONV_MODE_MULTI_REAL_TIME) {
        set_conversion_mode (SCIM_ANTHY_CONVERSION_MULTI_SEGMENT_IMMEDIATE);
    } else if (property == SCIM_PROP_CONV_MODE_SINGLE_REAL_TIME) {
        set_conversion_mode (SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT_IMMEDIATE);

    // typing method
    } else if (property == SCIM_PROP_TYPING_METHOD_ROMAJI) {
        set_typing_method (SCIM_ANTHY_TYPING_METHOD_ROMAJI);
    } else if (property == SCIM_PROP_TYPING_METHOD_KANA) {
        set_typing_method (SCIM_ANTHY_TYPING_METHOD_KANA);
    } else if (property == SCIM_PROP_TYPING_METHOD_NICOLA) {
        set_typing_method (SCIM_ANTHY_TYPING_METHOD_NICOLA);

    // period type
    } else if (property == SCIM_PROP_PERIOD_STYLE_JAPANESE) {
        set_period_style (SCIM_ANTHY_PERIOD_JAPANESE,
                          SCIM_ANTHY_COMMA_JAPANESE);
    } else if (property == SCIM_PROP_PERIOD_STYLE_WIDE_LATIN_JAPANESE) {
        set_period_style (SCIM_ANTHY_PERIOD_JAPANESE,
                          SCIM_ANTHY_COMMA_WIDE);
    } else if (property == SCIM_PROP_PERIOD_STYLE_WIDE_LATIN) {
        set_period_style (SCIM_ANTHY_PERIOD_WIDE,
                          SCIM_ANTHY_COMMA_WIDE);
    } else if (property == SCIM_PROP_PERIOD_STYLE_LATIN) {
        set_period_style (SCIM_ANTHY_PERIOD_HALF,
                          SCIM_ANTHY_COMMA_HALF);

    // symbol type
    } else if (property == SCIM_PROP_SYMBOL_STYLE_JAPANESE) {
        set_symbol_style (SCIM_ANTHY_BRACKET_JAPANESE,
                          SCIM_ANTHY_SLASH_JAPANESE);
    } else if (property == SCIM_PROP_SYMBOL_STYLE_CORNER_BRACKET_SLASH) {
        set_symbol_style (SCIM_ANTHY_BRACKET_JAPANESE,
                          SCIM_ANTHY_SLASH_WIDE);
    } else if (property == SCIM_PROP_SYMBOL_STYLE_BRACKET_MIDDLE_DOT) {
        set_symbol_style (SCIM_ANTHY_BRACKET_WIDE,
                          SCIM_ANTHY_SLASH_JAPANESE);
    } else if (property == SCIM_PROP_SYMBOL_STYLE_BRACKET_SLASH) {
        set_symbol_style (SCIM_ANTHY_BRACKET_WIDE,
                          SCIM_ANTHY_SLASH_WIDE);

    // dictionary
    } else if (property == SCIM_PROP_DICT_ADD_WORD) {
        action_add_word ();
    } else if (property == SCIM_PROP_DICT_LAUNCH_ADMIN_TOOL) {
        action_launch_dict_admin_tool ();
    }
}

void
AnthyInstance::process_helper_event (const String &helper_uuid,
                                     const Transaction &recv)
{
    TransactionReader reader (recv);
    int cmd;

    if (helper_uuid != SCIM_ANTHY_HELPER_UUID)
        return;

    if (!reader.get_command (cmd))
        return;

    switch (cmd) {
    case SCIM_ANTHY_TRANS_CMD_GET_SELECTION:
    {
        // For reconversion feature, but this code is ad-hoc solution.

        WideString selection, surround;
        if (!reader.get_data (selection) || selection.empty ())
            break;

        int cursor;
        unsigned int len = selection.length ();
        if (!get_surrounding_text (surround, cursor, len, len))
        {
            // We expect application to delete selection text.
            m_preedit.convert(selection);
            set_preedition();
            set_lookup_table();
        }
        else
        {
            // This code will conflict if same string exists at both before and
            // after the caret.
            if (surround.length () - cursor >= len &&
                surround.substr (cursor, len) == selection)
            {
                delete_surrounding_text (0, len);
                m_preedit.convert (selection);
                set_preedition ();
                set_lookup_table ();
            } else if (cursor >= (int) len &&
                       surround.substr (cursor - len, len) == selection)
            {
                delete_surrounding_text (0 - len, len);
                m_preedit.convert (selection);
                set_preedition ();
                set_lookup_table ();
            }
        }
        break;
    }
    case SCIM_ANTHY_TRANS_CMD_TIMEOUT_NOTIFY:
    {
        uint32 id;
        if (reader.get_data (id) &&
            m_closures.find (id) != m_closures.end ())
        {
            m_closures[id].close ();
            m_closures.erase (id);
        }
        break;
    }
    default:
        break;
    }
}

void
AnthyInstance::reload_config (const ConfigPointer &config)
{
    // set romaji settings
    m_preedit.set_symbol_width (m_factory->m_romaji_half_symbol);
    m_preedit.set_number_width (m_factory->m_romaji_half_number);

    // set input mode
    if (m_on_init || !m_factory->m_show_input_mode_label) {
        if (m_factory->m_input_mode == "Hiragana")
            m_preedit.set_input_mode (SCIM_ANTHY_MODE_HIRAGANA);
        else if (m_factory->m_input_mode == "Katakana")
            m_preedit.set_input_mode (SCIM_ANTHY_MODE_KATAKANA);
        else if (m_factory->m_input_mode == "HalfKatakana")
            m_preedit.set_input_mode (SCIM_ANTHY_MODE_HALF_KATAKANA);
        else if (m_factory->m_input_mode == "Latin")
            m_preedit.set_input_mode (SCIM_ANTHY_MODE_LATIN);
        else if (m_factory->m_input_mode == "WideLatin")
            m_preedit.set_input_mode (SCIM_ANTHY_MODE_WIDE_LATIN);
    }

    // set typing method and pseudo ASCII mode
    if (m_on_init || !m_factory->m_show_typing_method_label) {
        if (m_factory->m_typing_method == "NICOLA") {
            m_preedit.set_typing_method (SCIM_ANTHY_TYPING_METHOD_NICOLA);
        } else if (m_factory->m_typing_method == "Kana") {
            m_preedit.set_typing_method (SCIM_ANTHY_TYPING_METHOD_KANA);
        } else {
            m_preedit.set_typing_method (SCIM_ANTHY_TYPING_METHOD_ROMAJI);
        }
        m_preedit.set_pseudo_ascii_mode (get_pseudo_ascii_mode ());
    } else {
        m_preedit.set_typing_method (get_typing_method ());
        m_preedit.set_pseudo_ascii_mode (get_pseudo_ascii_mode ());
    }

    // set conversion mode
    if (m_on_init || !m_factory->m_show_conv_mode_label) {
        if (m_factory->m_conversion_mode == "MultiSeg")
            m_conv_mode = SCIM_ANTHY_CONVERSION_MULTI_SEGMENT;
        else if (m_factory->m_conversion_mode == "SingleSeg")
            m_conv_mode = SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT;
        else if (m_factory->m_conversion_mode == "CAYT_MultiSeg")
            m_conv_mode = SCIM_ANTHY_CONVERSION_MULTI_SEGMENT_IMMEDIATE;
        else if (m_factory->m_conversion_mode == "CAYT_SingleSeg")
            m_conv_mode = SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT_IMMEDIATE;
    }

    // set period style
    if (m_on_init || !m_factory->m_show_period_style_label) {
        if (m_factory->m_period_style == "WideLatin") {
            m_preedit.set_comma_style  (SCIM_ANTHY_COMMA_WIDE);
            m_preedit.set_period_style (SCIM_ANTHY_PERIOD_WIDE);
        } else if (m_factory->m_period_style == "Latin") {
            m_preedit.set_comma_style  (SCIM_ANTHY_COMMA_HALF);
            m_preedit.set_period_style (SCIM_ANTHY_PERIOD_HALF);
        } else if (m_factory->m_period_style == "Japanese") {
            m_preedit.set_comma_style  (SCIM_ANTHY_COMMA_JAPANESE);
            m_preedit.set_period_style (SCIM_ANTHY_PERIOD_JAPANESE);
        } else if (m_factory->m_period_style == "WideLatin_Japanese") {
            m_preedit.set_comma_style  (SCIM_ANTHY_COMMA_WIDE);
            m_preedit.set_period_style (SCIM_ANTHY_PERIOD_JAPANESE);
        } else {
            m_preedit.set_comma_style  (SCIM_ANTHY_COMMA_JAPANESE);
            m_preedit.set_period_style (SCIM_ANTHY_PERIOD_JAPANESE);
        }
    }

    // set symbol style
    if (m_on_init || !m_factory->m_show_symbol_style_label) {
        if (m_factory->m_symbol_style == "Japanese") {
            m_preedit.set_bracket_style (SCIM_ANTHY_BRACKET_JAPANESE);
            m_preedit.set_slash_style   (SCIM_ANTHY_SLASH_JAPANESE);
        } else if (m_factory->m_symbol_style == "WideBracket_WideSlash") {
            m_preedit.set_bracket_style (SCIM_ANTHY_BRACKET_WIDE);
            m_preedit.set_slash_style   (SCIM_ANTHY_SLASH_WIDE);
        } else if (m_factory->m_symbol_style == "CornerBracket_WideSlash") {
            m_preedit.set_bracket_style (SCIM_ANTHY_BRACKET_JAPANESE);
            m_preedit.set_slash_style   (SCIM_ANTHY_SLASH_WIDE);
        } else if (m_factory->m_symbol_style == "WideBracket_MiddleDot") {
            m_preedit.set_bracket_style (SCIM_ANTHY_BRACKET_WIDE);
            m_preedit.set_slash_style   (SCIM_ANTHY_SLASH_JAPANESE);
        } else {
            m_preedit.set_bracket_style (SCIM_ANTHY_BRACKET_JAPANESE);
            m_preedit.set_slash_style   (SCIM_ANTHY_SLASH_JAPANESE);
        }
    }

    // set lookup table
    if (m_factory->m_cand_win_page_size > 0)
        m_lookup_table.set_page_size (m_factory->m_cand_win_page_size);
    else
        m_lookup_table.set_page_size (SCIM_ANTHY_CONFIG_CAND_WIN_PAGE_SIZE_DEFAULT);

    // setup toolbar
    m_properties.clear ();
    install_properties ();

    // set encoding
    m_preedit.set_dict_encoding (m_factory->m_dict_encoding);
}

bool
AnthyInstance::is_single_segment (void)
{
    if (m_conv_mode == SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT ||
        m_conv_mode == SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT_IMMEDIATE)
        return true;
    else
        return false;
}

bool
AnthyInstance::is_realtime_conversion (void)
{
    if (m_conv_mode == SCIM_ANTHY_CONVERSION_MULTI_SEGMENT_IMMEDIATE ||
        m_conv_mode == SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT_IMMEDIATE)
        return true;
    else
        return false;
}

int
AnthyInstance::get_pseudo_ascii_mode (void)
{
    int retval = 0;
    TypingMethod m = get_typing_method ();

    if (m == SCIM_ANTHY_TYPING_METHOD_ROMAJI) {
        if (m_factory->m_romaji_pseudo_ascii_mode)
            retval |= SCIM_ANTHY_PSEUDO_ASCII_TRIGGERED_CAPITALIZED;
    }

    return retval;
}

/*
vi:ts=4:nowrap:ai:expandtab
*/
