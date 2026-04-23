/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2025 Vladimir Makeev.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CHATINTERFHOOKS_H
#define CHATINTERFHOOKS_H

#include "imagepointlist.h"   
#include "mqrect.h"           
#include "chatinterf.h"      

namespace hooks {


void __fastcall chatInterfListBoxDisplayHooked(game::CChatInterf* thisptr,
                                               int /*%edx*/,
                                               game::ImagePointList* contents,
                                               const game::CMqRect* lineArea,
                                               int index,
                                               bool selected);

} // namespace hooks

#endif // CHATINTERFHOOKS_H
