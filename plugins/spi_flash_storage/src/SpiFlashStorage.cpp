/**
 * @file SpiFlashStorage.cpp
 * @brief Implementation of transactional SPI NOR Flash storage driver for GGG.
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

#include "ggg/plugins/SpiFlashStorage.h"
#include <string.h>

#if defined(ARDUINO) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)
#include "ggg/plugins/HardwareSpiFlashHal.h"
#else
#include "ggg/plugins/MockSpiFlashHal.h"
#endif

namespace ggg {
namespace plugins {

#pragma pack(push, 1)
struct FlashSectorHeader {
    uint16_t magic;
    uint16_t state;
    uint16_t handle;
    uint16_t reserved;
    uint32_t dataLength;
    uint32_t checksum;
};
#pragma pack(pop)

#define FLASH_HEADER_SIZE sizeof(FlashSectorHeader)

#if defined(ARDUINO) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)
#if defined(CONFIG_GGG_SPIFLASH_USE_CUSTOM_PINS)
static HardwareSpiFlashHal s_defaultHwHal(
    CONFIG_GGG_SPIFLASH_CS_PIN,
    CONFIG_GGG_SPIFLASH_SCK_PIN,
    CONFIG_GGG_SPIFLASH_MOSI_PIN,
    CONFIG_GGG_SPIFLASH_MISO_PIN
);
#else
static HardwareSpiFlashHal s_defaultHwHal(
#if defined(CONFIG_GGG_SPIFLASH_CS_PIN)
    CONFIG_GGG_SPIFLASH_CS_PIN
#else
    4
#endif
);
#endif
#else
static MockSpiFlashHal<128 * 1024> s_defaultMockHal;
#endif

SpiFlashStorage::SpiFlashStorage()
    : _hal(nullptr),
      _ownsHal(false),
      _baseOffset(0),
      _maxRecords(CONFIG_GGG_SPIFLASH_MAX_RECORDS),
      _sectorSize(4096),
      _nextHandle(1),
      _isInitialized(false)
{
#if defined(ARDUINO) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)
    _hal = &s_defaultHwHal;
#else
    _hal = &s_defaultMockHal;
#endif

#if defined(CONFIG_GGG_SPIFLASH_STORAGE_BASE_OFFSET)
    _baseOffset = CONFIG_GGG_SPIFLASH_STORAGE_BASE_OFFSET;
#endif

#if defined(CONFIG_GGG_SPIFLASH_SECTOR_SIZE)
    _sectorSize = CONFIG_GGG_SPIFLASH_SECTOR_SIZE;
#endif

    memset(_slots, 0, sizeof(_slots));
}

SpiFlashStorage::SpiFlashStorage(ISpiFlashHal* hal, uint32_t baseOffset, size_t maxRecords, size_t sectorSize)
    : _hal(hal),
      _ownsHal(false),
      _baseOffset(baseOffset),
      _maxRecords(maxRecords > CONFIG_GGG_SPIFLASH_MAX_RECORDS ? CONFIG_GGG_SPIFLASH_MAX_RECORDS : maxRecords),
      _sectorSize(sectorSize),
      _nextHandle(1),
      _isInitialized(false)
{
    memset(_slots, 0, sizeof(_slots));
}

SpiFlashStorage::~SpiFlashStorage() {
}

bool SpiFlashStorage::begin() {
    if (_hal == nullptr || !_hal->begin()) {
        return false;
    }

    // Initialize slot addresses
    for (size_t i = 0; i < _maxRecords; ++i) {
        _slots[i].sectorAddress = _baseOffset + (uint32_t)(i * _sectorSize);
        _slots[i].handle = GGG_INVALID_HANDLE;
        _slots[i].currentWriteOffset = 0;
        _slots[i].totalLength = 0;
        _slots[i].state = FLASH_STATE_FREE;
        _slots[i].inUse = false;

        // Read existing header from flash
        FlashSectorHeader hdr{};
        if (_hal->read(_slots[i].sectorAddress, reinterpret_cast<uint8_t*>(&hdr), FLASH_HEADER_SIZE)) {
            if (hdr.magic == FLASH_HEADER_MAGIC) {
                _slots[i].state = hdr.state;
                if (hdr.state == FLASH_STATE_COMMITTED) {
                    _slots[i].handle = hdr.handle;
                    _slots[i].totalLength = hdr.dataLength;
                    _slots[i].currentWriteOffset = hdr.dataLength;
                    _slots[i].inUse = true;
                    if (hdr.handle >= _nextHandle) {
                        _nextHandle = hdr.handle + 1;
                    }
                } else if (hdr.state == FLASH_STATE_WRITING) {
                    // Power-loss during write: invalidate incomplete record
                    uint16_t delState = FLASH_STATE_DELETED;
                    _hal->writePage(_slots[i].sectorAddress + 2, reinterpret_cast<const uint8_t*>(&delState), 2);
                    _slots[i].state = FLASH_STATE_DELETED;
                }
            } else if (hdr.magic == 0xFFFF && hdr.state == 0xFFFF) {
                _slots[i].state = FLASH_STATE_FREE;
            } else {
                _slots[i].state = FLASH_STATE_DELETED; // Non-standard or dirty sector
            }
        }
    }

    _isInitialized = true;
    return true;
}

void SpiFlashStorage::onEvent(const system::SystemEvent& event) {
    (void)event;
}

void SpiFlashStorage::tick() {
}

int SpiFlashStorage::findSlotByHandle(hal::StorageHandle_t handle) const {
    if (handle == GGG_INVALID_HANDLE) {
        return -1;
    }
    for (size_t i = 0; i < _maxRecords; ++i) {
        if (_slots[i].inUse && _slots[i].handle == handle) {
            return (int)i;
        }
    }
    return -1;
}

int SpiFlashStorage::allocateFreeSlot() {
    // 1. Look for an erased, completely free sector
    for (size_t i = 0; i < _maxRecords; ++i) {
        if (!_slots[i].inUse && _slots[i].state == FLASH_STATE_FREE) {
            if (_hal->eraseSector4K(_slots[i].sectorAddress)) {
                return (int)i;
            }
        }
    }

    // 2. Look for an obsolete, deleted sector and erase it
    for (size_t i = 0; i < _maxRecords; ++i) {
        if (!_slots[i].inUse && _slots[i].state == FLASH_STATE_DELETED) {
            if (_hal->eraseSector4K(_slots[i].sectorAddress)) {
                _slots[i].state = FLASH_STATE_FREE;
                return (int)i;
            }
        }
    }

    return -1; // Out of storage slots
}

hal::StorageHandle_t SpiFlashStorage::beginWrite() {
    if (!_isInitialized) {
        if (!begin()) {
            return GGG_INVALID_HANDLE;
        }
    }

    int slotIdx = allocateFreeSlot();
    if (slotIdx < 0) {
        return GGG_INVALID_HANDLE;
    }

    FlashRecordSlot& slot = _slots[slotIdx];

    // Allocate next handle (skip invalid handle values)
    hal::StorageHandle_t h = _nextHandle++;
    if (_nextHandle == GGG_INVALID_HANDLE || _nextHandle == 0) {
        _nextHandle = 1;
    }

    slot.handle = h;
    slot.currentWriteOffset = 0;
    slot.totalLength = 0;
    slot.state = FLASH_STATE_WRITING;
    slot.inUse = true;

    // Write initial header with STATE_WRITING
    FlashSectorHeader hdr{};
    hdr.magic = FLASH_HEADER_MAGIC;
    hdr.state = FLASH_STATE_WRITING;
    hdr.handle = h;
    hdr.reserved = 0xFFFF;
    hdr.dataLength = 0xFFFFFFFF; // uncommitted
    hdr.checksum = 0xFFFFFFFF;

    if (!_hal->writePage(slot.sectorAddress, reinterpret_cast<const uint8_t*>(&hdr), FLASH_HEADER_SIZE)) {
        slot.inUse = false;
        slot.state = FLASH_STATE_DELETED;
        return GGG_INVALID_HANDLE;
    }

    return h;
}

size_t SpiFlashStorage::writeData(hal::StorageHandle_t handle, const uint8_t* data, size_t len) {
    if (!_isInitialized || data == nullptr || len == 0) {
        return 0;
    }

    int slotIdx = findSlotByHandle(handle);
    if (slotIdx < 0) {
        return 0;
    }

    FlashRecordSlot& slot = _slots[slotIdx];
    if (slot.state != FLASH_STATE_WRITING) {
        return 0;
    }

    size_t maxPayload = getMaxPayloadPerRecord();
    if (slot.currentWriteOffset + len > maxPayload) {
        len = (slot.currentWriteOffset < maxPayload) ? (maxPayload - slot.currentWriteOffset) : 0;
    }

    size_t written = 0;
    while (written < len) {
        uint32_t flashAddr = slot.sectorAddress + (uint32_t)FLASH_HEADER_SIZE + slot.currentWriteOffset;
        size_t pageOffset = flashAddr & 0xFF;
        size_t pageSpace = 256 - pageOffset;
        size_t chunk = (len - written < pageSpace) ? (len - written) : pageSpace;

        if (!_hal->writePage(flashAddr, data + written, chunk)) {
            break;
        }

        slot.currentWriteOffset += chunk;
        written += chunk;
    }

    slot.totalLength = slot.currentWriteOffset;
    return written;
}

bool SpiFlashStorage::commitWrite(hal::StorageHandle_t handle) {
    if (!_isInitialized) {
        return false;
    }

    int slotIdx = findSlotByHandle(handle);
    if (slotIdx < 0) {
        return false;
    }

    FlashRecordSlot& slot = _slots[slotIdx];
    if (slot.state != FLASH_STATE_WRITING) {
        return false;
    }

    // Program totalLength (offset 8 in header)
    uint32_t len = slot.totalLength;
    if (!_hal->writePage(slot.sectorAddress + 8, reinterpret_cast<const uint8_t*>(&len), sizeof(len))) {
        return false;
    }

    // Program STATE_COMMITTED (offset 2 in header)
    uint16_t commState = FLASH_STATE_COMMITTED;
    if (!_hal->writePage(slot.sectorAddress + 2, reinterpret_cast<const uint8_t*>(&commState), sizeof(commState))) {
        return false;
    }

    slot.state = FLASH_STATE_COMMITTED;
    return true;
}

void SpiFlashStorage::abortWrite(hal::StorageHandle_t handle) {
    if (!_isInitialized) {
        return;
    }

    int slotIdx = findSlotByHandle(handle);
    if (slotIdx < 0) {
        return;
    }

    FlashRecordSlot& slot = _slots[slotIdx];

    // Transition state to DELETED (0x0000)
    uint16_t delState = FLASH_STATE_DELETED;
    _hal->writePage(slot.sectorAddress + 2, reinterpret_cast<const uint8_t*>(&delState), sizeof(delState));

    slot.inUse = false;
    slot.state = FLASH_STATE_DELETED;
    slot.handle = GGG_INVALID_HANDLE;
}

size_t SpiFlashStorage::readData(hal::StorageHandle_t handle, size_t offset, uint8_t* dest, size_t len) {
    if (!_isInitialized || dest == nullptr || len == 0) {
        return 0;
    }

    int slotIdx = findSlotByHandle(handle);
    if (slotIdx < 0) {
        return 0;
    }

    const FlashRecordSlot& slot = _slots[slotIdx];
    if (slot.state != FLASH_STATE_COMMITTED) {
        return 0; // Not readable
    }

    if (offset >= slot.totalLength) {
        return 0;
    }

    if (offset + len > slot.totalLength) {
        len = slot.totalLength - offset;
    }

    uint32_t flashAddr = slot.sectorAddress + (uint32_t)FLASH_HEADER_SIZE + (uint32_t)offset;
    if (!_hal->read(flashAddr, dest, len)) {
        return 0;
    }

    return len;
}

size_t SpiFlashStorage::getSize(hal::StorageHandle_t handle) {
    int slotIdx = findSlotByHandle(handle);
    if (slotIdx < 0) {
        return 0;
    }
    const FlashRecordSlot& slot = _slots[slotIdx];
    if (slot.state != FLASH_STATE_COMMITTED) {
        return 0;
    }
    return slot.totalLength;
}

bool SpiFlashStorage::deleteRecord(hal::StorageHandle_t handle) {
    if (!_isInitialized) {
        return false;
    }

    int slotIdx = findSlotByHandle(handle);
    if (slotIdx < 0) {
        return false;
    }

    FlashRecordSlot& slot = _slots[slotIdx];

    // Transition state to DELETED (0x0000)
    uint16_t delState = FLASH_STATE_DELETED;
    _hal->writePage(slot.sectorAddress + 2, reinterpret_cast<const uint8_t*>(&delState), sizeof(delState));

    slot.inUse = false;
    slot.state = FLASH_STATE_DELETED;
    slot.handle = GGG_INVALID_HANDLE;
    return true;
}

size_t SpiFlashStorage::getCommittedCount() const {
    size_t count = 0;
    for (size_t i = 0; i < _maxRecords; ++i) {
        if (_slots[i].inUse && _slots[i].state == FLASH_STATE_COMMITTED) {
            count++;
        }
    }
    return count;
}

} // namespace plugins
} // namespace ggg
