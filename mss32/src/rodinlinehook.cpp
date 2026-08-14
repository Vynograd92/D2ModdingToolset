#include "rodinlinehook.h"

#include <Windows.h>
#include <cstdint>
#include <spdlog/spdlog.h>

namespace {
constexpr uintptr_t PlantRodPatch = 0x00437BE9;

constexpr uintptr_t ReturnAddress = 0x00437BF0;

constexpr uintptr_t BreakRodPatch = 0x005DB2B3;
constexpr uintptr_t BreakRodReturn = 0x005DB2BA;

constexpr uintptr_t MovePatch = 0x0043A4EA;
constexpr uintptr_t MoveReturn = 0x0043A4F1;

constexpr uintptr_t MoveVisitorPatch = 0x005E8B0E;
constexpr uintptr_t MoveVisitorReturn = 0x005E8B15;

constexpr uintptr_t BattlePenaltyAddress = 0x006EFEC8;

static int calculateRodMovementCost(uint8_t maxMovement)
{
    //
    // Половина максимума с округлением вверх
    //

    //return (maxMovement + 1) / 2;
    return 5;
}

static void patchBattlePenalty(double value)
{
    DWORD oldProtect;

    auto ptr = reinterpret_cast<double*>(BattlePenaltyAddress);

    VirtualProtect(ptr, sizeof(double), PAGE_EXECUTE_READWRITE, &oldProtect);

    *ptr = value;

    VirtualProtect(ptr, sizeof(double), oldProtect, &oldProtect);

    FlushInstructionCache(GetCurrentProcess(), ptr, sizeof(double));

    spdlog::info("Battle movement penalty set to {}", value);
}

static int logMove(int movement, void* ret)
{
    spdlog::info("MoveAllowance cost={} ret={:08X}", movement, reinterpret_cast<uintptr_t>(ret));

    return movement;
}

static int calculateMovementCost(int movement)
{
    spdlog::info("Movement cost {}", movement);

    return 0;
}


void writeJump(void* from, void* to)
{
    DWORD oldProtect;

    VirtualProtect(from, 7, PAGE_EXECUTE_READWRITE, &oldProtect);

    auto p = static_cast<unsigned char*>(from);

    p[0] = 0xE9;

    *reinterpret_cast<int32_t*>(p + 1) = (int32_t)((char*)to - ((char*)from + 5));

    // добиваем остаток инструкции
    p[5] = 0x90;
    p[6] = 0x90;

    VirtualProtect(from, 7, oldProtect, &oldProtect);

    FlushInstructionCache(GetCurrentProcess(), from, 7);
}

extern "C" __declspec(naked) void PlantRodMovementHook()
{
    __asm
        {
        pushad
        pushfd
        }

    spdlog::info("PlantRod inline reached");

    __asm
    {
        popfd
        popad

         //
         // Повторяем украденную инструкцию
         //

       movzx ecx, byte ptr [ebx+98h]
       push ecx
       call calculateRodMovementCost
       add esp,4

        jmp ReturnAddress
    }
}

extern "C" __declspec(naked) void BreakRodMovementHook()
{
    __asm
    {
        movzx ecx, byte ptr [edi+98h]

        push ecx
        call calculateRodMovementCost
        add esp, 4

        jmp BreakRodReturn
    }
}

extern "C" __declspec(naked) void MovementHook()
{
    __asm
    {
        mov eax, [esi+1Ch]

        push eax
        call calculateMovementCost
        add esp, 4

        test eax, eax

        jmp MoveReturn
    }
}




} // namespace

void hooks::installRodInlineHook()
{
    patchBattlePenalty(0.1);

    writeJump(reinterpret_cast<void*>(PlantRodPatch),
              reinterpret_cast<void*>(PlantRodMovementHook));

    writeJump(reinterpret_cast<void*>(BreakRodPatch),
              reinterpret_cast<void*>(BreakRodMovementHook));

    writeJump(reinterpret_cast<void*>(MovePatch), reinterpret_cast<void*>(MovementHook));
}