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

#ifndef __SCIM_ANTHY_ACTION_H__
#define __SCIM_ANTHY_ACTION_H__

#include <scim.h>
#include <scim_event.h>

using namespace scim;

class AnthyInstance;

namespace scim_anthy {

class Action
{
    typedef bool (AnthyInstance::*PMF) (void);
    typedef bool (*Func)               (AnthyInstance *anthy);

public:
    Action  (const String &name, const String &key_bindings, PMF pmf);
    Action  (const String &name, const String &key_bindings, Func func);
    ~Action ();

public:
    bool perform (AnthyInstance  *performer);
    bool perform (AnthyInstance  *performer,
                  const KeyEvent &key);

    bool match_action_name (const char *name);

private:
    bool match_key_event (const KeyEvent &key);

private:
    String         m_name;
    String         m_desc;
    PMF            m_pmf;
    Func           m_func;
    KeyEventList   m_key_bindings;
};

}

#endif /* __SCIM_ANTHY_ACTION_H__ */
