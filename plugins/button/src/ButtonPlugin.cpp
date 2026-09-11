/**
 * @file ButtonPlugin.cpp
 * @brief Implementation of ButtonPlugin with interrupt edge detection and integral debounce.
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

#include "ggg/plugins/ButtonPlugin.h"

#if defined(ARDUINO) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)
#include <Arduino.h>
#else
// Simulated pin registry for host desktop testing
static bool s_nativeSimulatedPins[64] = { false };
#endif

namespace ggg {
namespace plugins {

ButtonPlugin* ButtonPlugin::s_instances[GGG_BUTTON_MAX_INSTANCES] = { nullptr };
uint8_t ButtonPlugin::s_instanceCount = 0;

void ButtonPlugin::isr0() { if (s_instances[0]) s_instances[0]->handleInterrupt(); }
void ButtonPlugin::isr1() { if (s_instances[1]) s_instances[1]->handleInterrupt(); }
void ButtonPlugin::isr2() { if (s_instances[2]) s_instances[2]->handleInterrupt(); }
void ButtonPlugin::isr3() { if (s_instances[3]) s_instances[3]->handleInterrupt(); }
void ButtonPlugin::isr4() { if (s_instances[4]) s_instances[4]->handleInterrupt(); }
void ButtonPlugin::isr5() { if (s_instances[5]) s_instances[5]->handleInterrupt(); }
void ButtonPlugin::isr6() { if (s_instances[6]) s_instances[6]->handleInterrupt(); }
void ButtonPlugin::isr7() { if (s_instances[7]) s_instances[7]->handleInterrupt(); }

const ButtonPlugin::IsrTrampoline_t ButtonPlugin::s_trampolines[GGG_BUTTON_MAX_INSTANCES] = {
    ButtonPlugin::isr0, ButtonPlugin::isr1, ButtonPlugin::isr2, ButtonPlugin::isr3,
    ButtonPlugin::isr4, ButtonPlugin::isr5, ButtonPlugin::isr6, ButtonPlugin::isr7
};

ButtonPlugin::ButtonPlugin()
    : _samplingActive(false),
      _lastRawLevel(false),
      _debouncedState(false),
      _integrator(0),
      _instanceIndex(0xFF),
      _isInitialized(false)
{
    // Load default settings from autoconf.h
#if defined(CONFIG_GGG_BUTTON_PIN)
    _config.pin = CONFIG_GGG_BUTTON_PIN;
#else
    _config.pin = 5;
#endif

#if defined(CONFIG_GGG_BUTTON_ACTIVE_LOW_PULLUP)
    _config.pullMode = ButtonPullMode::PULL_UP;
    _config.activeLow = true;
#elif defined(CONFIG_GGG_BUTTON_ACTIVE_LOW_EXTERNAL)
    _config.pullMode = ButtonPullMode::PULL_NONE;
    _config.activeLow = true;
#elif defined(CONFIG_GGG_BUTTON_ACTIVE_HIGH_PULLDOWN)
    _config.pullMode = ButtonPullMode::PULL_DOWN;
    _config.activeLow = false;
#elif defined(CONFIG_GGG_BUTTON_ACTIVE_HIGH_EXTERNAL)
    _config.pullMode = ButtonPullMode::PULL_NONE;
    _config.activeLow = false;
#else
    _config.pullMode = ButtonPullMode::PULL_UP;
    _config.activeLow = true;
#endif

#if defined(CONFIG_GGG_BUTTON_EDGE_RISING)
    _config.activeEdge = ButtonActiveEdge::EDGE_RISING;
#elif defined(CONFIG_GGG_BUTTON_EDGE_BOTH)
    _config.activeEdge = ButtonActiveEdge::EDGE_BOTH;
#else
    _config.activeEdge = ButtonActiveEdge::EDGE_FALLING;
#endif

#if defined(CONFIG_GGG_BUTTON_SAMPLE_INTERVAL_MS)
    _config.sampleIntervalMs = CONFIG_GGG_BUTTON_SAMPLE_INTERVAL_MS;
#else
    _config.sampleIntervalMs = 5;
#endif

#if defined(CONFIG_GGG_BUTTON_DEBOUNCE_THRESHOLD)
    _config.debounceThreshold = CONFIG_GGG_BUTTON_DEBOUNCE_THRESHOLD;
#else
    _config.debounceThreshold = 6;
#endif

#if defined(CONFIG_GGG_BUTTON_EVENT_ID)
    _config.eventId = CONFIG_GGG_BUTTON_EVENT_ID;
#else
    _config.eventId = 0x0100;
#endif

#if defined(CONFIG_GGG_BUTTON_EVENT_CODE)
    _config.eventCode = CONFIG_GGG_BUTTON_EVENT_CODE;
#else
    _config.eventCode = 1;
#endif

    _config.moduleId = 0x10; // Module ID for Button
}

ButtonPlugin::ButtonPlugin(const ButtonConfig& config)
    : _config(config),
      _samplingActive(false),
      _lastRawLevel(false),
      _debouncedState(false),
      _integrator(0),
      _instanceIndex(0xFF),
      _isInitialized(false)
{
}

ButtonPlugin::~ButtonPlugin() {
    if (_instanceIndex < GGG_BUTTON_MAX_INSTANCES && s_instances[_instanceIndex] == this) {
#if defined(ARDUINO) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)
        detachInterrupt(digitalPinToInterrupt(_config.pin));
#endif
        s_instances[_instanceIndex] = nullptr;
    }
}

bool ButtonPlugin::begin() {
    // Register in static trampoline instance table
    if (_instanceIndex >= GGG_BUTTON_MAX_INSTANCES) {
        for (uint8_t i = 0; i < GGG_BUTTON_MAX_INSTANCES; ++i) {
            if (s_instances[i] == nullptr) {
                _instanceIndex = i;
                s_instances[i] = this;
                break;
            }
        }
        if (_instanceIndex >= GGG_BUTTON_MAX_INSTANCES) {
            return false; // Out of instance slots
        }
    }

#if defined(ARDUINO) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)
    if (_config.pullMode == ButtonPullMode::PULL_UP) {
        pinMode(_config.pin, INPUT_PULLUP);
    } else if (_config.pullMode == ButtonPullMode::PULL_DOWN) {
#if defined(INPUT_PULLDOWN)
        pinMode(_config.pin, INPUT_PULLDOWN);
#else
        pinMode(_config.pin, INPUT);
#endif
    } else {
        pinMode(_config.pin, INPUT);
    }

    int irq = digitalPinToInterrupt(_config.pin);
    if (irq != NOT_AN_INTERRUPT) {
        attachInterrupt(irq, s_trampolines[_instanceIndex], CHANGE);
    }
#else
    // Initialise simulated pin based on pull mode
    if (_config.pullMode == ButtonPullMode::PULL_UP) {
        s_nativeSimulatedPins[_config.pin % 64] = true;
    } else {
        s_nativeSimulatedPins[_config.pin % 64] = false;
    }
#endif

    bool initialHigh = readPin();
    _lastRawLevel = initialHigh;
    bool initialPressed = _config.activeLow ? !initialHigh : initialHigh;
    _debouncedState = initialPressed;
    _integrator = initialPressed ? _config.debounceThreshold : 0;
    _samplingActive = false;
    _isInitialized = true;

    return true;
}

void ButtonPlugin::handleInterrupt() {
    _samplingActive = true;
}

bool ButtonPlugin::readPin() const {
#if defined(ARDUINO) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)
    return digitalRead(_config.pin) == HIGH;
#else
    return s_nativeSimulatedPins[_config.pin % 64];
#endif
}

void ButtonPlugin::simulatePinTransition(bool pinLevelHigh) {
#if !defined(ARDUINO) || defined(GGG_TARGET_NATIVE) || defined(TARGET_NATIVE)
    s_nativeSimulatedPins[_config.pin % 64] = pinLevelHigh;
#else
    (void)pinLevelHigh;
#endif
    handleInterrupt();
}

void ButtonPlugin::onEvent(const system::SystemEvent& event) {
    (void)event;
}

void ButtonPlugin::tick() {
    if (!_isInitialized) {
        return;
    }

    bool rawHigh = readPin();
    bool rawPressed = _config.activeLow ? !rawHigh : rawHigh;

    // Integrator filter step
    if (rawPressed) {
        if (_integrator < (int16_t)_config.debounceThreshold) {
            _integrator++;
        }
    } else {
        if (_integrator > 0) {
            _integrator--;
        }
    }

    // State validation
    if (_integrator >= (int16_t)_config.debounceThreshold) {
        if (!_debouncedState) {
            _debouncedState = true; // Transition to Pressed

            bool trigger = false;
            if (_config.activeEdge == ButtonActiveEdge::EDGE_BOTH) {
                trigger = true;
            } else if (_config.activeLow && _config.activeEdge == ButtonActiveEdge::EDGE_FALLING) {
                trigger = true; // Physical falling edge = pressed
            } else if (!_config.activeLow && _config.activeEdge == ButtonActiveEdge::EDGE_RISING) {
                trigger = true; // Physical rising edge = pressed
            }

            if (trigger) {
                system::SystemEvent evt{};
                evt.type = _config.eventId;
                evt.source = _config.moduleId;
                evt.priority = 10;
                evt.payload.u32[0] = _config.eventCode;
                evt.payload.u32[1] = 1; // Pressed state
                system::SystemBus::getInstance().publish(evt);
            }
        }
        if (rawPressed) {
            _samplingActive = false; // Stable
        }
    } else if (_integrator == 0) {
        if (_debouncedState) {
            _debouncedState = false; // Transition to Released

            bool trigger = false;
            if (_config.activeEdge == ButtonActiveEdge::EDGE_BOTH) {
                trigger = true;
            } else if (_config.activeLow && _config.activeEdge == ButtonActiveEdge::EDGE_RISING) {
                trigger = true; // Physical rising edge = released
            } else if (!_config.activeLow && _config.activeEdge == ButtonActiveEdge::EDGE_FALLING) {
                trigger = true; // Physical falling edge = released
            }

            if (trigger) {
                system::SystemEvent evt{};
                evt.type = _config.eventId;
                evt.source = _config.moduleId;
                evt.priority = 10;
                evt.payload.u32[0] = _config.eventCode;
                evt.payload.u32[1] = 0; // Released state
                system::SystemBus::getInstance().publish(evt);
            }
        }
        if (!rawPressed) {
            _samplingActive = false; // Stable
        }
    }
}

} // namespace plugins
} // namespace ggg
