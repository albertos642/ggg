/**
 * @file test_main.cpp
 * @brief Unit tests for GGG Button and SPI Flash Storage plugins.
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

#include <unity.h>
#include <string.h>

#include "ggg/system/SystemBus.h"
#include "ggg/plugins/ButtonPlugin.h"
#include "ggg/plugins/MockSpiFlashHal.h"
#include "ggg/plugins/SpiFlashStorage.h"

using namespace ggg;
using namespace ggg::plugins;
using namespace ggg::system;

// Test event listener to record SystemBus events
class TestEventListener : public IEventListener {
public:
    uint32_t lastEventId;
    uint32_t lastEventCode;
    uint32_t lastState;
    uint32_t eventCount;

    TestEventListener() : lastEventId(0), lastEventCode(0), lastState(0), eventCount(0) {}

    void onEvent(const SystemEvent& event) override {
        lastEventId = event.type;
        lastEventCode = event.payload.u32[0];
        lastState = event.payload.u32[1];
        eventCount++;
    }

    void reset() {
        lastEventId = 0;
        lastEventCode = 0;
        lastState = 0;
        eventCount = 0;
    }
};

static TestEventListener s_testListener;

void setUp(void) {
    SystemBus::getInstance().init();
    SystemBus::getInstance().subscribe(&s_testListener);
    s_testListener.reset();
}

void tearDown(void) {
    SystemBus::getInstance().unsubscribe(&s_testListener);
}

// ----------------------------------------------------------------------------
// Button Plugin Tests
// ----------------------------------------------------------------------------

void test_button_integral_debounce_and_glitch_filtering() {
    ButtonConfig cfg{};
    cfg.pin = 5;
    cfg.pullMode = ButtonPullMode::PULL_UP;
    cfg.activeLow = true; // Pressed is LOW (false)
    cfg.activeEdge = ButtonActiveEdge::EDGE_FALLING; // Trigger on press
    cfg.sampleIntervalMs = 5;
    cfg.debounceThreshold = 4;
    cfg.eventId = 0x0100;
    cfg.eventCode = 42;
    cfg.moduleId = 0x10;

    ButtonPlugin button(cfg);
    TEST_ASSERT_TRUE(button.begin());
    TEST_ASSERT_FALSE(button.isPressed());

    // 1. Noise glitch: pin goes LOW for only 2 ticks (< threshold 4) then back HIGH
    button.simulatePinTransition(false); // LOW (pressed attempt)
    button.tick(); // sample 1 (integrator = 1)
    button.tick(); // sample 2 (integrator = 2)
    TEST_ASSERT_FALSE(button.isPressed());
    TEST_ASSERT_EQUAL(0, s_testListener.eventCount);

    button.simulatePinTransition(true); // noise ends, back HIGH
    button.tick(); // integrator decrements to 1
    button.tick(); // integrator decrements to 0
    TEST_ASSERT_FALSE(button.isPressed());
    TEST_ASSERT_EQUAL(0, s_testListener.eventCount); // Glitch successfully filtered!

    // 2. Stable press: pin goes LOW for >= 4 samples
    button.simulatePinTransition(false);
    button.tick(); // sample 1 (integrator 1)
    button.tick(); // sample 2 (integrator 2)
    button.tick(); // sample 3 (integrator 3)
    TEST_ASSERT_FALSE(button.isPressed());
    TEST_ASSERT_EQUAL(0, s_testListener.eventCount);

    button.tick(); // sample 4 (integrator 4 -> threshold reached!)
    TEST_ASSERT_TRUE(button.isPressed());
    SystemBus::getInstance().dispatchOne();
    TEST_ASSERT_EQUAL(1, s_testListener.eventCount);
    TEST_ASSERT_EQUAL_HEX16(0x0100, s_testListener.lastEventId);
    TEST_ASSERT_EQUAL_UINT32(42, s_testListener.lastEventCode);
    TEST_ASSERT_EQUAL_UINT32(1, s_testListener.lastState); // 1 = pressed

    // 3. Stable release: pin returns HIGH
    button.simulatePinTransition(true);
    for (int i = 0; i < 4; ++i) {
        button.tick();
    }
    TEST_ASSERT_FALSE(button.isPressed());
    // Since activeEdge is FALLING, release does NOT emit another event
    TEST_ASSERT_EQUAL(1, s_testListener.eventCount);
}

void test_button_active_high_and_rising_edge() {
    ButtonConfig cfg{};
    cfg.pin = 7;
    cfg.pullMode = ButtonPullMode::PULL_DOWN;
    cfg.activeLow = false; // Pressed is HIGH (true)
    cfg.activeEdge = ButtonActiveEdge::EDGE_RISING; // Trigger on press
    cfg.sampleIntervalMs = 5;
    cfg.debounceThreshold = 3;
    cfg.eventId = 0x0105;
    cfg.eventCode = 99;
    cfg.moduleId = 0x11;

    ButtonPlugin button(cfg);
    TEST_ASSERT_TRUE(button.begin());
    TEST_ASSERT_FALSE(button.isPressed());

    button.simulatePinTransition(true); // HIGH
    button.tick();
    button.tick();
    button.tick();

    TEST_ASSERT_TRUE(button.isPressed());
    SystemBus::getInstance().dispatchOne();
    TEST_ASSERT_EQUAL(1, s_testListener.eventCount);
    TEST_ASSERT_EQUAL_HEX16(0x0105, s_testListener.lastEventId);
    TEST_ASSERT_EQUAL_UINT32(99, s_testListener.lastEventCode);
}

void test_button_multi_instance_registration() {
    ButtonConfig cfg1{};
    cfg1.pin = 10;
    cfg1.pullMode = ButtonPullMode::PULL_UP;
    cfg1.activeLow = true;
    cfg1.activeEdge = ButtonActiveEdge::EDGE_FALLING;
    cfg1.debounceThreshold = 2;
    cfg1.eventId = 0x0101;
    cfg1.eventCode = 1;
    cfg1.moduleId = 0x20;

    ButtonConfig cfg2{};
    cfg2.pin = 11;
    cfg2.pullMode = ButtonPullMode::PULL_UP;
    cfg2.activeLow = true;
    cfg2.activeEdge = ButtonActiveEdge::EDGE_FALLING;
    cfg2.debounceThreshold = 2;
    cfg2.eventId = 0x0102;
    cfg2.eventCode = 2;
    cfg2.moduleId = 0x21;

    ButtonPlugin btn1(cfg1);
    ButtonPlugin btn2(cfg2);

    TEST_ASSERT_TRUE(btn1.begin());
    TEST_ASSERT_TRUE(btn2.begin());

    // Trigger Button 1
    btn1.simulatePinTransition(false);
    btn1.tick();
    btn1.tick();
    TEST_ASSERT_TRUE(btn1.isPressed());
    TEST_ASSERT_FALSE(btn2.isPressed());
    SystemBus::getInstance().dispatchOne();
    TEST_ASSERT_EQUAL(1, s_testListener.eventCount);
    TEST_ASSERT_EQUAL_HEX16(0x0101, s_testListener.lastEventId);
    TEST_ASSERT_EQUAL_UINT32(1, s_testListener.lastEventCode);

    // Trigger Button 2
    btn2.simulatePinTransition(false);
    btn2.tick();
    btn2.tick();
    TEST_ASSERT_TRUE(btn1.isPressed());
    TEST_ASSERT_TRUE(btn2.isPressed());
    SystemBus::getInstance().dispatchOne();
    TEST_ASSERT_EQUAL(2, s_testListener.eventCount);
    TEST_ASSERT_EQUAL_HEX16(0x0102, s_testListener.lastEventId);
    TEST_ASSERT_EQUAL_UINT32(2, s_testListener.lastEventCode);
}

// ----------------------------------------------------------------------------
// SPI Flash Storage Plugin Tests
// ----------------------------------------------------------------------------

void test_mock_spi_flash_nor_behavior() {
    MockSpiFlashHal<16 * 1024> flash;
    TEST_ASSERT_TRUE(flash.begin());
    TEST_ASSERT_EQUAL_HEX32(0xEF4017, flash.readJedecId());

    uint8_t readBuf[4];
    TEST_ASSERT_TRUE(flash.read(0, readBuf, 4));
    TEST_ASSERT_EQUAL_HEX8(0xFF, readBuf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, readBuf[1]);

    // Program 0xAA (10101010) into byte 0
    uint8_t prog1[1] = { 0xAA };
    TEST_ASSERT_TRUE(flash.writePage(0, prog1, 1));
    flash.read(0, readBuf, 1);
    TEST_ASSERT_EQUAL_HEX8(0xAA, readBuf[0]);

    // Program 0x55 (01010101) into byte 0 without erase -> result should be 0xAA & 0x55 = 0x00
    uint8_t prog2[1] = { 0x55 };
    TEST_ASSERT_TRUE(flash.writePage(0, prog2, 1));
    flash.read(0, readBuf, 1);
    TEST_ASSERT_EQUAL_HEX8(0x00, readBuf[0]);

    // Erase sector 0 (4096 bytes) -> must restore to 0xFF
    TEST_ASSERT_TRUE(flash.eraseSector4K(0));
    flash.read(0, readBuf, 1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, readBuf[0]);
    TEST_ASSERT_EQUAL(1, flash.getEraseCount());
}

void test_spi_flash_storage_lifecycle() {
    MockSpiFlashHal<32 * 1024> flash;
    SpiFlashStorage storage(&flash, 0, 4, 4096);
    TEST_ASSERT_TRUE(storage.begin());

    hal::StorageHandle_t h = storage.beginWrite();
    TEST_ASSERT_NOT_EQUAL(GGG_INVALID_HANDLE, h);

    // Write 300 bytes (crossing 256-byte page boundary)
    uint8_t txData[300];
    for (size_t i = 0; i < sizeof(txData); ++i) {
        txData[i] = (uint8_t)(i ^ 0x5A);
    }

    size_t written = storage.writeData(h, txData, sizeof(txData));
    TEST_ASSERT_EQUAL(sizeof(txData), written);

    TEST_ASSERT_TRUE(storage.commitWrite(h));
    TEST_ASSERT_EQUAL(sizeof(txData), storage.getSize(h));

    // Read back and verify exact data
    uint8_t rxData[300];
    memset(rxData, 0, sizeof(rxData));
    size_t readBytes = storage.readData(h, 0, rxData, sizeof(rxData));
    TEST_ASSERT_EQUAL(sizeof(txData), readBytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(txData, rxData, sizeof(txData));

    // Delete record
    TEST_ASSERT_TRUE(storage.deleteRecord(h));
    TEST_ASSERT_EQUAL(0, storage.getSize(h));
    TEST_ASSERT_EQUAL(0, storage.readData(h, 0, rxData, 10));
}

void test_spi_flash_storage_abort_rollback() {
    MockSpiFlashHal<16 * 1024> flash;
    SpiFlashStorage storage(&flash, 0, 2, 4096);
    TEST_ASSERT_TRUE(storage.begin());

    hal::StorageHandle_t h1 = storage.beginWrite();
    TEST_ASSERT_NOT_EQUAL(GGG_INVALID_HANDLE, h1);

    uint8_t partialData[50] = { 0x12, 0x34 };
    storage.writeData(h1, partialData, sizeof(partialData));

    // Abort transaction
    storage.abortWrite(h1);
    TEST_ASSERT_EQUAL(0, storage.getSize(h1));

    uint8_t rx[10];
    TEST_ASSERT_EQUAL(0, storage.readData(h1, 0, rx, 10));

    // Reclaim: next beginWrite should reuse the sector after erasing
    hal::StorageHandle_t h2 = storage.beginWrite();
    TEST_ASSERT_NOT_EQUAL(GGG_INVALID_HANDLE, h2);
    TEST_ASSERT_NOT_EQUAL(h1, h2);

    uint8_t validData[20] = { "Recovered" };
    storage.writeData(h2, validData, 10);
    TEST_ASSERT_TRUE(storage.commitWrite(h2));
    TEST_ASSERT_EQUAL(10, storage.getSize(h2));
}

void test_spi_flash_storage_random_access() {
    MockSpiFlashHal<16 * 1024> flash;
    SpiFlashStorage storage(&flash, 0, 2, 4096);
    TEST_ASSERT_TRUE(storage.begin());

    hal::StorageHandle_t h = storage.beginWrite();
    uint8_t pattern[100];
    for (int i = 0; i < 100; ++i) pattern[i] = (uint8_t)i;

    storage.writeData(h, pattern, 100);
    storage.commitWrite(h);

    uint8_t slice[10];
    size_t readBytes = storage.readData(h, 40, slice, 10);
    TEST_ASSERT_EQUAL(10, readBytes);
    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT_EQUAL_UINT8(40 + i, slice[i]);
    }
}

int main(void) {
    UNITY_BEGIN();

    // Button Plugin Tests
    RUN_TEST(test_button_integral_debounce_and_glitch_filtering);
    RUN_TEST(test_button_active_high_and_rising_edge);
    RUN_TEST(test_button_multi_instance_registration);

    // SPI Flash Storage Tests
    RUN_TEST(test_mock_spi_flash_nor_behavior);
    RUN_TEST(test_spi_flash_storage_lifecycle);
    RUN_TEST(test_spi_flash_storage_abort_rollback);
    RUN_TEST(test_spi_flash_storage_random_access);

    return UNITY_END();
}
