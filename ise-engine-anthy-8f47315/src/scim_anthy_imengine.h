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

#ifndef __SCIM_ANTHY_IMENGINE_H__
#define __SCIM_ANTHY_IMENGINE_H__

#define Uses_SCIM_IMENGINE
#define Uses_SCIM_CONFIG_BASE
#include <map>
#include <anthy/anthy.h>
#include <scim.h>
#include "scim_anthy_preedit.h"
#include "scim_anthy_key2kana_table.h"

using namespace scim;
using namespace scim_anthy;

class AnthyFactory;

namespace scim_anthy {

typedef enum {
    SCIM_ANTHY_CONVERSION_MULTI_SEGMENT,
    SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT,
    SCIM_ANTHY_CONVERSION_MULTI_SEGMENT_IMMEDIATE,
    SCIM_ANTHY_CONVERSION_SINGLE_SEGMENT_IMMEDIATE,
} ConversionMode;

typedef void (*timeout_func) (void *data);
typedef void (*delete_func)  (void *data);

class TimeoutClosure
{
public:
    TimeoutClosure ()
        : m_time_msec  (0),
          m_timeout_fn (NULL),
          m_data       (NULL),
          m_delete_fn  (NULL)
        {}
    TimeoutClosure  (uint32        time_msec,
                     timeout_func  timeout_fn,
                     void         *data,
                     delete_func   delete_fn)
        : m_time_msec  (time_msec),
          m_timeout_fn (timeout_fn),
          m_data       (data),
          m_delete_fn  (delete_fn)
        {}
    virtual ~TimeoutClosure ()
        {
            if (m_delete_fn && m_data)
                m_delete_fn (m_data);
        }

    void close (void) { m_timeout_fn (m_data); }
private:
    uint32        m_time_msec;
    timeout_func  m_timeout_fn;
    void         *m_data;
    delete_func   m_delete_fn;
};

typedef std::map<int, TimeoutClosure>  TimeoutClosures;

}

class AnthyInstance : public IMEngineInstanceBase
{
public:
    AnthyInstance (AnthyFactory   *factory,
                   const String   &encoding,
                   int             id = -1);
    virtual ~AnthyInstance ();

    virtual bool process_key_event            (const KeyEvent& key);
    virtual void move_preedit_caret           (unsigned int pos);
    virtual void select_candidate             (unsigned int item);
    virtual void update_lookup_table_page_size(unsigned int page_size);
    virtual void lookup_table_page_up         (void);
    virtual void lookup_table_page_down       (void);
    virtual void reset                        (void);
    virtual void focus_in                     (void);
    virtual void focus_out                    (void);
    virtual void trigger_property             (const String &property);
    virtual void process_helper_event         (const String &helper_uuid,
                                               const Transaction &trans);

    virtual void reload_config                (const ConfigPointer &config);
    virtual void flush ();
    virtual void set_layout                   (unsigned int layout);

public:
    /* actions */
    bool   action_do_nothing                  (void);

    bool   action_convert                     (void);
    bool   action_predict                     (void);
    bool   action_revert                      (void);
    bool   action_cancel_all                  (void);
    bool   action_commit_follow_preference    (void);
    bool   action_commit_reverse_preference   (void);
    bool   action_commit_first_segment        (void);
    bool   action_commit_selected_segment     (void);
    bool   action_commit_first_segment_reverse_preference
                                              (void);
    bool   action_commit_selected_segment_reverse_preference
                                              (void);
    bool   action_back                        (void);
    bool   action_delete                      (void);
    bool   action_insert_space                (void);
    bool   action_insert_alternative_space    (void);
    bool   action_insert_half_space           (void);
    bool   action_insert_wide_space           (void);

    bool   action_move_caret_backward         (void);
    bool   action_move_caret_forward          (void);
    bool   action_move_caret_first            (void);
    bool   action_move_caret_last             (void);

    bool   action_select_prev_segment         (void);
    bool   action_select_next_segment         (void);
    bool   action_select_first_segment        (void);
    bool   action_select_last_segment         (void);
    bool   action_shrink_segment              (void);
    bool   action_expand_segment              (void);

    bool   action_select_first_candidate      (void);
    bool   action_select_last_candidate       (void);
    bool   action_select_next_candidate       (void);
    bool   action_select_prev_candidate       (void);
    bool   action_candidates_page_up          (void);
    bool   action_candidates_page_down        (void);

