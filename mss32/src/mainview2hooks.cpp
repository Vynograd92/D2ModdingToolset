/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2022 Vladimir Makeev.
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

#include "cmdstackvisitmsg.h"
#include "fortification.h"
#include "midruin.h"
#include "fortview.h"
#include "ruinview.h"
#include "siteview.h"
#include "idview.h"
#include "point.h"
#include "playerview.h"
#include "d2string.h"
#include "dialoginterf.h"
#include "dynamiccast.h"
#include "gameimages.h"
#include "gameutils.h"
#include "isolayers.h"
#include "mainview2.h"
#include "mainview2hooks.h"
#include "mapgraphics.h"
#include "midclient.h"
#include "midclientcore.h"
#include "midcommandqueue2.h"
#include "midobjectlock.h"
#include "midsite.h"
#include "midtaskopeninterfparamresmarket.h"
#include "originalfunctions.h"
#include "phasegame.h"
#include "scenarioinfo.h"
#include "sitecategoryhooks.h"
#include "settings.h"
#include "savegamehooks.h"
#include "taskmanager.h"
#include "textboxinterf.h"
#include "togglebutton.h"


#include <spdlog/spdlog.h>
#include <textids.h>
#include <utils.h>



namespace hooks {

static bool gridVisible{false};

static bool objectsVisible{true};

static std::string getCityPrefixFromSubrace(game::SubRaceId subraceId)
{
    using namespace game;
    switch (subraceId) {
    case SubRaceId::Dwarf:
        return "D"; // гномы
    case SubRaceId::Heretic:
        return "E"; // демоны
    case SubRaceId::Human:
        return "H"; // импери€
    case SubRaceId::Elf:
        return "HF"; // эльфы
    case SubRaceId::Undead:
        return "U"; // нежить
    case SubRaceId::Neutral:
    case SubRaceId::NeutralHuman:
    case SubRaceId::NeutralElf:
    case SubRaceId::NeutralGreenSkin:
    case SubRaceId::NeutralDragon:
    case SubRaceId::NeutralMarsh:
    case SubRaceId::NeutralWater:
    case SubRaceId::NeutralBarbarian:
    case SubRaceId::NeutralWolf:
    default:
        return ""; // нейтралы без буквы
    }
}


static void toggleMapLayers(bool visible)
{
    using namespace game;

    const auto& mapGraphics{MapGraphicsApi::get()};
    auto& layers = isoLayers();

    // —писок слоЄв, которые будем скрывать/показывать
    CIsoLayer* targetLayers[] = {layers.capitals, layers.villages, layers.sites, layers.ruins};

    if (!visible) {
        spdlog::info("Hiding object layers: capitals, villages, sites, ruins");
        for (auto* layer : targetLayers) {
            if (layer)
                mapGraphics.hideLayerImages(layer);
        }
    } else {
        spdlog::info("Restoring object layers: capitals, villages, sites, ruins");

        const auto& imagesApi = GameImagesApi::get();
        GameImagesPtr imagesPtr;
        imagesApi.getGameImages(&imagesPtr);
        auto images = *imagesPtr.data;

        // ѕолучаем dummy-изображение (любое, чтобы "разбудить" слой)
        const IMqImage2* dummyImg = imagesApi.getImage(images->isoCmon, "GRID", 0, true,
                                                       images->log);
        if (!dummyImg) {
            spdlog::warn("Dummy GRID image not found, cannot refresh layers");
            imagesApi.createOrFreeGameImages(&imagesPtr, nullptr);
            return;
        }

        CMqPoint tile{0, 0}; // безопасна€ точка на карте

        for (auto* layer : targetLayers) {
            if (layer) {
                mapGraphics.showImageOnMap(&tile, layer, dummyImg, 0, 0);
            }
        }

        imagesApi.createOrFreeGameImages(&imagesPtr, nullptr);

        spdlog::info("Map layers reactivated (dummy draw triggered)");
    }
}

static void highlightViewsObjects(game::CPhaseGame* phaseGame, bool drawGridOverlay)
{
    using namespace game;
    if (!phaseGame)
        return;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    if (!objectMap)
        return;

    const auto& mapGraphics = MapGraphicsApi::get();
    const auto& imagesApi = GameImagesApi::get();

    GameImagesPtr imagesPtr;
    imagesApi.getGameImages(&imagesPtr);
    auto images = *imagesPtr.data;

    const auto& idApi = CMidgardIDApi::get();

    const IMqImage2* gridImg = nullptr;
    if (drawGridOverlay)
        gridImg = imagesApi.getImage(images->isoCmon, "GRID", 0, true, images->log);

    IteratorPtr itBegin, itEnd;
    objectMap->vftable->begin(objectMap, &itBegin);
    objectMap->vftable->end(objectMap, &itEnd);

    auto* it = itBegin.data;
    auto* endIt = itEnd.data;

    for (; !it->vftable->end(it, endIt); it->vftable->advance(it)) {
        CMidgardID* id = it->vftable->getObjectId(it);
        if (!id)
            continue;

        IdType type = idApi.getType(id);
        IMidScenarioObject* obj = objectMap->vftable->findScenarioObjectById(objectMap, id);
        if (!obj)
            continue;

        if (type == IdType::Fortification) {
            auto* fort = static_cast<CFortification*>(obj);
            auto pos = fort->mapElement.position;
            int sx = fort->mapElement.sizeX;
            int sy = fort->mapElement.sizeY;

            if (drawGridOverlay) {
                for (int dx = 0; dx < sx; ++dx)
                    for (int dy = 0; dy < sy; ++dy) {
                        CMqPoint tile{pos.x + dx, pos.y + dy};
                        mapGraphics.showImageOnMap(&tile, isoLayers().grid, gridImg, 0, 0);
                    }
            } else {
                const IMqImage2* fortImg = imagesApi.getImage(images->isoCmon, "G000FT0000DW0", 0, true,
                                                              images->log);
                if (fortImg)
                    mapGraphics.showImageOnMap(&pos, isoLayers().capitals, fortImg, 0, 0);
                    mapGraphics.showImageOnMap(&pos, isoLayers().villages, fortImg, 0, 0);
            }
        } else if (type == IdType::Ruin) {
            auto* ruin = static_cast<CMidRuin*>(obj);
            auto pos = ruin->mapElement.position;
            int sx = ruin->mapElement.sizeX;
            int sy = ruin->mapElement.sizeY;

            if (drawGridOverlay) {
                for (int dx = 0; dx < sx; ++dx)
                    for (int dy = 0; dy < sy; ++dy) {
                        CMqPoint tile{pos.x + dx, pos.y + dy};
                        mapGraphics.showImageOnMap(&tile, isoLayers().grid, gridImg, 0, 0);
                    }
            } else {
                const IMqImage2* ruinImg = imagesApi.getImage(images->isoCmon, "RUIN",
                                                              ruin->imageIndex, true, images->log);
                if (ruinImg)
                    mapGraphics.showImageOnMap(&pos, isoLayers().ruins, ruinImg, 0, 0);
            }
        } else if (type == IdType::Site) {
            auto* site = static_cast<CMidSite*>(obj);
            auto pos = site->mapElement.position;
            int sx = site->mapElement.sizeX;
            int sy = site->mapElement.sizeY;

            if (drawGridOverlay) {
                for (int dx = 0; dx < sx; ++dx)
                    for (int dy = 0; dy < sy; ++dy) {
                        CMqPoint tile{pos.x + dx, pos.y + dy};
                        mapGraphics.showImageOnMap(&tile, isoLayers().grid, gridImg, 0, 0);
                    }
            } else {
                const IMqImage2* siteImg = imagesApi.getImage(images->isoCmon, site->imgIntf,
                                                              site->imgIso, true, images->log);
                if (siteImg)
                    mapGraphics.showImageOnMap(&pos, isoLayers().sites, siteImg, 0, 0);
            }
        }
    }

    imagesApi.createOrFreeGameImages(&imagesPtr, nullptr);
}


static void __fastcall mainView2OnToggleObjectsViaViews(game::CMainView2* thisptr,
                                                        int /*%edx*/,
                                                        bool toggleOn,
                                                        game::CToggleButton*)
{
    using namespace game;

    objectsVisible = toggleOn;

    const auto& mapGraphics = MapGraphicsApi::get();
    auto& layers = isoLayers();

    if (!objectsVisible) {
        toggleMapLayers(false);
        highlightViewsObjects(thisptr->phaseGame, true); // ?? рисуем сетку
    } else {
        //MapGraphicsApi::get().hideLayerImages(isoLayers().grid); // убираем сетку
        highlightViewsObjects(thisptr->phaseGame, false);        // ?? восстанавливаем объекты
    }
}


static void showGrid(int mapSize)
{
    using namespace game;

    const auto& imagesApi = GameImagesApi::get();

    GameImagesPtr imagesPtr;
    imagesApi.getGameImages(&imagesPtr);
    auto images = *imagesPtr.data;

    const auto& mapGraphics{MapGraphicsApi::get()};

    for (int x = 0; x < mapSize; ++x) {
        for (int y = 0; y < mapSize; ++y) {
            auto gridImage{imagesApi.getImage(images->isoCmon, "GRID", 0, true, images->log)};
            if (!gridImage) {
                continue;
            }

            const CMqPoint mapPosition{x, y};
            mapGraphics.showImageOnMap(&mapPosition, isoLayers().grid, gridImage, 0, 0);
        }
    }

    imagesApi.createOrFreeGameImages(&imagesPtr, nullptr);
}

static void hideGrid()
{
    using namespace game;

    MapGraphicsApi::get().hideLayerImages(isoLayers().grid);
}

static void __fastcall mainView2OnToggleGrid(game::CMainView2* thisptr,
                                             int /*%edx*/,
                                             bool toggleOn,
                                             game::CToggleButton*)
{
    gridVisible = toggleOn;

    if (gridVisible) {
        auto objectMap{game::CPhaseApi::get().getDataCache(&thisptr->phaseGame->phase)};
        auto scenarioInfo{getScenarioInfo(objectMap)};

        showGrid(scenarioInfo->mapSize);
        return;
    }

    hideGrid();
}

void __fastcall mainView2ShowIsoDialogHooked(game::CMainView2* thisptr, int /*%edx*/)
{
    using namespace game;

    const auto& mainViewApi{CMainView2Api::get()};

    mainViewApi.showDialog(thisptr, nullptr);

    static const char buttonName[]{"TOG_GRID"};

    const auto& dialogApi{CDialogInterfApi::get()};
    auto dialog{thisptr->dialogInterf};

    if (!dialogApi.findControl(dialog, buttonName)) {
        // Grid button was not added to Interf.dlg, skip
        return;
    }

    auto toggleButton{dialogApi.findToggleButton(dialog, buttonName)};
    if (!toggleButton) {
        // Control was found, but it is not CToggleButton
        spdlog::error("{:s} in {:s} must be a toggle button", buttonName, dialog->data->dialogName);
        return;
    }

    using ButtonCallback = CMainView2Api::Api::ToggleButtonCallback;

    ButtonCallback callback{};
    callback.callback = (ButtonCallback::Callback)&mainView2OnToggleGrid;

    SmartPointer functor;
    mainViewApi.createToggleButtonFunctor(&functor, 0, thisptr, &callback);

    const auto& buttonApi{CToggleButtonApi::get()};
    buttonApi.assignFunctor(dialog, buttonName, dialog->data->dialogName, &functor, 0);
    SmartPointerApi::get().createOrFreeNoDtor(&functor, nullptr);

    buttonApi.setChecked(toggleButton, gridVisible);

    static const char buttonNameObjects[]{"TOG_OBJECTS"};

    if (dialogApi.findControl(dialog, buttonNameObjects)) {
        auto toggleButton{dialogApi.findToggleButton(dialog, buttonNameObjects)};
        if (toggleButton) {
            using ButtonCallback = CMainView2Api::Api::ToggleButtonCallback;
            ButtonCallback callback{};
            callback.callback = (ButtonCallback::Callback)&mainView2OnToggleObjectsViaViews;

            SmartPointer functor;
            mainViewApi.createToggleButtonFunctor(&functor, 0, thisptr, &callback);

            const auto& buttonApi{CToggleButtonApi::get()};
            buttonApi.assignFunctor(dialog, buttonNameObjects, dialog->data->dialogName, &functor,
                                    0);
            SmartPointerApi::get().createOrFreeNoDtor(&functor, nullptr);
            buttonApi.setChecked(toggleButton, objectsVisible);
        }
    }

    static const char turnTextName[]{"TXT_TURN"};
    const auto& textApi = CTextBoxInterfApi::get();

    auto textBox = dialogApi.findTextBox(dialog, turnTextName);
    if (textBox && textBox->data) {
        auto objectMap = CPhaseApi::get().getDataCache(&thisptr->phaseGame->phase);
        auto scenarioInfo = getScenarioInfo(objectMap);

        if (scenarioInfo) {
            // Extract the current text from the DLG
            std::string text = textBox->data->text.string ? textBox->data->text.string : "";
            spdlog::debug("Current turn text before update: '{}'", text);

            // If the text is empty or does not contain the %TURN% placeholder
            if (text.empty() || text.find("%TURN%") == std::string::npos) {
                // Try to load the text ID from textids.lua
                std::string textId = hooks::textIds().interf.currentTurn;
                if (!textId.empty()) {
                    auto idText = getInterfaceText(textId.c_str());
                    if (!idText.empty()) {
                        text = idText;
                        spdlog::debug(
                            "TXT_TURN fallback loaded from textIds().interf.currentTurn = '{}'",
                            textId);
                    }
                }

                // If both the DLG and Lua values are missing, use the default template
                if (text.empty()) {
                    spdlog::debug("TXT_TURN missing or no placeholder Ч using default template");
                    text = "Current turn %TURN%";
                }
            }

            // Replace the %TURN% placeholder with the current turn number
            replace(text, "%TURN%", fmt::format("{}", scenarioInfo->currentTurn));

            // Set the text back to the UI element
            textApi.setString(textBox, text.c_str());
            spdlog::debug("TXT_TURN updated to: '{}'", text);
        }
    } else {
        spdlog::warn("TXT_TURN not found in dialog");
    }
}

void __fastcall mainView2HandleCmdStackVisitMsgHooked(game::CMainView2* thisptr,
                                                      int /*%edx*/,
                                                      const game::CCommandMsg* stackVisitMsg)
{
    using namespace game;

    const auto& dynamicCast{RttiApi::get().dynamicCast};
    const auto& rtti{RttiApi::rtti()};

    auto msg{(const CCmdStackVisitMsg*)dynamicCast(stackVisitMsg, 0, rtti.CCommandMsgType,
                                                   rtti.CCmdStackVisitMsgType, 0)};

    const auto& phaseApi{CPhaseApi::get()};
    CPhase* phase{&thisptr->phaseGame->phase};

    IMidgardObjectMap* objectMap{phaseApi.getDataCache(phase)};

    auto obj{objectMap->vftable->findScenarioObjectById(objectMap, &msg->siteId)};
    auto site{
        (const CMidSite*)dynamicCast(obj, 0, rtti.IMidScenarioObjectType, rtti.CMidSiteType, 0)};
    if (!customSiteCategories().exists
        || customSiteCategories().resourceMarket.id != site->siteCategory.id) {
        return getOriginalFunctions().handleCmdStackVisitMsg(thisptr, stackVisitMsg);
    }

    // Handle stack visiting resource market
    ITaskManagerHolder* holder{&thisptr->taskManagerHolder};
    CTaskManager* taskManager{holder->vftable->getTaskManager(holder)};

    const CMidgardID& siteId{msg->siteId};
    const CMidgardID& visitorStackId{msg->visitorStackId};

    ITask* task{createMidTaskOpenInterfParamResMarket(taskManager, thisptr->phaseGame,
                                                      visitorStackId, siteId)};

    auto commandQueue{phaseApi.getCommandQueue(phase)};
    CMidCommandQueue2Api::get().processCommands(commandQueue);

    CTaskManagerApi::get().setCurrentTask(taskManager, task);
}

void __fastcall mainView2CommandQueueCallbackHooked(game::CMainView2* thisptr, int)
{
    using namespace game;

    static int lastTurn = -1;
    static bool isAuthoritativeInstance = false;
    static bool pendingBattleSave = false;

    auto original = getOriginalFunctions().mainView2CommandQueueCallback;

    if (!thisptr || !thisptr->phaseGame)
        return original(thisptr);

    auto phaseGame = thisptr->phaseGame;

    const auto& phaseApi = CPhaseApi::get();
    const auto& commandQueueApi = CMidCommandQueue2Api::get();

    auto commandQueue = phaseApi.getCommandQueue(&phaseGame->phase);

    // =====================================================
    // Deferred BattleSave (вне CQ)
    // =====================================================

    if (pendingBattleSave) {
        pendingBattleSave = false;

        auto midgard = CMidgardApi::get().instance();

        if (midgard && midgard->data && midgard->data->host) {
            auto* impl = midgard->data->interfManager.data;

            if (impl) {
                CInterfManager* manager = static_cast<CInterfManager*>(impl);

                CInterface* topInterf = manager->vftable->getTopmostInterface(manager);

                if (topInterf) {
                    using SaveFn = void(__thiscall*)(void*, char*);
                    SaveFn stratSave = reinterpret_cast<SaveFn>(0x0048FED7);

                    stratSave(topInterf, (char*)"BattleSave");
                }
            }
        }

        return original(thisptr);
    }

    // =====================================================
    // CQ обработка
    // =====================================================

    auto message = commandQueueApi.front(commandQueue);

    if (message) {
        auto messageId = message->vftable->getId(message);

        if (messageId == CommandMsgId::MoveStackEnd) {
            phaseGame->data->midObjectLock->patched.movingStack = false;
            commandQueueApi.processCommands(commandQueue);
            return;
        }
    }

    auto midgard = CMidgardApi::get().instance();
    auto data = midgard ? midgard->data : nullptr;

    auto objectMap = phaseApi.getDataCache(&phaseGame->phase);
    auto scenarioInfo = getScenarioInfo(objectMap);

    int currentTurn = scenarioInfo ? scenarioInfo->currentTurn : 0;
    int maxSlots = hooks::userSettings().autoSaveSlots;

    // -----------------------------------------------------
    // TurnStart Ч только authoritative флаг
    // -----------------------------------------------------

    if (currentTurn > 0 && currentTurn != lastTurn) {
        lastTurn = currentTurn;

        if (data && data->host)
            isAuthoritativeInstance = true;
        else
            isAuthoritativeInstance = false;
    }

    if (!message || !data || maxSlots <= 0)
        return original(thisptr);

    auto messageId = message->vftable->getId(message);

    int slot = (currentTurn > 0) ? ((currentTurn - 1) % maxSlots) + 1 : 1;

    // -----------------------------------------------------
    // BattleEnd ? ставим флаг
    // -----------------------------------------------------

    if (messageId == CommandMsgId::BattleEnd) {
        if (isAuthoritativeInstance)
            pendingBattleSave = true;

        return original(thisptr);
    }

    // -----------------------------------------------------
    // EndTurn
    // -----------------------------------------------------

    CMidgardID currentPlayerId = phaseGame->data->currentPlayerId;
    CMidgardID msgPlayerId = message->playerId;

    if (msgPlayerId != currentPlayerId)
        return original(thisptr);

    if (messageId == CommandMsgId::EndTurn) {
        char name[64];

        std::snprintf(name, sizeof(name), "TurnEnd_AutoSave_%03d", slot);

        CPhaseGameApi::get().sendCSaveGameMsg(phaseGame, name, true);
    }

    return original(thisptr);
}

} // namespace hooks
