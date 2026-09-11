/**
 * @file SystemBus.cpp
 * @brief FreeRTOS queue and task integration for the System Event Bus.
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

#include "ggg/system/SystemBus.h"

// Check if FreeRTOS target is active and not building for native host simulation
#if defined(CONFIG_GGG_RTOS_FREERTOS) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)

#include <FreeRTOS.h>
#include <queue.h>

namespace ggg {
namespace system {

// Static FreeRTOS Queue memory structures
#if defined(configSUPPORT_STATIC_ALLOCATION) && (configSUPPORT_STATIC_ALLOCATION == 1)
static StaticQueue_t _staticQueue;
static uint8_t       _queueStorageArea[SystemBus::QUEUE_SIZE * sizeof(SystemEvent)];
#endif
static QueueHandle_t _eventQueue = nullptr;

SystemBus& SystemBus::getInstance() {
    static SystemBus instance;
    return instance;
}

SystemBus::SystemBus() : _listenerCount(0), _initialized(false) {
    for (size_t i = 0; i < MAX_LISTENERS; ++i) {
        _listeners[i] = nullptr;
    }
    init();
}

bool SystemBus::init() {
    if (_eventQueue == nullptr) {
#if defined(configSUPPORT_STATIC_ALLOCATION) && (configSUPPORT_STATIC_ALLOCATION == 1)
        _eventQueue = xQueueCreateStatic(
            QUEUE_SIZE,
            sizeof(SystemEvent),
            _queueStorageArea,
            &_staticQueue
        );
#else
        // Fallback for third-party FreeRTOS ports where configSUPPORT_STATIC_ALLOCATION == 0
        _eventQueue = xQueueCreate(QUEUE_SIZE, sizeof(SystemEvent));
#endif
        _initialized = (_eventQueue != nullptr);
    }
    return _initialized;
}

bool SystemBus::subscribe(IEventListener* listener) {
    if (listener == nullptr) {
        return false;
    }
    // Prevent duplicate subscriptions
    for (size_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] == listener) {
            return true;
        }
    }
    if (_listenerCount < MAX_LISTENERS) {
        _listeners[_listenerCount++] = listener;
        return true;
    }
    return false; // Listener pool exhausted
}

bool SystemBus::unsubscribe(IEventListener* listener) {
    if (listener == nullptr) {
        return false;
    }
    for (size_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] == listener) {
            // Shift remaining listeners
            for (size_t j = i; j < _listenerCount - 1; ++j) {
                _listeners[j] = _listeners[j + 1];
            }
            _listeners[--_listenerCount] = nullptr;
            return true;
        }
    }
    return false;
}

size_t SystemBus::getListenerCount() const {
    return _listenerCount;
}

bool SystemBus::publish(const SystemEvent& event) {
    if (_eventQueue == nullptr) {
        if (!init()) return false;
    }
    // Non-blocking send: timeout 0 (drop and return false if queue is full)
    return (xQueueSend(_eventQueue, &event, 0) == pdPASS);
}

bool SystemBus::publishFromISR(const SystemEvent& event) {
    if (_eventQueue == nullptr) {
        return false;
    }
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t status = xQueueSendFromISR(_eventQueue, &event, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return (status == pdPASS);
}

void SystemBus::processEventsTask() {
    if (_eventQueue == nullptr) {
        init();
    }
    SystemEvent event;
    while (true) {
        if (xQueueReceive(_eventQueue, &event, portMAX_DELAY) == pdPASS) {
            for (size_t i = 0; i < _listenerCount; ++i) {
                if (_listeners[i] != nullptr) {
                    _listeners[i]->onEvent(event);
                }
            }
        }
    }
}

bool SystemBus::dispatchOne(uint32_t waitTicks) {
    if (_eventQueue == nullptr) {
        if (!init()) return false;
    }
    SystemEvent event;
    TickType_t ticks = (waitTicks == 0) ? 0 : pdMS_TO_TICKS(waitTicks);
    if (xQueueReceive(_eventQueue, &event, ticks) == pdPASS) {
        for (size_t i = 0; i < _listenerCount; ++i) {
            if (_listeners[i] != nullptr) {
                _listeners[i]->onEvent(event);
            }
        }
        return true;
    }
    return false;
}

size_t SystemBus::getPendingCount() const {
    if (_eventQueue == nullptr) return 0;
    return static_cast<size_t>(uxQueueMessagesWaiting(_eventQueue));
}

void SystemBus::reset() {
    if (_eventQueue != nullptr) {
        xQueueReset(_eventQueue);
    }
    for (size_t i = 0; i < MAX_LISTENERS; ++i) {
        _listeners[i] = nullptr;
    }
    _listenerCount = 0;
}

} // namespace system
} // namespace ggg

#else // Native Host Target / Simulation (Zero-Malloc Circular Buffer)

namespace ggg {
namespace system {

static SystemEvent _nativeQueueStorage[SystemBus::QUEUE_SIZE];
static size_t      _nativeQueueHead  = 0;
static size_t      _nativeQueueTail  = 0;
static size_t      _nativeQueueCount = 0;

SystemBus& SystemBus::getInstance() {
    static SystemBus instance;
    return instance;
}

SystemBus::SystemBus() : _listenerCount(0), _initialized(false) {
    for (size_t i = 0; i < MAX_LISTENERS; ++i) {
        _listeners[i] = nullptr;
    }
    init();
}

bool SystemBus::init() {
    _nativeQueueHead = 0;
    _nativeQueueTail = 0;
    _nativeQueueCount = 0;
    _initialized = true;
    return true;
}

bool SystemBus::subscribe(IEventListener* listener) {
    if (listener == nullptr) {
        return false;
    }
    for (size_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] == listener) {
            return true;
        }
    }
    if (_listenerCount < MAX_LISTENERS) {
        _listeners[_listenerCount++] = listener;
        return true;
    }
    return false;
}

bool SystemBus::unsubscribe(IEventListener* listener) {
    if (listener == nullptr) {
        return false;
    }
    for (size_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] == listener) {
            for (size_t j = i; j < _listenerCount - 1; ++j) {
                _listeners[j] = _listeners[j + 1];
            }
            _listeners[--_listenerCount] = nullptr;
            return true;
        }
    }
    return false;
}

size_t SystemBus::getListenerCount() const {
    return _listenerCount;
}

bool SystemBus::publish(const SystemEvent& event) {
    if (!_initialized) init();
    if (_nativeQueueCount >= QUEUE_SIZE) {
        return false; // Queue full
    }
    _nativeQueueStorage[_nativeQueueTail] = event;
    _nativeQueueTail = (_nativeQueueTail + 1) % QUEUE_SIZE;
    _nativeQueueCount++;
    return true;
}

bool SystemBus::publishFromISR(const SystemEvent& event) {
    return publish(event);
}

void SystemBus::processEventsTask() {
    while (true) {
        if (!dispatchOne(0)) {
            break;
        }
    }
}

bool SystemBus::dispatchOne(uint32_t waitTicks) {
    (void)waitTicks;
    if (!_initialized) init();
    if (_nativeQueueCount == 0) {
        return false;
    }
    SystemEvent event = _nativeQueueStorage[_nativeQueueHead];
    _nativeQueueHead = (_nativeQueueHead + 1) % QUEUE_SIZE;
    _nativeQueueCount--;

    for (size_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] != nullptr) {
            _listeners[i]->onEvent(event);
        }
    }
    return true;
}

size_t SystemBus::getPendingCount() const {
    return _nativeQueueCount;
}

void SystemBus::reset() {
    _nativeQueueHead = 0;
    _nativeQueueTail = 0;
    _nativeQueueCount = 0;
    for (size_t i = 0; i < MAX_LISTENERS; ++i) {
        _listeners[i] = nullptr;
    }
    _listenerCount = 0;
}

} // namespace system
} // namespace ggg

#endif
