/**
 * @file SystemBus.h
 * @brief Zero-Malloc FreeRTOS-backed System Event Bus implementation declaration.
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

#ifndef GGG_SYSTEM_SYSTEM_BUS_H
#define GGG_SYSTEM_SYSTEM_BUS_H

#include "ggg/system/SystemEvent.h"
#include "ggg/system/IEventListener.h"
#include <stdint.h>
#include <stddef.h>

#if __has_include("autoconf.h")
#include "autoconf.h"
#endif

#ifndef CONFIG_GGG_SYSTEM_BUS_QUEUE_SIZE
#define CONFIG_GGG_SYSTEM_BUS_QUEUE_SIZE 16
#endif

#ifndef CONFIG_GGG_SYSTEM_BUS_MAX_LISTENERS
#define CONFIG_GGG_SYSTEM_BUS_MAX_LISTENERS 10
#endif

namespace ggg {
namespace system {

/**
 * @brief High-performance, Zero-Malloc Event Bus for GGG Framework.
 * Uses a static FreeRTOS queue (xQueueCreateStatic) on embedded platforms,
 * and a static circular buffer on native/host targets for simulation and unit tests.
 */
class SystemBus {
public:
    static constexpr size_t QUEUE_SIZE    = CONFIG_GGG_SYSTEM_BUS_QUEUE_SIZE;
    static constexpr size_t MAX_LISTENERS = CONFIG_GGG_SYSTEM_BUS_MAX_LISTENERS;

    /**
     * @brief Access the global singleton instance.
     */
    static SystemBus& getInstance();

    // Non-copyable, non-assignable
    SystemBus(const SystemBus&) = delete;
    SystemBus& operator=(const SystemBus&) = delete;

    /**
     * @brief Deterministically initializes the static event queue and listener array.
     * Guaranteed to be Zero-Malloc.
     * @return true on success.
     */
    bool init();

    /**
     * @brief Subscribes an event listener to receive all dispatched SystemEvents.
     * @param listener Pointer to object implementing IEventListener.
     * @return true if registered, false if MAX_LISTENERS capacity reached or listener is null.
     */
    bool subscribe(IEventListener* listener);

    /**
     * @brief Unsubscribes a previously registered listener.
     * @param listener Pointer to object implementing IEventListener.
     * @return true if found and removed, false otherwise.
     */
    bool unsubscribe(IEventListener* listener);

    /**
     * @brief Returns current count of registered listeners.
     */
    size_t getListenerCount() const;

    /**
     * @brief Publishes an event to the queue from a regular Task context.
     * Non-blocking: returns false immediately if the queue is full.
     * @param event The SystemEvent to publish (copied into the queue).
     * @return true if enqueued, false if queue is full or uninitialized.
     */
    bool publish(const SystemEvent& event);

    /**
     * @brief Publishes an event to the queue from an Interrupt Service Routine (ISR).
     * Safely triggers portYIELD_FROM_ISR if a higher-priority task is unblocked.
     * @param event The SystemEvent to publish.
     * @return true if enqueued, false if queue is full or uninitialized.
     */
    bool publishFromISR(const SystemEvent& event);

    /**
     * @brief Endless blocking task loop executed by the RTOS SystemBusTask.
     * Blocks on the FreeRTOS queue and dispatches dequeued events O(1) to all listeners.
     */
    void processEventsTask();

    /**
     * @brief Dequeues and dispatches a single event from the queue.
     * Non-blocking (or with specified wait ticks).
     * Useful for cooperative scheduling, unit tests, and host simulation.
     * @param waitTicks Maximum ticks to wait (0 = non-blocking).
     * @return true if an event was processed, false if queue was empty.
     */
    bool dispatchOne(uint32_t waitTicks = 0);

    /**
     * @brief Returns the number of events currently queued.
     */
    size_t getPendingCount() const;

    /**
     * @brief Clears pending events in the queue and unregisters all listeners.
     * Primarily used for isolation between unit tests.
     */
    void reset();

private:
    SystemBus();
    ~SystemBus() = default;

    IEventListener* _listeners[MAX_LISTENERS];
    size_t          _listenerCount;
    bool            _initialized;
};

} // namespace system
} // namespace ggg

#endif // GGG_SYSTEM_SYSTEM_BUS_H
