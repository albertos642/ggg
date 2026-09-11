/**
 * @file main.cpp
 * @brief Framework entry point and FreeRTOS startup coordinator.
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

#if defined(GGG_BUILD_STANDALONE) && defined(ARDUINO)

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include "ggg.h"
#include "autoconf.h"

// SystemBus dispatcher task stub for standalone verification
static void StandaloneSystemBusTask(void *pvParameters) {
    (void)pvParameters;
    ggg::system::SystemBus::getInstance().processEventsTask();
}

void setup() {
    ggg::system::SystemBus::getInstance().init();

    xTaskCreate(
        StandaloneSystemBusTask,
        "SysBus",
        CONFIG_GGG_SYSTEM_BUS_TASK_STACK_SIZE,
        nullptr,
        CONFIG_GGG_SYSTEM_BUS_TASK_PRIORITY,
        nullptr
    );

    ggg::system::SystemEvent startupEvt = {};
    startupEvt.type = ggg::system::GGG_EVT_STARTUP;
    startupEvt.source = 0;
    startupEvt.priority = 255;
    ggg::system::SystemBus::getInstance().publish(startupEvt);
}

void loop() {
    // Empty
}

#endif // GGG_BUILD_STANDALONE && ARDUINO
