/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2022 Stanislav Egorov.
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

#ifndef INTERFACEUTILS_H
#define INTERFACEUTILS_H

#include "menubase.h"
#include <string>

namespace game {
struct LAttackSource;
struct IEncUnitDescriptor;
struct CMidUnitDescriptor;
struct CUnitTypeDescriptor;
struct CLeaderUnitDescriptor;
struct CDynUpgrade;
struct CMidUnit;
struct IUsUnit;
struct TUsUnitImpl;
struct CPictureInterf;
struct CButtonInterf;
struct IMqImage2;
struct CBorderedImg;

enum class AttackSourceId : int;
enum class AttackClassId : int;
enum class BorderType : int;
enum class EditFilter : int;
} // namespace game

namespace hooks {

const game::CMidUnitDescriptor* castToMidUnitDescriptor(const game::IEncUnitDescriptor* descriptor);
const game::CUnitTypeDescriptor* castToUnitTypeDescriptor(
    const game::IEncUnitDescriptor* descriptor);
const game::CLeaderUnitDescriptor* castToLeaderUnitDescriptor(
    const game::IEncUnitDescriptor* descriptor);
const game::CMidUnit* getUnit(const game::IEncUnitDescriptor* descriptor);
/** Returns either global (from Gunits.dbf), generated (leveled) or modified unit implementation. */
const game::IUsUnit* getModifiedUnitImpl(const game::IEncUnitDescriptor* descriptor);
/** Returns either global (from Gunits.dbf) or generated (leveled) unit implementation. */
const game::TUsUnitImpl* getUnitImpl(const game::IEncUnitDescriptor* descriptor);
bool hasCriticalHitLeaderAbility(const game::IEncUnitDescriptor* descriptor);

std::string getNumberText(int value, bool percent);
std::string getBonusNumberText(int bonus, bool percent, bool reverse);
std::string addBonusNumberText(const std::string& base, int bonus, bool percent, bool reverse);
std::string getModifiedNumberText(int value, int base, bool percent);
std::string getModifiedNumberTextFull(int value, int base, bool percent);
std::string getModifiedNumberTextReverseBonus(int value, int base, bool percent);
std::string getModifiedNumberTextTotal(int value, int base, bool percent);
std::string getModifiedNumberTextTotalReverseBonus(int value, int base, bool percent);
std::string getModifiedStringText(const std::string& value, bool modified);

std::string getRemovedAttackWardText(const std::string& value);
std::string getAttackSourceText(const game::LAttackSource* source);
std::string getAttackSourceText(game::AttackSourceId id);
std::string getAttackClassText(game::AttackClassId id);

bool getDynUpgradesToDisplay(game::IEncUnitDescriptor* descriptor,
                             const game::CDynUpgrade** upgrade1,
                             const game::CDynUpgrade** upgrade2);
void addDynUpgradeLevelToField(std::string& text, const char* field, int level);
void addDynUpgradeTextToField(std::string& text, const char* field, int upgrade1, int upgrade2);

void setCenteredImage(game::CPictureInterf* picture, game::IMqImage2* image);
game::CBorderedImg* createBorderedImage(game::IMqImage2* image, game::BorderType borderType);
// callback is void* for convenience, should correspond to game::CMenuBaseApi::Api::ButtonCallback
void setButtonCallback(game::CDialogInterf* dialog,
                       const char* buttonName,
                       void* callback,
                       void* callbackParam);
void setButtonCallbackSafe(game::CDialogInterf* dialog,
                           const char* buttonName,
                           void* callback,
                           void* callbackParam);
void setButtonCallback(game::CButtonInterf* button, void* callback, void* callbackParam);
void setEditBoxData(game::CDialogInterf* dialog,
                    const char* editName,
                    game::EditFilter filter,
                    int length,
                    bool password);
const char* getEditBoxText(game::CDialogInterf* dialog, const char* editName);
void setEditBoxText(game::CDialogInterf* dialog,
                    const char* editName,
                    const char* text,
                    bool moveCursorPos);
void openInBrowser(const std::string& url);

} // namespace hooks

#endif // INTERFACEUTILS_H
