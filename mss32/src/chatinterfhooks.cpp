#include "chatinterfhooks.h"
#include "chatinterf.h"
#include "dialoginterf.h"
#include "d2vector.h"
#include "editboxinterf.h"
#include "formattedtext.h"
#include "gameutils.h"
#include "imagepointlist.h"
#include "lovechat.h"
#include "midscenvariables.h"
#include "originalfunctions.h"
#include "phasegame.h"
#include "listbox.h"
#include "netmsg.h"
#include "mempool.h"
#include "spdlog/spdlog.h"
#include "textmessage.h"
#include "menucustombase.h"
#include "menuphase.h"
#include "utils.h"
#include <cstring>
#include <ctime>
#include <detours.h>
#include <fmt/format.h>
#include <regex>
#include <unordered_map>
#include <windows.h>

using namespace game;


namespace game {
// ================================================================
// ?? Утилиты
// ================================================================

template <typename T>
inline void vectorPushBack(game::Vector<T>& vec, const T& value)
{
    if (vec.end >= vec.allocatedMemEnd)
        return;
    *vec.end = value;
    ++vec.end;
}

inline std::string stripFormattingCodes(const char* text)
{
    if (!text)
        return {};

    std::string s{text};

    // Удаляем управляющие последовательности игры
    s = std::regex_replace(s, std::regex(R"(\\f[A-Za-z]+;)"), "");
    s = std::regex_replace(s, std::regex(R"(\\c[0-9]+;[0-9]+;[0-9]+;)"), "");
    s = std::regex_replace(s, std::regex(R"(\\o[0-9]+;[0-9]+;[0-9]+;)"), "");
    s = std::regex_replace(s, std::regex(R"(\\v[A-Za-z]+;)"), "");
    s = std::regex_replace(s, std::regex(R"(^\s+|\s+$)"), "");
    return s;
}

inline bool isCtrlPressed()
{
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
}

inline void copyToClipboard(const std::string& text)
{
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}


template <typename T>
T* begin(const Vector<T>& v)
{
    return v.bgn;
}

template <typename T>
T* end(const Vector<T>& v)
{
    return v.end;
}
} // namespace game

