/**
 * @file SpiFlashStorage.h
 * @brief Zero-Malloc transactional storage implementation for SPI NOR Flash memories.
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

#ifndef GGG_SPI_FLASH_STORAGE_H
#define GGG_SPI_FLASH_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include "ggg/hal/IStorage.h"
#include "ggg/core/IPlugin.h"
#include "ggg/plugins/ISpiFlashHal.h"

#ifndef CONFIG_GGG_SPIFLASH_MAX_RECORDS
#define CONFIG_GGG_SPIFLASH_MAX_RECORDS 16
#endif

namespace ggg {
namespace plugins {

#define FLASH_HEADER_MAGIC 0x4747

enum FlashRecordState : uint16_t {
    FLASH_STATE_FREE      = 0xFFFF, // Sector erased, all 1s
    FLASH_STATE_WRITING   = 0xFFFE, // Transaction in progress (bit 0 cleared)
    FLASH_STATE_COMMITTED = 0xFC00, // Valid committed record
    FLASH_STATE_DELETED   = 0x0000  // Deleted / Aborted record (all bits cleared)
};

struct FlashRecordSlot {
    hal::StorageHandle_t handle;
    uint32_t sectorAddress;
    uint32_t currentWriteOffset;
    uint32_t totalLength;
    uint16_t state;
    bool inUse;
};

/**
 * @brief Transactional IStorage driver backed by an SPI NOR Flash chip.
 */
class SpiFlashStorage : public hal::IStorage, public core::IPlugin {
private:
    ISpiFlashHal* _hal;
    bool _ownsHal;
    uint32_t _baseOffset;
    size_t _maxRecords;
    size_t _sectorSize;
    hal::StorageHandle_t _nextHandle;
    bool _isInitialized;

    FlashRecordSlot _slots[CONFIG_GGG_SPIFLASH_MAX_RECORDS];

    int findSlotByHandle(hal::StorageHandle_t handle) const;
    int allocateFreeSlot();

public:
    /**
     * @brief Constructs SpiFlashStorage using Kconfig settings and default hardware driver.
     */
    SpiFlashStorage();

    /**
     * @brief Constructs SpiFlashStorage with an injected HAL driver (for testing/mocking).
     */
    explicit SpiFlashStorage(ISpiFlashHal* hal,
                             uint32_t baseOffset = 0x000000,
                             size_t maxRecords = CONFIG_GGG_SPIFLASH_MAX_RECORDS,
                             size_t sectorSize = 4096);

    ~SpiFlashStorage() override;

    // --- IPlugin Lifecycle ---
    bool begin() override;
    void onEvent(const system::SystemEvent& event) override;
    void tick() override;

    // --- IStorage Transactional Interface ---
    hal::StorageHandle_t beginWrite() override;
    size_t writeData(hal::StorageHandle_t handle, const uint8_t* data, size_t len) override;
    bool commitWrite(hal::StorageHandle_t handle) override;
    void abortWrite(hal::StorageHandle_t handle) override;
    size_t readData(hal::StorageHandle_t handle, size_t offset, uint8_t* dest, size_t len) override;
    size_t getSize(hal::StorageHandle_t handle) override;
    bool deleteRecord(hal::StorageHandle_t handle) override;

    /**
     * @brief Returns the maximum usable payload size per record in bytes.
     */
    size_t getMaxPayloadPerRecord() const { return _sectorSize - 16; }

    /**
     * @brief Returns the count of currently committed active records.
     */
    size_t getCommittedCount() const override;

    /**
     * @brief Returns the underlying ISpiFlashHal pointer.
     */
    ISpiFlashHal* getHal() const { return _hal; }
};

} // namespace plugins
} // namespace ggg

#endif // GGG_SPI_FLASH_STORAGE_H
