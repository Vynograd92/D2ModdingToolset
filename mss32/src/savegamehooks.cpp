/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2020 Vladimir Makeev.
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

#include "savegamehooks.h"

#include "hooks.h"
#include "originalfunctions.h"

namespace hooks {

// ============================================================
// Original function pointer
// ============================================================

static game::CPhaseGameApi::Api::SendCSaveGameMsg originalSendCSaveGameMsg = nullptr;


// ============================================================
// Recursion guard
// ============================================================

static bool isInternalAutoSave(const char* name)
{
    return std::strncmp(name, "TurnStart_", 10) == 0 || std::strncmp(name, "TurnEnd_", 8) == 0
           || std::strncmp(name, "BattleSave", 10) == 0 || std::strncmp(name, "AutoSave_", 9) == 0;
}

// ============================================================
// Hooked autosave vanilla 
// ============================================================

void __fastcall sendSaveGameMsgHooked(game::CPhaseGame* thisptr,
                                      int /*edx*/,
                                      char* saveFilename,
                                      bool autosave)
{
    using namespace game;

    // Non-autosave - original only
    if (!autosave) {
        originalSendCSaveGameMsg(thisptr, saveFilename, autosave);
        return;
    }

    // Prevent recursion
    if (isInternalAutoSave(saveFilename)) {
        originalSendCSaveGameMsg(thisptr, saveFilename, autosave);
        return;
    }

    // 1 Create vanilla autosave
    originalSendCSaveGameMsg(thisptr, saveFilename, autosave);

}

// ============================================================
// Hook registration
// ============================================================

void registerSaveGameHooks(Hooks& hooks)
{
    hooks.push_back({(void*)game::CPhaseGameApi::get().sendCSaveGameMsg,
                     (void*)&sendSaveGameMsgHooked, (void**)&originalSendCSaveGameMsg});
}

} // namespace hooks
