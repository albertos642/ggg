/**
 * @file test_main.cpp
 * @brief Unit and integration tests for GGG core, HAL storage, and System Bus.
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
#include <stdint.h>
#include <string.h>
#include "ggg.h"
#include "autoconf.h"
#include "ggg/hal/RamStorage.h"
#include "ggg/system/SystemBus.h"

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// 1. SystemEvent & Payload Size Tests
// ============================================================================

void test_system_event_structure(void) {
    // Assert payload union is strictly limited to 8 bytes
    static_assert(sizeof(decltype(ggg::system::SystemEvent::payload)) == 8,
                  "SystemEvent::payload union MUST be exactly 8 bytes!");

    ggg::system::SystemEvent evt;
    evt.type = ggg::system::GGG_EVT_STARTUP;
    evt.source = 1;
    evt.priority = 100;
    evt.payload.u32[0] = 0x12345678;
    evt.payload.u32[1] = 0x9ABCDEF0;

    TEST_ASSERT_EQUAL_UINT16(ggg::system::GGG_EVT_STARTUP, evt.type);
    TEST_ASSERT_EQUAL_UINT8(1, evt.source);
    TEST_ASSERT_EQUAL_UINT8(100, evt.priority);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, evt.payload.u32[0]);
    TEST_ASSERT_EQUAL_HEX32(0x9ABCDEF0, evt.payload.u32[1]);
    TEST_ASSERT_EQUAL_size_t(8, sizeof(evt.payload));
}

void test_kconfig_autoconf_macros(void) {
#if defined(CONFIG_GGG_SYSTEM_BUS_QUEUE_SIZE)
    TEST_ASSERT_TRUE(CONFIG_GGG_SYSTEM_BUS_QUEUE_SIZE > 0);
#else
    TEST_FAIL_MESSAGE("CONFIG_GGG_SYSTEM_BUS_QUEUE_SIZE macro not found!");
#endif

#if defined(CONFIG_GGG_HAL_RAM_STORAGE)
    TEST_ASSERT_EQUAL_INT(1, CONFIG_GGG_HAL_RAM_STORAGE);
#else
    TEST_FAIL_MESSAGE("CONFIG_GGG_HAL_RAM_STORAGE macro not found!");
#endif

#if defined(CONFIG_GGG_SYSTEM_BUS)
    TEST_ASSERT_EQUAL_INT(1, CONFIG_GGG_SYSTEM_BUS);
#else
    TEST_FAIL_MESSAGE("CONFIG_GGG_SYSTEM_BUS macro not found!");
#endif
}

// ============================================================================
// 2. RamStorage Tests
// ============================================================================

void test_ram_storage_lifecycle(void) {
    ggg::hal::RamStorage storage;
    TEST_ASSERT_EQUAL_size_t(storage.MAX_RECORDS, storage.getAvailableSlots());

    ggg::hal::StorageHandle_t h = storage.beginWrite();
    TEST_ASSERT_NOT_EQUAL(GGG_INVALID_HANDLE, h);
    TEST_ASSERT_EQUAL_size_t(storage.MAX_RECORDS - 1, storage.getAvailableSlots());
    TEST_ASSERT_EQUAL_size_t(1, storage.getWritingCount());
    TEST_ASSERT_EQUAL_size_t(0, storage.getCommittedCount());

    const uint8_t chunk1[] = "Hello ";
    const uint8_t chunk2[] = "World 123";
    size_t w1 = storage.writeData(h, chunk1, 6);
    TEST_ASSERT_EQUAL_size_t(6, w1);
    size_t w2 = storage.writeData(h, chunk2, 9);
    TEST_ASSERT_EQUAL_size_t(9, w2);

    uint8_t readBuf[32] = {0};
    size_t rEarly = storage.readData(h, 0, readBuf, sizeof(readBuf));
    TEST_ASSERT_EQUAL_size_t(0, rEarly);
    TEST_ASSERT_EQUAL_size_t(0, storage.getSize(h));

    bool committed = storage.commitWrite(h);
    TEST_ASSERT_TRUE(committed);
    TEST_ASSERT_EQUAL_size_t(1, storage.getCommittedCount());
    TEST_ASSERT_EQUAL_size_t(0, storage.getWritingCount());

    const uint8_t extra[] = "More";
    size_t wAfter = storage.writeData(h, extra, 4);
    TEST_ASSERT_EQUAL_size_t(0, wAfter);

    TEST_ASSERT_EQUAL_size_t(15, storage.getSize(h));
    size_t rTotal = storage.readData(h, 0, readBuf, sizeof(readBuf));
    TEST_ASSERT_EQUAL_size_t(15, rTotal);
    TEST_ASSERT_EQUAL_STRING_LEN("Hello World 123", (char*)readBuf, 15);

    uint8_t offsetBuf[10] = {0};
    size_t rOffset = storage.readData(h, 6, offsetBuf, 5);
    TEST_ASSERT_EQUAL_size_t(5, rOffset);
    TEST_ASSERT_EQUAL_STRING_LEN("World", (char*)offsetBuf, 5);

    bool deleted = storage.deleteRecord(h);
    TEST_ASSERT_TRUE(deleted);
    TEST_ASSERT_EQUAL_size_t(0, storage.getSize(h));
    TEST_ASSERT_EQUAL_size_t(0, storage.readData(h, 0, readBuf, sizeof(readBuf)));
    TEST_ASSERT_EQUAL_size_t(storage.MAX_RECORDS, storage.getAvailableSlots());
    TEST_ASSERT_FALSE(storage.deleteRecord(h));
}

void test_ram_storage_abort_rollback(void) {
    ggg::hal::RamStorage storage;
    size_t initSlots = storage.getAvailableSlots();

    ggg::hal::StorageHandle_t h = storage.beginWrite();
    TEST_ASSERT_NOT_EQUAL(GGG_INVALID_HANDLE, h);
    TEST_ASSERT_EQUAL_size_t(initSlots - 1, storage.getAvailableSlots());

    const uint8_t partialData[] = "Partial Corrupted Frame Data";
    storage.writeData(h, partialData, sizeof(partialData));

    storage.abortWrite(h);

    TEST_ASSERT_EQUAL_size_t(initSlots, storage.getAvailableSlots());
    TEST_ASSERT_EQUAL_size_t(0, storage.getWritingCount());
    TEST_ASSERT_EQUAL_size_t(0, storage.getCommittedCount());

    TEST_ASSERT_FALSE(storage.commitWrite(h));
    uint8_t buf[16];
    TEST_ASSERT_EQUAL_size_t(0, storage.readData(h, 0, buf, sizeof(buf)));
}

void test_ram_storage_capacity_exhaustion(void) {
    ggg::hal::RamStorage storage;
    ggg::hal::StorageHandle_t handles[ggg::hal::RamStorage::MAX_RECORDS];

    for (size_t i = 0; i < storage.MAX_RECORDS; ++i) {
        handles[i] = storage.beginWrite();
        TEST_ASSERT_NOT_EQUAL(GGG_INVALID_HANDLE, handles[i]);
    }
    TEST_ASSERT_EQUAL_size_t(0, storage.getAvailableSlots());

    ggg::hal::StorageHandle_t overflowHandle = storage.beginWrite();
    TEST_ASSERT_EQUAL(GGG_INVALID_HANDLE, overflowHandle);

    for (size_t i = 0; i < storage.MAX_RECORDS; ++i) {
        storage.commitWrite(handles[i]);
    }
    TEST_ASSERT_EQUAL_size_t(storage.MAX_RECORDS, storage.getCommittedCount());

    TEST_ASSERT_TRUE(storage.deleteRecord(handles[0]));
    TEST_ASSERT_EQUAL_size_t(1, storage.getAvailableSlots());

    ggg::hal::StorageHandle_t recycled = storage.beginWrite();
    TEST_ASSERT_NOT_EQUAL(GGG_INVALID_HANDLE, recycled);
}

void test_ram_storage_stale_handle_protection(void) {
    ggg::hal::RamStorage storage;

    ggg::hal::StorageHandle_t h1 = storage.beginWrite();
    TEST_ASSERT_NOT_EQUAL(GGG_INVALID_HANDLE, h1);
    storage.commitWrite(h1);
    storage.deleteRecord(h1);

    ggg::hal::StorageHandle_t h2 = storage.beginWrite();
    TEST_ASSERT_NOT_EQUAL(GGG_INVALID_HANDLE, h2);
    TEST_ASSERT_NOT_EQUAL(h1, h2);

    const uint8_t payload[] = "New Content";
    storage.writeData(h2, payload, sizeof(payload));
    storage.commitWrite(h2);

    uint8_t buf[16];
    TEST_ASSERT_EQUAL_size_t(0, storage.readData(h1, 0, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_size_t(0, storage.getSize(h1));
    TEST_ASSERT_FALSE(storage.deleteRecord(h1));

    TEST_ASSERT_EQUAL_size_t(sizeof(payload), storage.getSize(h2));
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), storage.readData(h2, 0, buf, sizeof(buf)));
}

// ============================================================================
// 3. SystemBus Tests
// ============================================================================

class MockListener : public ggg::system::IEventListener {
public:
    int callCount = 0;
    ggg::system::SystemEvent lastEvent = {};

    void onEvent(const ggg::system::SystemEvent& event) override {
        callCount++;
        lastEvent = event;
    }
};

void test_system_bus_subscribe_unsubscribe(void) {
    ggg::system::SystemBus& bus = ggg::system::SystemBus::getInstance();
    bus.reset();
    TEST_ASSERT_EQUAL_size_t(0, bus.getListenerCount());

    MockListener l1;
    MockListener l2;

    TEST_ASSERT_TRUE(bus.subscribe(&l1));
    TEST_ASSERT_EQUAL_size_t(1, bus.getListenerCount());

    // Duplicate subscription must not increment count
    TEST_ASSERT_TRUE(bus.subscribe(&l1));
    TEST_ASSERT_EQUAL_size_t(1, bus.getListenerCount());

    TEST_ASSERT_TRUE(bus.subscribe(&l2));
    TEST_ASSERT_EQUAL_size_t(2, bus.getListenerCount());

    TEST_ASSERT_TRUE(bus.unsubscribe(&l1));
    TEST_ASSERT_EQUAL_size_t(1, bus.getListenerCount());

    TEST_ASSERT_FALSE(bus.unsubscribe(&l1)); // Already unsubscribed
    TEST_ASSERT_TRUE(bus.unsubscribe(&l2));
    TEST_ASSERT_EQUAL_size_t(0, bus.getListenerCount());
}

void test_system_bus_publish_dispatch(void) {
    ggg::system::SystemBus& bus = ggg::system::SystemBus::getInstance();
    bus.reset();

    MockListener l1;
    MockListener l2;
    bus.subscribe(&l1);
    bus.subscribe(&l2);

    ggg::system::SystemEvent evt;
    evt.type = ggg::system::GGG_EVT_APP_TRIGGER;
    evt.source = 0x42;
    evt.priority = 10;
    evt.payload.u32[0] = 12345;
    evt.payload.u32[1] = 67890;

    TEST_ASSERT_TRUE(bus.publish(evt));
    TEST_ASSERT_EQUAL_size_t(1, bus.getPendingCount());

    // Listeners have not been called yet (dispatching is asynchronous)
    TEST_ASSERT_EQUAL_INT(0, l1.callCount);
    TEST_ASSERT_EQUAL_INT(0, l2.callCount);

    // Dispatch one event
    TEST_ASSERT_TRUE(bus.dispatchOne());
    TEST_ASSERT_EQUAL_size_t(0, bus.getPendingCount());

    // Both listeners must have received the event
    TEST_ASSERT_EQUAL_INT(1, l1.callCount);
    TEST_ASSERT_EQUAL_UINT16(ggg::system::GGG_EVT_APP_TRIGGER, l1.lastEvent.type);
    TEST_ASSERT_EQUAL_UINT8(0x42, l1.lastEvent.source);
    TEST_ASSERT_EQUAL_UINT8(10, l1.lastEvent.priority);
    TEST_ASSERT_EQUAL_HEX32(12345, l1.lastEvent.payload.u32[0]);
    TEST_ASSERT_EQUAL_HEX32(67890, l1.lastEvent.payload.u32[1]);

    TEST_ASSERT_EQUAL_INT(1, l2.callCount);
    TEST_ASSERT_EQUAL_HEX32(12345, l2.lastEvent.payload.u32[0]);

    // Subsequent dispatch with empty queue returns false
    TEST_ASSERT_FALSE(bus.dispatchOne());
}

void test_system_bus_queue_saturation(void) {
    ggg::system::SystemBus& bus = ggg::system::SystemBus::getInstance();
    bus.reset();

    // Fill queue to capacity
    for (size_t i = 0; i < bus.QUEUE_SIZE; ++i) {
        ggg::system::SystemEvent e = {};
        e.type = ggg::system::GGG_EVT_SYS_TICK;
        e.payload.u32[0] = static_cast<uint32_t>(i);
        TEST_ASSERT_TRUE(bus.publish(e));
    }
    TEST_ASSERT_EQUAL_size_t(bus.QUEUE_SIZE, bus.getPendingCount());

    // Next publish must return false (Zero-Malloc drop policy, no heap allocation!)
    ggg::system::SystemEvent overflowEvt = {};
    overflowEvt.type = ggg::system::GGG_EVT_HARDWARE_FAULT;
    TEST_ASSERT_FALSE(bus.publish(overflowEvt));

    // Dispatch one event to free a slot
    MockListener dummy;
    bus.subscribe(&dummy);
    TEST_ASSERT_TRUE(bus.dispatchOne());
    TEST_ASSERT_EQUAL_size_t(bus.QUEUE_SIZE - 1, bus.getPendingCount());

    // Now publishing must succeed again
    TEST_ASSERT_TRUE(bus.publish(overflowEvt));
    TEST_ASSERT_EQUAL_size_t(bus.QUEUE_SIZE, bus.getPendingCount());
}

class MockPlugin : public ggg::core::IPlugin {
public:
    bool begun = false;
    int eventCount = 0;
    int tickCount = 0;

    bool begin() override { begun = true; return true; }
    void onEvent(const ggg::system::SystemEvent& event) override {
        (void)event;
        eventCount++;
    }
    void tick() override { tickCount++; }
};

void test_plugin_event_listener_integration(void) {
    ggg::system::SystemBus& bus = ggg::system::SystemBus::getInstance();
    bus.reset();

    MockPlugin plugin;
    TEST_ASSERT_TRUE(plugin.begin());
    TEST_ASSERT_TRUE(plugin.begun);

    // IPlugin can be subscribed directly as an IEventListener!
    TEST_ASSERT_TRUE(bus.subscribe(&plugin));
    TEST_ASSERT_EQUAL_size_t(1, bus.getListenerCount());

    ggg::system::SystemEvent evt = {};
    evt.type = ggg::system::GGG_EVT_STARTUP;
    bus.publish(evt);
    bus.dispatchOne();

    TEST_ASSERT_EQUAL_INT(1, plugin.eventCount);
}

// ============================================================================
// Main Unity Test Runner
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    // 1. Core & Event types
    RUN_TEST(test_system_event_structure);
    RUN_TEST(test_kconfig_autoconf_macros);

    // 2. HAL Storage
    RUN_TEST(test_ram_storage_lifecycle);
    RUN_TEST(test_ram_storage_abort_rollback);
    RUN_TEST(test_ram_storage_capacity_exhaustion);
    RUN_TEST(test_ram_storage_stale_handle_protection);

    // 3. SystemBus RTOS Core
    RUN_TEST(test_system_bus_subscribe_unsubscribe);
    RUN_TEST(test_system_bus_publish_dispatch);
    RUN_TEST(test_system_bus_queue_saturation);
    RUN_TEST(test_plugin_event_listener_integration);

    return UNITY_END();
}
