/**
 * @file ButtonPlugin.h
 * @brief Multi-instance, interrupt-driven button plugin with integral debounce filtering.
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

#ifndef GGG_BUTTON_PLUGIN_H
#define GGG_BUTTON_PLUGIN_H

#include <stdint.h>
#include <stddef.h>
#include "ggg/core/IPlugin.h"
#include "ggg/system/SystemBus.h"

#define GGG_BUTTON_MAX_INSTANCES 8

namespace ggg {
namespace plugins {

/**
 * @brief Hardware pull resistor configuration.
 */
enum class ButtonPullMode : uint8_t {
    PULL_UP = 0,
    PULL_DOWN = 1,
    PULL_NONE = 2
};

/**
 * @brief Logical or physical transition triggering SystemBus publication.
 */
enum class ButtonActiveEdge : uint8_t {
    EDGE_FALLING = 0,
    EDGE_RISING = 1,
    EDGE_BOTH = 2
};

/**
 * @brief Complete configuration parameters for a ButtonPlugin instance.
 */
struct ButtonConfig {
    uint32_t pin;
    ButtonPullMode pullMode;
    bool activeLow;
    ButtonActiveEdge activeEdge;
    uint16_t sampleIntervalMs;
    uint16_t debounceThreshold;
    system::EventId_t eventId;
    uint32_t eventCode;
    uint8_t moduleId;
};

/**
 * @brief Interrupt-driven pushbutton plugin with integral debounce.
 */
class ButtonPlugin : public core::IPlugin {
private:
    ButtonConfig _config;
    volatile bool _samplingActive;
    volatile bool _lastRawLevel;
    bool _debouncedState;       // true = pressed, false = released
    int16_t _integrator;        // 0 to debounceThreshold
    uint8_t _instanceIndex;
    bool _isInitialized;

    static ButtonPlugin* s_instances[GGG_BUTTON_MAX_INSTANCES];
    static uint8_t s_instanceCount;

    static void isr0();
    static void isr1();
    static void isr2();
    static void isr3();
    static void isr4();
    static void isr5();
    static void isr6();
    static void isr7();

    typedef void (*IsrTrampoline_t)();
    static const IsrTrampoline_t s_trampolines[GGG_BUTTON_MAX_INSTANCES];

    void handleInterrupt();

public:
    /**
     * @brief Constructs a ButtonPlugin using default settings injected from Kconfig.
     */
    ButtonPlugin();

    /**
     * @brief Constructs a ButtonPlugin with custom configuration (for multi-instance registration).
     */
    explicit ButtonPlugin(const ButtonConfig& config);

    ~ButtonPlugin() override;

    bool begin() override;
    void onEvent(const system::SystemEvent& event) override;
    void tick() override;

    /**
     * @brief Returns current debounced logical state (true = pressed, false = released).
     */
    bool isPressed() const { return _debouncedState; }

    /**
     * @brief Directly queries or simulates physical pin read (useful for testing and HAL abstraction).
     */
    bool readPin() const;

    /**
     * @brief Test and simulation hook to simulate pin level changes and trigger ISR.
     */
    void simulatePinTransition(bool pinLevelHigh);

    /**
     * @brief Returns the active configuration for this button.
     */
    const ButtonConfig& getConfig() const { return _config; }
};

} // namespace plugins
} // namespace ggg

#endif // GGG_BUTTON_PLUGIN_H
