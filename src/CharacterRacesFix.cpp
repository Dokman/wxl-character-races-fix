// wxl-character-races-fix: port of the WotLKExtensions character-creation race crash fix.
// Repoints the client's fixed-size race tables to module-owned storage, so character creation keeps
// working when the client exposes more than 21 playable races.
// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "core/Logger.hpp"
#include "core/Mem.hpp"
#include "events/Event.hpp"
#include "events/EventScript.hpp"

#include <array>
#include <cstdint>
#include <cstring>

namespace wxl::scripts::characterracesfix
{
    namespace ev = wxl::events;

    namespace
    {
        static_assert(sizeof(void*) == 4, "wxl-character-races-fix targets the 32-bit 3.3.5a client.");

        constexpr uint32_t kOriginalRaceNameTableVa  = 0xB24180;
        constexpr uint32_t kRaceNameTablePatchVa     = 0x4CDA43;
        constexpr uint32_t kRaceLoopBoundPatchVa     = 0x4E0F86;
        constexpr uint8_t  kRaceLoopBoundValue       = 0x40;
        constexpr size_t   kRaceNameCopyBytes        = 0x30; // 12 native pointers from Wow.exe.
        constexpr size_t   kRaceNameCapacity         = 32;
        constexpr size_t   kMemoryTableCapacity      = 64;
        constexpr size_t   kFirstExtendedRaceIndex   = 22;

        constexpr std::array<uint32_t, 8> kMemoryTablePatchVas = {
            0x4E157D, 0x4E16A3, 0x4E15B5, 0x4E20EE,
            0x4E222A, 0x4E2127, 0x4E1E94, 0x4E1C3A,
        };

        void* Va(uint32_t va)
        {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(va));
        }

        bool WriteU32(uint32_t va, const void* ptr)
        {
            const uint32_t value = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ptr));
            return wxl::core::mem::Patch(Va(va), &value, sizeof value);
        }

        bool WriteU8(uint32_t va, uint8_t value)
        {
            return wxl::core::mem::Patch(Va(va), &value, sizeof value);
        }
    }

    class CharacterRacesFix final : public ev::EventScript
    {
    public:
        CharacterRacesFix()
        {
            on<&CharacterRacesFix::OnEndScene>(ev::Event::OnEndScene);
        }

        void OnEndScene(const ev::EndSceneArgs&)
        {
            if (applied_ || attempted_) return;
            attempted_ = true;
            applied_ = Apply();
        }

    private:
        bool Apply()
        {
            raceNameTable_.fill(0);
            memoryTable_.fill(0);

            std::memcpy(raceNameTable_.data(), Va(kOriginalRaceNameTableVa), kRaceNameCopyBytes);
            for (size_t i = kFirstExtendedRaceIndex; i < raceNameTable_.size(); ++i)
                raceNameTable_[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&dummy_));

            for (uint32_t va : kMemoryTablePatchVas)
                if (!WriteU32(va, memoryTable_.data()))
                {
                    WLOG_ERROR("wxl-character-races-fix: failed to patch memory-table VA 0x%08X", va);
                    return false;
                }

            if (!WriteU32(kRaceNameTablePatchVa, raceNameTable_.data()))
            {
                WLOG_ERROR("wxl-character-races-fix: failed to patch race-name-table VA 0x%08X",
                           kRaceNameTablePatchVa);
                return false;
            }

            if (!WriteU8(kRaceLoopBoundPatchVa, kRaceLoopBoundValue))
            {
                WLOG_ERROR("wxl-character-races-fix: failed to patch loop-bound VA 0x%08X",
                           kRaceLoopBoundPatchVa);
                return false;
            }

            WLOG_INFO("wxl-character-races-fix: applied (raceTable=%p memoryTable=%p dummy=%p)",
                      static_cast<void*>(raceNameTable_.data()),
                      static_cast<void*>(memoryTable_.data()),
                      static_cast<void*>(&dummy_));
            return true;
        }

        bool applied_ = false;
        bool attempted_ = false;
        uint32_t dummy_ = 0;
        std::array<uint32_t, kRaceNameCapacity> raceNameTable_{};
        std::array<uint32_t, kMemoryTableCapacity> memoryTable_{};
    };

    CharacterRacesFix g_characterRacesFix;
}
