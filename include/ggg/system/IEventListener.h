/**
 * @file IEventListener.h
 * @brief Observer interface for subscribing to and handling SystemBus events.
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

#ifndef GGG_SYSTEM_I_EVENT_LISTENER_H
#define GGG_SYSTEM_I_EVENT_LISTENER_H

#include "ggg/system/SystemEvent.h"

namespace ggg {
namespace system {

/**
 * @brief Observer interface for listening to SystemBus events.
 */
class IEventListener {
public:
    virtual ~IEventListener() = default;

    /**
     * @brief Invoked by SystemBus dispatcher when an event is dequeued.
     * @param event The dispatched event.
     */
    virtual void onEvent(const SystemEvent& event) = 0;
};

} // namespace system
} // namespace ggg

#endif // GGG_SYSTEM_I_EVENT_LISTENER_H
