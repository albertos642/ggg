/**
 * @file IPlugin.h
 * @brief Abstract interface for modular framework plugins and lifecycle management.
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

#ifndef GGG_IPLUGIN_H
#define GGG_IPLUGIN_H

#include "ggg/system/SystemEvent.h"
#include "ggg/system/IEventListener.h"

namespace ggg {
namespace core {

class IPlugin : public system::IEventListener {
public:
    virtual ~IPlugin() override = default;

    // Called once during boot before the RTOS scheduler starts.
    // Returns false if hardware or module initialization fails.
    virtual bool begin() = 0;

    // Called by SystemBusTask whenever an event is dispatched from the queue.
    virtual void onEvent(const system::SystemEvent& event) override = 0;

    // Called by TickTask for low-priority periodic polling.
    // MUST be strictly non-blocking (immediate return).
    virtual void tick() = 0;
};

} // namespace core
} // namespace ggg

#endif // GGG_IPLUGIN_H
