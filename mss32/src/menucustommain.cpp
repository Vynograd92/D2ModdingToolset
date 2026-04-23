/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Stanislav Egorov.
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

#include "menucustommain.h"
#include "dialoginterf.h"
#include "editboxinterf.h"
#include "gamesettings.h"
#include "interfaceutils.h"
#include "listbox.h"
#include "mempool.h"
#include "menuflashwait.h"
#include "menuphase.h"
#include "midgard.h"
#include "restrictions.h"
#include "settings.h"
#include "textboxinterf.h"
#include "textids.h"
#include "utils.h"
#include <spdlog/spdlog.h>

namespace hooks {

bool hasControl(game::CDialogInterf* dialog, const char* name)
{
    using namespace game;

    if (!dialog || !dialog->data)
        return false;

    auto& tree = dialog->data->childControls;

    for (auto it = tree.begin(); it != tree.end(); ++it) {
        if (strcmp(it->first, name) == 0)
            return true;
    }

    return false;
}

CMenuCustomMain::CMenuCustomMain(game::CMenuPhase* menuPhase)
    : CMenuCustomBase{this}
    , m_peerCallback{this}
    , m_lobbyCallback{this}
    , m_loginDialog{nullptr}
    , m_registerDialog{nullptr}
{
    using namespace game;

    CMenuMainApi::get().constructor(this, menuPhase);

    static RttiInfo<CMenuBaseVftable> rttiInfo = {};
    if (rttiInfo.locator == nullptr) {
        replaceRttiInfo(rttiInfo, this->vftable);
        rttiInfo.vftable.destructor = (CInterfaceVftable::Destructor)&destructor;
    }
    this->vftable = &rttiInfo.vftable;

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto txtTutorial = CDialogInterfApi::get().findTextBox(dialog, "TXT_TUTORIAL");
    if (txtTutorial) {
        auto text{getInterfaceText(textIds().lobby.serverName.c_str())};
        if (text.empty()) {
            text = "ONLINE LOBBY";
        }
        CTextBoxInterfApi::get().setString(txtTutorial, text.c_str());
    }

    // Since original button does not work anyway, this is the ideal spot
    setButtonCallback(dialog, "BTN_TUTORIAL", tutorialBtnHandler, this);

    if (hasControl(dialog, "BTN_LINK_1")) {
        auto button = CDialogInterfApi::get().findButton(dialog, "BTN_LINK_1");
        if (button)
            setButtonCallback(button, link1BtnHandler, this);
    }
    if (hasControl(dialog, "BTN_LINK_2")) {
        auto button = CDialogInterfApi::get().findButton(dialog, "BTN_LINK_2");
        if (button)
            setButtonCallback(button, link2BtnHandler, this);
    }

    if (hasControl(dialog, "BTN_LINK_3")) {
        auto button = CDialogInterfApi::get().findButton(dialog, "BTN_LINK_3");
        if (button)
            setButtonCallback(button, link3BtnHandler, this);
    }
}
void __fastcall CMenuCustomMain::link1BtnHandler(CMenuCustomMain* thisptr, int /*%edx*/)
{
    using namespace game;

    auto url = textIds().lobby.link1;
    if (url.empty())
        url = "https://example.com/link1";

    spdlog::debug("BTN_LINK_1 pressed");
    openInBrowser(url);
}

void __fastcall CMenuCustomMain::link2BtnHandler(CMenuCustomMain* thisptr, int /*%edx*/)
{
    using namespace game;

    auto url = textIds().lobby.link2;
    if (url.empty())
        url = "https://example.com/link2";

    spdlog::debug("BTN_LINK_2 pressed");
    openInBrowser(url);
}

void __fastcall CMenuCustomMain::link3BtnHandler(CMenuCustomMain* thisptr, int /*%edx*/)
{
    using namespace game;

    auto url = textIds().lobby.link3;
    if (url.empty())
        url = "https://example.com/link3";

    spdlog::debug("BTN_LINK_3 pressed");
    openInBrowser(url);
}

CMenuCustomMain ::~CMenuCustomMain()
{
    using namespace game;

    hideLoginDialog();
    hideRegisterDialog();

    auto service = CNetCustomService::get();
    if (service) {
        service->removeLobbyCallback(&m_lobbyCallback);
        service->removePeerCallback(&m_peerCallback);
    }

    CMenuMainApi::get().destructor(this);
}

void __fastcall CMenuCustomMain::destructor(CMenuCustomMain* thisptr, int /*%edx*/, char flags)
{
    thisptr->~CMenuCustomMain();

    if (flags & 1) {
        spdlog::debug("Free CMenuCustomMain memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

void __fastcall CMenuCustomMain::tutorialBtnHandler(CMenuCustomMain* thisptr, int /*%edx*/)
{
    using namespace game;

    const auto& midgardApi = CMidgardApi::get();

    auto service = (CNetCustomService*)game::Memory::get().allocate(sizeof(CNetCustomService));
    new (service) CNetCustomService();
    if (!service->connect()) {
        service->vftable->destructor(service, 1);

        auto message{getInterfaceText(textIds().lobby.connectStartFailed.c_str())};
        if (message.empty()) {
            message = "Connection start failed";
        }
        showMessageBox(message);
        return;
    }
    service->addPeerCallback(&thisptr->m_peerCallback);
    service->addLobbyCallback(&thisptr->m_lobbyCallback);
    midgardApi.setNetService(midgardApi.instance(), service, true, false);

    thisptr->showWaitDialog();
}

void CMenuCustomMain::showLoginDialog()
{
    using namespace game;

    if (m_loginDialog) {
        spdlog::debug("Error showing login dialog that is already shown");
        return;
    }

    m_loginDialog = (CLoginAccountInterf*)game::Memory::get().allocate(sizeof(CLoginAccountInterf));
    showInterface(new (m_loginDialog) CLoginAccountInterf(this));
}

void CMenuCustomMain::hideLoginDialog()
{
    if (m_loginDialog) {
        hideInterface(m_loginDialog);
        m_loginDialog->vftable->destructor(m_loginDialog, 1);
        m_loginDialog = nullptr;
    }
}

void CMenuCustomMain::showRegisterDialog()
{
    using namespace game;

    if (m_registerDialog) {
        spdlog::debug("Error showing register dialog that is already shown");
        return;
    }

    m_registerDialog = (CRegisterAccountInterf*)game::Memory::get().allocate(
        sizeof(CRegisterAccountInterf));
    showInterface(new (m_registerDialog) CRegisterAccountInterf(this));
}

void CMenuCustomMain::hideRegisterDialog()
{
    if (m_registerDialog) {
        hideInterface(m_registerDialog);
        m_registerDialog->vftable->destructor(m_registerDialog, 1);
        m_registerDialog = nullptr;
    }
}

void CMenuCustomMain::PeerCallback::onPacketReceived(DefaultMessageIDTypes type,
                                                     SLNet::RakPeerInterface* peer,
                                                     const SLNet::Packet* packet)
{
    using namespace game;

    switch (type) {
    case ID_CONNECTION_REQUEST_ACCEPTED:
        m_menu->hideWaitDialog();
        m_menu->showLoginDialog();
        break;

    case ID_CONNECTION_ATTEMPT_FAILED: {
        m_menu->hideWaitDialog();

        auto message{getInterfaceText(textIds().lobby.connectAttemptFailed.c_str())};
        if (message.empty()) {
            message = "Connection attempt failed";
        }
        showMessageBox(message);
        break;
    }

    case ID_NO_FREE_INCOMING_CONNECTIONS: {
        m_menu->hideWaitDialog();

        auto message{getInterfaceText(textIds().lobby.serverIsFull.c_str())};
        if (message.empty()) {
            message = "Lobby server is full";
        }
        showMessageBox(message);
        break;
    }

    case ID_DISCONNECTION_NOTIFICATION:
    case ID_CONNECTION_LOST:
        // We need to handle this ourself: the CMenuCustomBase callback is not attached because the
        // service is not there yet on the menu creation
        m_menu->onConnectionLost();
        break;
    }
}

void CMenuCustomMain::LobbyCallback::MessageResult(SLNet::Client_Login* message)
{
    using namespace game;

    m_menu->hideWaitDialog();

    switch (message->resultCode) {
    case SLNet::L2RC_SUCCESS: {
        m_menu->hideLoginDialog();

        CMenuPhase* menuPhase = m_menu->menuBaseData->menuPhase;
        CMenuPhaseApi::get().switchPhase(menuPhase, MenuTransition::Main2CustomLobby);
        break;
    }

    case SLNet::L2RC_Client_Login_HANDLE_NOT_IN_USE_OR_BAD_SECRET_KEY: {
        auto msg{getInterfaceText(textIds().lobby.noSuchAccountOrWrongPassword.c_str())};
        if (msg.empty()) {
            msg = "Wrong password or the account does not exist.";
        }
        showMessageBox(msg);
        break;
    }

    case SLNet::L2RC_Client_Login_BANNED: {
        auto msg{getInterfaceText(textIds().lobby.accountIsBanned.c_str())};
        if (msg.empty()) {
            msg = "The account is banned from the lobby server.";
        }
        showMessageBox(msg);
        break;
    }

    default: {
        auto msg{getInterfaceText(textIds().lobby.unableToLogin.c_str())};
        if (msg.empty()) {
            msg = "An unexpected error during login.\n%ERROR%";
        }
        replace(msg, "%ERROR%", SLNet::Lobby2ResultCodeDescription::ToEnglish(message->resultCode));
        showMessageBox(msg);
        break;
    }
    }
}

void CMenuCustomMain::LobbyCallback::MessageResult(SLNet::Client_RegisterAccount* message)
{
    using namespace game;

    m_menu->hideWaitDialog();

    switch (message->resultCode) {
    case SLNet::L2RC_SUCCESS: {
        m_menu->hideRegisterDialog();

        if (!CNetCustomService::get()->login(message->userName,
                                             message->createAccountParameters.password)) {
            auto msg{getInterfaceText(textIds().lobby.unableToLoginAfterRegistration.c_str())};
            if (msg.empty()) {
                msg = "An unexpected error trying to login after successful registration.";
            }
            showMessageBox(msg);
            break;
        }

        m_menu->showWaitDialog();
        break;
    }

    case SLNet::L2RC_Client_RegisterAccount_HANDLE_ALREADY_IN_USE: {
        auto msg{getInterfaceText(textIds().lobby.accountNameAlreadyInUse.c_str())};
        if (msg.empty()) {
            msg = "The account name already exists.";
        }
        showMessageBox(msg);
        break;
    }

    default: {
        auto msg{getInterfaceText(textIds().lobby.unableToRegister.c_str())};
        if (msg.empty()) {
            msg = "An unexpected error during account registration.\n%ERROR%";
        }
        replace(msg, "%ERROR%", SLNet::Lobby2ResultCodeDescription::ToEnglish(message->resultCode));
        showMessageBox(msg);
        break;
    }
    }
}

CMenuCustomMain::CLoginAccountInterf::CLoginAccountInterf(CMenuCustomMain* menu)
    : CPopupDialogCustomBase{this, loginAccountDialogName}
    , m_menu{menu}
{
    using namespace game;

    const auto& restrictions = gameRestrictions();

    CPopupDialogInterfApi::get().constructor(this, loginAccountDialogName, nullptr);

    setButtonCallback(*dialog, "BTN_CANCEL", cancelBtnHandler, this);
    setButtonCallback(*dialog, "BTN_OK", okBtnHandler, this);
    setButtonCallback(*dialog, "BTN_REGISTER", registerBtnHandler, this);

    // Using EditFilter::Names for consistency with other game menus like CMenuNewSkirmishMulti.
    // Account name shares player name restrictions as they are interchangeable (at least for now).
    setEditBoxData(*dialog, "EDIT_ACCOUNT_NAME", EditFilter::Names,
                   *restrictions.playerNameMaxLength, false);
    setEditBoxData(*dialog, "EDIT_PASSWORD", EditFilter::Names,
                   CNetCustomService::passwordMaxLength, true);

    const CMidgard* midgard{CMidgardApi::get().instance()};
    const GameSettings* gameSettings{*midgard->data->settings};
    std::string username = gameSettings->defaultPlayerName.string;

    setEditBoxText(*dialog, "EDIT_ACCOUNT_NAME", username.c_str(), true);
    setEditBoxText(*dialog, "EDIT_PASSWORD", lobbySettings().password.c_str(), true);
    if (!username.empty()) {
        auto editPassword = CDialogInterfApi::get().findEditBox(*dialog, "EDIT_PASSWORD");
        CEditBoxInterfApi::get().setFocus(editPassword);
    }
}

void __fastcall CMenuCustomMain::CLoginAccountInterf::okBtnHandler(CLoginAccountInterf* thisptr,
                                                                   int /*%edx*/)
{
    using namespace game;

    lobbySettings().password = getEditBoxText(*thisptr->dialog, "EDIT_PASSWORD");
    std::string username = getEditBoxText(*thisptr->dialog, "EDIT_ACCOUNT_NAME");

    const CMidgard* midgard{CMidgardApi::get().instance()};
    GameSettingsApi::get().setDefaultPlayerName(midgard->data->settings, username.c_str());

    if (!CNetCustomService::get()->login(username.c_str(), lobbySettings().password.c_str())) {
        auto message{getInterfaceText(textIds().lobby.invalidAccountNameOrPassword.c_str())};
        if (message.empty()) {
            message = "Account name or password are either empty or invalid.";
        }
        showMessageBox(message);
        return;
    }

    thisptr->m_menu->showWaitDialog();
}

void __fastcall CMenuCustomMain::CLoginAccountInterf::cancelBtnHandler(CLoginAccountInterf* thisptr,
                                                                       int /*%edx*/)
{
    using namespace game;

    const auto& midgardApi = CMidgardApi::get();

    spdlog::debug("User canceled logging in");
    thisptr->m_menu->hideLoginDialog();

    // Drop connection to the lobby server
    midgardApi.clearNetworkStateAndService(midgardApi.instance());
}

void __fastcall CMenuCustomMain::CLoginAccountInterf::registerBtnHandler(
    CLoginAccountInterf* thisptr,
    int /*%edx*/)
{
    auto menu = thisptr->m_menu;
    menu->hideLoginDialog();
    menu->showRegisterDialog();
}

CMenuCustomMain::CRegisterAccountInterf::CRegisterAccountInterf(CMenuCustomMain* menu)
    : CPopupDialogCustomBase{this, registerAccountDialogName}
    , m_menu{menu}
{
    using namespace game;

    const auto& restrictions = gameRestrictions();

    CPopupDialogInterfApi::get().constructor(this, registerAccountDialogName, nullptr);

    setButtonCallback(*dialog, "BTN_CANCEL", cancelBtnHandler, this);
    setButtonCallback(*dialog, "BTN_OK", okBtnHandler, this);

    // Using EditFilter::Names for consistency with other game menus like CMenuNewSkirmishMulti.
    // Account name shares player name restrictions as they are interchangeable (at least for now).
    setEditBoxData(*dialog, "EDIT_ACCOUNT_NAME", EditFilter::Names,
                   *restrictions.playerNameMaxLength, false);
    // TODO: add repeat-password edit, mask both edits with asterisks
    setEditBoxData(*dialog, "EDIT_PASSWORD", EditFilter::Names,
                   CNetCustomService::passwordMaxLength, false);
}

void __fastcall CMenuCustomMain::CRegisterAccountInterf::okBtnHandler(
    CRegisterAccountInterf* thisptr,
    int /*%edx*/)
{
    auto dialog = *thisptr->dialog;
    if (!CNetCustomService::get()->registerAccount(getEditBoxText(dialog, "EDIT_ACCOUNT_NAME"),
                                                   getEditBoxText(dialog, "EDIT_PASSWORD"))) {
        auto message{getInterfaceText(textIds().lobby.invalidAccountNameOrPassword.c_str())};
        if (message.empty()) {
            message = "Account name or password are either empty or invalid.";
        }
        showMessageBox(message);
        return;
    }

    thisptr->m_menu->showWaitDialog();
}

void __fastcall CMenuCustomMain::CRegisterAccountInterf::cancelBtnHandler(
    CRegisterAccountInterf* thisptr,
    int /*%edx*/)
{
    using namespace game;

    const auto& midgardApi = CMidgardApi::get();

    spdlog::debug("User canceled register account");
    thisptr->m_menu->hideRegisterDialog();

    // Drop connection to the lobby server
    midgardApi.clearNetworkStateAndService(midgardApi.instance());
}

} // namespace hooks
