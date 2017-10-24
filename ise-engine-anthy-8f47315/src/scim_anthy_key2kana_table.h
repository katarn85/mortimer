/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2005 Takuro Ashie
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

#ifndef __SCIM_ANTHY_KEY2KANA_TABLE_H__
#define __SCIM_ANTHY_KEY2KANA_TABLE_H__

#define Uses_SCIM_TYPES
#include <scim.h>
#include "scim_anthy_default_tables.h"

using namespace scim;

namespace scim_anthy {

typedef enum {
    SCIM_ANTHY_PERIOD_JAPANESE,
    SCIM_ANTHY_PERIOD_WIDE,
    SCIM_ANTHY_PERIOD_HALF,
} PeriodStyle;

typedef enum {
    SCIM_ANTHY_COMMA_JAPANESE,
    SCIM_ANTHY_COMMA_WIDE,
    SCIM_ANTHY_COMMA_HALF,
} CommaStyle;

typedef enum {
    SCIM_ANTHY_BRACKET_JAPANESE,
    SCIM_ANTHY_BRACKET_WIDE,
} BracketStyle;

typedef enum {
    SCIM_ANTHY_SLASH_JAPANESE,
    SCIM_ANTHY_SLASH_WIDE,
} SlashStyle;

typedef enum {
    SCIM_ANTHY_TYPING_METHOD_ROMAJI,
    SCIM_ANTHY_TYPING_METHOD_KANA,
    SCIM_ANTHY_TYPING_METHOD_NICOLA,
    SCIM_ANTHY_TYPING_METHOD_CUSTOM,
} TypingMethod;


class Key2KanaRule;
class Key2KanaTable;
class Key2KanaTableSet;

typedef std::vector<Key2KanaRule> Key2KanaRules;


class Key2KanaRule
{
public:
    Key2KanaRule ();
    Key2KanaRule (String sequence,
                  const std::vector<String> &result);
    virtual ~Key2KanaRule ();

    String get_sequence        (void);
    String get_result          (unsigned int idx);

    void   clear               (void);

    bool   is_empty            (void);

private:
    String              m_sequence;
    std::vector<String> m_result;
};


class Key2KanaTable
{
public:
    Key2KanaTable (WideString  name);
    Key2KanaTable (WideString  name,
                   ConvRule   *table);
    Key2KanaTable (WideString  name,
                   NicolaRule *table);
    virtual ~Key2KanaTable ();

    Key2KanaRules & get_table   (void) { return m_rules; }

    void            append_rule (String sequence,
                                 const std::vector<String> &result);
    void            append_rule (String sequence,
                                 String result,
                                 String cont);
    void            append_rule (String sequence,
                                 String normal,
                                 String left_shift,
                                 String right_shift);
    void            clear       (void);

private:
    WideString    m_name;
    Key2KanaRules m_rules;
};


class Key2KanaTableSet
{
public:
    Key2KanaTableSet ();
    virtual ~Key2KanaTableSet ();

    std::vector<Key2KanaTable*> &
         get_tables (void) { return m_all_tables; };

    void set_typing_method       (TypingMethod   method,
                                  Key2KanaTable *fundamental_table = NULL);
    void set_symbol_width        (bool           half);
    void set_number_width        (bool           half);
    void set_period_style        (PeriodStyle    style);
    void set_comma_style         (CommaStyle     style);
    void set_bracket_style       (BracketStyle   style);
    void set_slash_style         (SlashStyle     style);

    TypingMethod
         get_typing_method       (void) { return m_typing_method; }
    bool symbol_is_half          (void) { return m_use_half_symbol; }
    bool number_is_half          (void) { return m_use_half_number;}
    PeriodStyle
         get_period_style        (void) { return m_period_style; }
    CommaStyle
         get_comma_style         (void) { return m_comma_style; }
    BracketStyle
         get_bracket_style       (void) { return m_bracket_style; }
    SlashStyle
         get_slash_style         (void) { return m_slash_style; }

#if 0
    void set_use_consonant_table (bool use);
    void set_use_symbol_table    (bool use);
    void set_use_number_table    (bool use);
    void set_use_period_table    (bool use);
    void set_use_comma_table     (bool use);

    bool get_use_consonant_table (void);
    bool get_use_symbol_table    (void);
    bool get_use_number_table    (void);
    bool get_use_period_table    (void);
    bool get_use_comma_table     (void);
#endif

private:
    void reset_tables            (void);

private:
    WideString m_name;

    // tables
    Key2KanaTable               *m_fundamental_table;
    Key2KanaTable                m_voiced_consonant_table;
    std::vector<Key2KanaTable*> *m_additional_table;
    std::vector<Key2KanaTable*>  m_all_tables;

    // flags
    TypingMethod m_typing_method;
    PeriodStyle  m_period_style;
    CommaStyle   m_comma_style;
    BracketStyle m_bracket_style;
    SlashStyle   m_slash_style;
    bool         m_use_half_symbol;
    bool         m_use_half_number;
};

}

#endif /* __SCIM_ANTHY_KEY2KANA_TABLE_H__ */
/*
vi:ts=4:nowrap:ai:expandtab
*/
