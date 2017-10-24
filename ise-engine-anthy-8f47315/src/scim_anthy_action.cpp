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

#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#include "scim_anthy_action.h"
#include "scim_anthy_utils.h"

using namespace scim_anthy;

Action::Action (const String &name, const String &key_bindings, PMF pmf)
    : m_name (name),
      m_pmf  (pmf),
      m_func (NULL)
{
    scim_string_to_key_list (m_key_bindings, key_bindings);
}

Action::Action (const String &name, const String &key_bindings, Func func)
    : m_name (name),
      m_pmf  (NULL),
      m_func (func)
{
    scim_string_to_key_list (m_key_bindings, key_bindings);
}

Action::~Action (void)
{
}

bool
Action::perform (AnthyInstance *performer)
{
    if (m_pmf)
        return (performer->*m_pmf) ();
    else if (m_func)
        return m_func (performer);

    return false;
}

bool
Action::perform (AnthyInstance *performer, const KeyEvent &key)
{
    if (!m_pmf && !m_func)
        return false;

    if (match_key_event (key)) {
        if (m_pmf)
            return (performer->*m_pmf) ();
        else if (m_func)
            return m_func (performer);
    }

    return false;
}

bool
Action::match_key_event (const KeyEvent &key)
{
    return util_match_key_event (m_key_bindings, key, SCIM_KEY_CapsLockMask);
}

bool
Action::match_action_name (const char *name)
{
    return (m_name.compare (name) == 0);
}
