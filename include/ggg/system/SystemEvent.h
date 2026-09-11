/**
 * @file SystemEvent.h
 * @brief Type-safe fixed-size System Event data structure with 8-byte payload.
 *
 * @author Alberto Soncini <alberto@synergon-lab.xyz>
 * @copyright Copyright (c) 2012-2026 Alberto Soncini.
 * @license GPL-3.0-or-later
 *
 * This file is part of GGG.
 *
 * GGG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GGG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GGG. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GGG_SYSTEM_EVENT_H
#define GGG_SYSTEM_EVENT_H

#include <stdint.h>
#include <stddef.h>

namespace ggg {
namespace system {

typedef uint16_t EventId_t;
typedef uint8_t  ModuleId_t;

// Standard Native GGG Kernel Events (0x0000 - 0x0FFF reserved for GGG kernel)
constexpr EventId_t GGG_EVT_STARTUP        = 0x0001; // Emitted after system initialization
constexpr EventId_t GGG_EVT_SYS_TICK       = 0x0002; // Periodic tick event (payload.u32[0] = uptime sec)
constexpr EventId_t GGG_EVT_HARDWARE_FAULT = 0x000A; // Critical hardware fault (payload.u32[0] = error code)
constexpr EventId_t GGG_EVT_APP_TRIGGER    = 0x0100; // Generic input trigger (payload.u32[0] = trigger ID)

struct SystemEvent {
    EventId_t  type;       // Unique event identifier
    ModuleId_t source;     // Source module ID (e.g. 0x01 for Plugin A)
    uint8_t    priority;   // 0 = Low, 255 = Critical

    // Polymorphic Zero-Malloc payload. Maximum 8 bytes.
    union {
        uint32_t  u32[2];
        int32_t   i32[2];
        float     f32[2];
        uint8_t   bytes[8];
        void*     ptr;     // Pointer to STATIC or PRE-ALLOCATED storage memory. NEVER HEAP!
    } payload;
};

} // namespace system
} // namespace ggg

#endif // GGG_SYSTEM_EVENT_H