    bool   action_select_candidate_1          (void);
    bool   action_select_candidate_2          (void);
    bool   action_select_candidate_3          (void);
    bool   action_select_candidate_4          (void);
    bool   action_select_candidate_5          (void);
    bool   action_select_candidate_6          (void);
    bool   action_select_candidate_7          (void);
    bool   action_select_candidate_8          (void);
    bool   action_select_candidate_9          (void);
    bool   action_select_candidate_10         (void);

    bool   action_convert_to_hiragana         (void);
    bool   action_convert_to_katakana         (void);
    bool   action_convert_to_half             (void);
    bool   action_convert_to_half_katakana    (void);
    bool   action_convert_to_latin            (void);
    bool   action_convert_to_wide_latin       (void);
    bool   action_convert_char_type_forward   (void);
    bool   action_convert_char_type_backward  (void);
    bool   action_reconvert                   (void);

    bool   action_on_off                      (void);

    bool   action_circle_input_mode           (void);
    bool   action_circle_kana_mode            (void);
    bool   action_circle_typing_method        (void);

    bool   action_latin_mode                  (void);
    bool   action_wide_latin_mode             (void);
    bool   action_hiragana_mode               (void);
    bool   action_katakana_mode               (void);
    bool   action_half_katakana_mode          (void);
    bool   action_cancel_pseudo_ascii_mode    (void);

    bool   action_add_word                    (void);
    bool   action_launch_dict_admin_tool      (void);
    /*
    void   actoin_register_word               (void);
    */

public:
    AnthyFactory *
           get_factory                        (void);
    TypingMethod
           get_typing_method                  (void);
    InputMode
           get_input_mode                     (void);
    int    timeout_add                        (uint32        time_msec,
                                               timeout_func  timeout_fn,
                                               void         *data = NULL,
                                               delete_func   delete_fn = NULL);
    void   timeout_remove                     (uint32        id);
    int    get_pseudo_ascii_mode              (void);

private:
    /* processing key event */
    bool   process_key_event_input            (const KeyEvent &key);
    bool   process_key_event_lookup_keybind   (const KeyEvent &key);
    bool   process_key_event_latin_mode       (const KeyEvent &key);
    bool   process_key_event_wide_latin_mode  (const KeyEvent &key);

    /* utility */
    void   set_preedition                     (void);
    void   set_aux_string                     (void);
    void   set_lookup_table                   (void);
    void   unset_lookup_table                 (void);
    void   install_properties                 (void);
    void   set_input_mode                     (InputMode      mode);
    void   set_conversion_mode                (ConversionMode mode);
    void   set_typing_method                  (TypingMethod   method);
    void   set_period_style                   (PeriodStyle    period,
                                               CommaStyle     comma);
    void   set_symbol_style                   (BracketStyle   bracket,
                                               SlashStyle     slash);
    bool   is_selecting_candidates            (void);
    void   select_candidate_no_direct         (unsigned int   item);
    bool   convert_kana                       (CandidateType  type);

    bool   action_commit                      (bool           learn);
    bool   action_select_candidate            (unsigned int   i);

    bool   is_single_segment                  (void);
    bool   is_realtime_conversion             (void);

private: // FIXME!
    bool   is_nicola_thumb_shift_key          (const KeyEvent &key);

private:
    AnthyFactory         *m_factory;

    bool                  m_on_init;

   /* for preedit */
    Preedit               m_preedit;
    bool                  m_preedit_string_visible;

    /* for candidates window */
    CommonLookupTable     m_lookup_table;
    bool                  m_lookup_table_visible;
    unsigned int          m_n_conv_key_pressed;

    /* for toggling latin and wide latin */
    InputMode             m_prev_input_mode;

    /* for toolbar */
    PropertyList          m_properties;

    /*  */
    ConversionMode        m_conv_mode;

    /* for action */
    KeyEvent              m_last_key;

    /* Helper */
    bool                  m_helper_started;

    /* timeout */
    uint32                m_timeout_id_seq;
    TimeoutClosures       m_closures;

    unsigned int          m_layout;
};
#endif /* __SCIM_ANTHY_IMENGINE_H__ */
/*
vi:ts=4:nowrap:ai:expandtab
*/
