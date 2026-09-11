/**
 * @file RamStorage.h
 * @brief In-RAM static block storage implementing the ggg::hal::IStorage interface.
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

#ifndef GGG_HAL_RAM_STORAGE_H
#define GGG_HAL_RAM_STORAGE_H

#include "ggg/hal/IStorage.h"
#include <stdint.h>
#include <stddef.h>

#if __has_include("autoconf.h")
#include "autoconf.h"
#endif

// Default compile-time configuration fallback if not set via Kconfig
#if (defined(CONFIG_MUON_STORAGE_BACKEND_SPI_FLASH) || defined(CONFIG_GGG_STORAGE_FLASH_SPI)) && !defined(RUN_NATIVE_TESTS)
#undef CONFIG_GGG_STORAGE_RAM_POOL_SIZE
#define CONFIG_GGG_STORAGE_RAM_POOL_SIZE 0
#undef CONFIG_GGG_STORAGE_MAX_RECORDS
#define CONFIG_GGG_STORAGE_MAX_RECORDS 1
#endif

#ifndef CONFIG_GGG_STORAGE_MAX_RECORDS
#define CONFIG_GGG_STORAGE_MAX_RECORDS 8
#endif

#ifndef CONFIG_GGG_STORAGE_RAM_POOL_SIZE
#define CONFIG_GGG_STORAGE_RAM_POOL_SIZE 4096
#endif

namespace ggg {
namespace hal {

/**
 * @brief Transactional In-Memory Storage Driver (Zero-Malloc).
 * Implements ggg::hal::IStorage with strict ACID semantics:
 * - beginWrite() creates a non-readable uncommitted slot.
 * - writeData() streams data incrementally into the slot.
 * - commitWrite() seals the record, transitioning it to COMMITTED state (read-only).
 * - abortWrite() discards uncommitted data, rolling back the slot immediately to FREE.
 * - readData() allows random access at any offset only on COMMITTED records.
 * - deleteRecord() releases a committed slot back to FREE.
 */
class RamStorage : public IStorage {
public:
    static constexpr size_t MAX_RECORDS = CONFIG_GGG_STORAGE_MAX_RECORDS;
    static constexpr size_t POOL_SIZE   = CONFIG_GGG_STORAGE_RAM_POOL_SIZE;
    static constexpr size_t SLOT_CAPACITY = (MAX_RECORDS > 0) ? (POOL_SIZE / MAX_RECORDS) : 0;

    enum class SlotState : uint8_t {
        FREE = 0,
        WRITING = 1,
        COMMITTED = 2
    };

    RamStorage();
    virtual ~RamStorage() override = default;

    /**
     * @brief Initialises the storage engine (always succeeds for RAM pool).
     */
    bool begin() { return true; }

    // --- IStorage Pure Virtual Interface Implementation ---
    StorageHandle_t beginWrite() override;
    size_t writeData(StorageHandle_t handle, const uint8_t* data, size_t len) override;
    bool commitWrite(StorageHandle_t handle) override;
    void abortWrite(StorageHandle_t handle) override;
    size_t readData(StorageHandle_t handle, size_t offset, uint8_t* dest, size_t len) override;
    size_t getSize(StorageHandle_t handle) override;
    bool deleteRecord(StorageHandle_t handle) override;

    // --- Diagnostic & Inspection Methods ---
    size_t getFreeSpace() const;
    size_t getAvailableSlots() const;
    size_t getCommittedCount() const;
    size_t getWritingCount() const;
    void clear();

private:
    struct StorageSlot {
        SlotState state;
        uint8_t   generation;
        uint16_t  handle;
        size_t    size;
        uint8_t   data[SLOT_CAPACITY];
    };

    StorageSlot _slots[MAX_RECORDS];
    uint8_t     _generationCounter;

    int findSlotIndexByHandle(StorageHandle_t handle) const;
    StorageHandle_t makeHandle(uint8_t slotIndex, uint8_t generation) const;
};

} // namespace hal
} // namespace ggg

#endif // GGG_HAL_RAM_STORAGE_H