namespace hooks {

// ================================================================
// ?? Hook chatInterfListBoxDisplayHandler
// ================================================================
static std::unordered_map<const TextMessage*, int> timeCache;
static void* loveChatMethod1Ptr = nullptr;

static bool g_PendingSendVariables = false;
static game::CChatInterf* g_LastChatInterf = nullptr;


void __fastcall chatInterfListBoxDisplayHooked(CChatInterf* thisptr,
                                               int /*%edx*/,
                                               ImagePointList* contents,
                                               const CMqRect* lineArea,
                                               int index,
                                               bool selected)
{
    auto& orig = getOriginalFunctions();

    if (!thisptr || !thisptr->chatData) {
        if (orig.chatInterfListBoxDisplayHandler)
            orig.chatInterfListBoxDisplayHandler(thisptr, contents, lineArea, index, selected);
        return;
    }

    // === Увеличиваем лимит символов в поле ввода ===
    const auto& dialogApi = game::CDialogInterfApi::get();
    const auto& editApi = game::CEditBoxInterfApi::get();

    if (auto popupData = thisptr->popupData; popupData && popupData->dialog) {
        auto dialog = popupData->dialog;

        if (auto edit = static_cast<game::CEditBoxInterf*>(
                dialogApi.findControl(dialog, "EDIT_INPUT"))) {
            if (edit->data) {
                auto& data = edit->data->editBoxData;
                if (data.maxInputLength < 512) {
                    data.maxInputLength = 512;
                    editApi.setInputLength(edit, 512);
                    spdlog::debug("[ChatInterf] EDIT_INPUT max length increased to {}",
                                  data.maxInputLength);
                }
            }
        }
    }

    // === Проверка данных ===
    auto& data = *thisptr->chatData;
    if (!data.chatMessages.bgn || index < 0
        || index >= static_cast<int>(data.chatMessages.size())) {
        if (orig.chatInterfListBoxDisplayHandler)
            orig.chatInterfListBoxDisplayHandler(thisptr, contents, lineArea, index, selected);
        return;
    }

    auto& msg = data.chatMessages.bgn[index];
    if (!msg.message)
        return;

    if (std::strcmp(msg.message, "GG") == 0 || std::strcmp(msg.message, "gg") == 0) {
        using namespace game;
        spdlog::info(
            "[ChatInterf] GG detected — closing current interface and returning to main menu");

        auto& midgardApi = CMidgardApi::get();
        if (auto* midgard = midgardApi.instance()) {
            // 1?? Безопасно закрываем текущий интерфейс (чат)
            if (thisptr) {
                spdlog::info("[ChatInterf] Hiding active chat interface...");
                hideInterface(thisptr);
            }

            // 2?? Чистим сетевое состояние (разрывает соединение, сбрасывает multiplayerGame/host)
            if (midgardApi.clearNetworkStateAndService) {
                spdlog::info("[ChatInterf] Clearing network state and service...");
                midgardApi.clearNetworkStateAndService(midgard);
            }

            // 3?? Возвращаемся в главное меню (если MenuPhase активна)
            if (midgard->data && midgard->data->menuPhase) {
                spdlog::info("[ChatInterf] Transitioning to main menu...");
                auto* menuPhase = midgard->data->menuPhase;
                CMenuPhaseApi::get().transitionToMainOrCloseGame(menuPhase, true);
            } else if (midgardApi.startMenuMessageCallback) {
                // Fallback: если меню ещё не инициализировано
                spdlog::warn("[ChatInterf] MenuPhase null — fallback to StartMenuMessageCallback");
                midgardApi.startMenuMessageCallback(midgard, 0, 0);
            }
        }

        return;
    }

    // ======================================================
    // ?? Обработка команды !showvariables — добавляем строки в чат
    // ======================================================
    if (std::strcmp(msg.message, "!showvariables") == 0) {
        spdlog::info("[ChatInterf] Executing !showvariables — appending messages");

        auto* phaseGame = thisptr->phaseGame;
        if (!phaseGame)
            return;

        const auto& phaseApi = game::CPhaseApi::get();
        auto* dataCache = phaseApi.getDataCache(&phaseGame->phase);
        if (!dataCache)
            return;

        const auto* scenVars = getScenarioVariables(dataCache);
        if (!scenVars)
            return;

        std::string clipboardText;
        clipboardText.reserve(256);

        auto& messages = data.chatMessages;
        const auto& memAlloc = game::Memory::get().allocate; // игровой аллокатор

        // --- Чтобы не вызывать повторно, очищаем команду прямо сейчас ---
        msg.message = "";

        // --- Добавляем системное сообщение-разделитель ---
        {
            game::TextMessage sysMsg{};
            const char* header = "\\fSmall;\\c255;255;0;---- Scenario Variables ----";
            size_t len = std::strlen(header) + 1;
            char* msgMem = static_cast<char*>(memAlloc(static_cast<int>(len)));
            std::memcpy(msgMem, header, len);
            sysMsg.message = msgMem;
            sysMsg.race = nullptr;
            //sysMsg.playerId = -1;
            game::vectorPushBack(messages, sysMsg);
        }



        bool first = true;

        // --- Добавляем все переменные ---
        for (auto it = scenVars->variables.begin(); it != scenVars->variables.end(); ++it) {
            const auto& pair = *it;
            const auto& var = pair.second;

            char buffer[128];
            std::snprintf(buffer, sizeof(buffer), "%s = %d", var.name, var.value);

            // === добавляем в буфер обмена ===
            if (!first)
                clipboardText += ", ";
            clipboardText += fmt::format("{}={}", var.name, var.value);
            first = false;

            size_t len = std::strlen(buffer) + 1;
            char* msgMem = static_cast<char*>(memAlloc(static_cast<int>(len)));
            std::memcpy(msgMem, buffer, len);

            game::TextMessage sysMsg{};
            sysMsg.message = msgMem;
            sysMsg.race = nullptr;
            //sysMsg.playerId = -1;
            game::vectorPushBack(messages, sysMsg);
        }

        if (!clipboardText.empty()) {
            copyToClipboard(clipboardText);
            spdlog::info("[ChatInterf] Scenario variables copied to clipboard: {}", clipboardText);
        }

        // --- Завершение блока ---
        {
            game::TextMessage sysMsg{};
            const char* footer = "\\fSmall;\\c255;255;0;-----------------------------";
            size_t len = std::strlen(footer) + 1;
            char* msgMem = static_cast<char*>(memAlloc(static_cast<int>(len)));
            std::memcpy(msgMem, footer, len);
            sysMsg.message = msgMem;
            sysMsg.race = nullptr;
            //sysMsg.playerId = -1;
            game::vectorPushBack(messages, sysMsg);
        }

        spdlog::info("[ChatInterf] Appended {} variable messages", scenVars->variables.length);

        // --- Удаляем саму команду из чата, чтобы не перезапускалось ---
        auto itMsg = data.chatMessages.bgn + index;
        if (itMsg < data.chatMessages.end) {
            for (auto* p = itMsg; p + 1 < data.chatMessages.end; ++p)
                *p = *(p + 1);
            data.chatMessages.end--;
        }

        return;
    }

    // === Копирование текста ===
    if (selected && isCtrlPressed() && *msg.message) {
        copyToClipboard(msg.message);
        spdlog::info("[ChatInterf] Copied to clipboard: {}", msg.message);
    }

    // === Кэш времени ===
    const TextMessage* msgPtr = &msg;
    const char* originalMessage = msg.message;

    if (timeCache.find(msgPtr) == timeCache.end())
        timeCache[msgPtr] = static_cast<int>(std::time(nullptr));

    std::time_t t = static_cast<std::time_t>(timeCache[msgPtr]);
    std::tm* tm_info = std::localtime(&t);
    char timeBuf[8]{};
    std::strftime(timeBuf, sizeof(timeBuf), "[%H:%M]", tm_info);

    // === Имя игрока ===
    std::string playerName = "System";
    for (const auto* it = data.playerListEntries.bgn; it && it < data.playerListEntries.end; ++it) {
        const auto& p = *it;
        if (msg.race && p.playerRace.id == msg.race->id && p.playerName) {
            playerName = p.playerName;
            break;
        }
    }

    // === Форматирование текста ===
    static thread_local std::string formatted;
    formatted = fmt::format(" {} \\fMedBold;{} \\fSmall;: {}", timeBuf, playerName,
                            msg.message ? msg.message : "");

    msg.message = formatted.c_str();

    // === Подсчёт высоты ===
    CMqRect expanded = *lineArea;
    const auto& fmtApi = IFormattedTextApi::get();
    game::FormattedTextPtr fmtTextPtr;
    fmtApi.getFormattedText(&fmtTextPtr);

    if (fmtTextPtr.data) {
        auto* fmt = fmtTextPtr.data;
        const int textWidth = expanded.right - expanded.left;
        const int textHeight = fmt->vftable->getTextHeight(fmt, msg.message, textWidth);
        expanded.bottom = expanded.top + std::max(64, textHeight + 4);
    }

    // === Рендер ===
    if (orig.chatInterfListBoxDisplayHandler)
        orig.chatInterfListBoxDisplayHandler(thisptr, contents, &expanded, index, selected);

    // === Восстановление ===
    msg.message = originalMessage;

    spdlog::debug("[ChatInterf] [{}] {}: {}", timeBuf, playerName, msg.message ? msg.message : "");
}

} // namespace hooks
