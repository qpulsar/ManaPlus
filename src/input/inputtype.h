/*
 *  The ManaPlus Client
 *  Copyright (C) 2012-2015  The ManaPlus Developers
 *
 *  This file is part of The ManaPlus Client.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INPUT_INPUTTYPE_H
#define INPUT_INPUTTYPE_H

// hack to avoid conflicts with windows headers.
#ifdef KEYBOARD
#undef KEYBOARD
#endif
#ifdef MOUSE
#undef MOUSE
#endif

namespace InputType
{
    enum Type
    {
        UNKNOWN = 0,
        KEYBOARD = 1,
        MOUSE = 2,
        JOYSTICK = 3
    };
}

#endif  // INPUT_INPUTTYPE_H
