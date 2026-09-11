/**
 * @file RamStorage.cpp
 * @brief Implementation of static RamStorage with atomic commit and rollback support.
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

#include "ggg/hal/RamStorage.h"
#include <cstring>

namespace ggg {
namespace hal {

RamStorage::RamStorage() : _generationCounter(1) {
    clear();
}

void RamStorage::clear() {
    for (size_t i = 0; i < MAX_RECORDS; ++i) {
        _slots[i].state = SlotState::FREE;
        _slots[i].generation = 0;
        _slots[i].handle = GGG_INVALID_HANDLE;
        _slots[i].size = 0;
        memset(_slots[i].data, 0, sizeof(_slots[i].data));
    }
}

StorageHandle_t RamStorage::makeHandle(uint8_t slotIndex, uint8_t generation) const {
    return static_cast<StorageHandle_t>((static_cast<uint16_t>(generation) << 8) | (slotIndex & 0xFF));
}

int RamStorage::findSlotIndexByHandle(StorageHandle_t handle) const {
    if (handle == GGG_INVALID_HANDLE || handle == 0) {
        return -1;
    }
    uint8_t slotIndex = static_cast<uint8_t>(handle & 0xFF);
    if (slotIndex >= MAX_RECORDS) {
        return -1;
    }
    if (_slots[slotIndex].handle != handle) {
        return -1; // Stale or mismatched handle
    }
    return static_cast<int>(slotIndex);
}

StorageHandle_t RamStorage::beginWrite() {
    for (size_t i = 0; i < MAX_RECORDS; ++i) {
        if (_slots[i].state == SlotState::FREE) {
            _generationCounter++;
            if (_generationCounter == 0 || _generationCounter == 0xFF) {
                _generationCounter = 1;
            }

            StorageHandle_t handle = makeHandle(static_cast<uint8_t>(i), _generationCounter);
            _slots[i].state = SlotState::WRITING;
            _slots[i].generation = _generationCounter;
            _slots[i].handle = handle;
            _slots[i].size = 0;
            return handle;
        }
    }
    return GGG_INVALID_HANDLE; // Pool exhausted
}

size_t RamStorage::writeData(StorageHandle_t handle, const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return 0;
    }

    int idx = findSlotIndexByHandle(handle);
    if (idx < 0) {
        return 0;
    }

    StorageSlot& slot = _slots[idx];
    if (slot.state != SlotState::WRITING) {
        return 0; // Cannot write to COMMITTED or FREE slots
    }

    if (slot.size >= SLOT_CAPACITY) {
        return 0; // Slot capacity reached
    }

    size_t remaining = SLOT_CAPACITY - slot.size;
    size_t toCopy = (len < remaining) ? len : remaining;

    if (toCopy > 0) {
        memcpy(&slot.data[slot.size], data, toCopy);
        slot.size += toCopy;
    }

    return toCopy;
}

bool RamStorage::commitWrite(StorageHandle_t handle) {
    int idx = findSlotIndexByHandle(handle);
    if (idx < 0) {
        return false;
    }

    StorageSlot& slot = _slots[idx];
    if (slot.state != SlotState::WRITING) {
        return false;
    }

    slot.state = SlotState::COMMITTED;
    return true;
}

void RamStorage::abortWrite(StorageHandle_t handle) {
    int idx = findSlotIndexByHandle(handle);
    if (idx < 0) {
        return;
    }

    StorageSlot& slot = _slots[idx];
    if (slot.state == SlotState::WRITING) {
        slot.state = SlotState::FREE;
        slot.handle = GGG_INVALID_HANDLE;
        slot.size = 0;
    }
}

size_t RamStorage::readData(StorageHandle_t handle, size_t offset, uint8_t* dest, size_t len) {
    if (dest == nullptr || len == 0) {
        return 0;
    }

    int idx = findSlotIndexByHandle(handle);
    if (idx < 0) {
        return 0;
    }

    const StorageSlot& slot = _slots[idx];
    if (slot.state != SlotState::COMMITTED) {
        return 0; // Isolation: Cannot read uncommitted records
    }

    if (offset >= slot.size) {
        return 0; // Offset beyond recorded size
    }

    size_t available = slot.size - offset;
    size_t toCopy = (len < available) ? len : available;

    if (toCopy > 0) {
        memcpy(dest, &slot.data[offset], toCopy);
    }

    return toCopy;
}

size_t RamStorage::getSize(StorageHandle_t handle) {
    int idx = findSlotIndexByHandle(handle);
    if (idx < 0) {
        return 0;
    }

    const StorageSlot& slot = _slots[idx];
    if (slot.state != SlotState::COMMITTED) {
        return 0;
    }

    return slot.size;
}

bool RamStorage::deleteRecord(StorageHandle_t handle) {
    int idx = findSlotIndexByHandle(handle);
    if (idx < 0) {
        return false;
    }

    StorageSlot& slot = _slots[idx];
    if (slot.state != SlotState::COMMITTED) {
        return false; // Only committed records can be deleted
    }

    slot.state = SlotState::FREE;
    slot.handle = GGG_INVALID_HANDLE;
    slot.size = 0;
    return true;
}

size_t RamStorage::getAvailableSlots() const {
    size_t count = 0;
    for (size_t i = 0; i < MAX_RECORDS; ++i) {
        if (_slots[i].state == SlotState::FREE) {
            count++;
        }
    }
    return count;
}

size_t RamStorage::getCommittedCount() const {
    size_t count = 0;
    for (size_t i = 0; i < MAX_RECORDS; ++i) {
        if (_slots[i].state == SlotState::COMMITTED) {
            count++;
        }
    }
    return count;
}

size_t RamStorage::getWritingCount() const {
    size_t count = 0;
    for (size_t i = 0; i < MAX_RECORDS; ++i) {
        if (_slots[i].state == SlotState::WRITING) {
            count++;
        }
    }
    return count;
}

size_t RamStorage::getFreeSpace() const {
    return getAvailableSlots() * SLOT_CAPACITY;
}

} // namespace hal
} // namespace ggg
