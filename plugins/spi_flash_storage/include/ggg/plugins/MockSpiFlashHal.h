/**
 * @file MockSpiFlashHal.h
 * @brief High-fidelity in-memory simulator for SPI NOR Flash chips.
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

#ifndef GGG_MOCK_SPI_FLASH_HAL_H
#define GGG_MOCK_SPI_FLASH_HAL_H

#include <string.h>
#include "ggg/plugins/ISpiFlashHal.h"

namespace ggg {
namespace plugins {

/**
 * @brief Deterministic in-memory simulation of a Winbond W25Q-series SPI NOR Flash.
 * Accurately models NOR bit-clearing (1 -> 0) and sector erase (4KB blocks reset to 0xFF).
 */
template <size_t FlashSizeBytes = 64 * 1024>
class MockSpiFlashHal : public ISpiFlashHal {
private:
    uint8_t _memory[FlashSizeBytes];
    uint32_t _eraseCount;
    uint32_t _pageWriteCount;
    bool _isInitialized;

public:
    MockSpiFlashHal()
        : _eraseCount(0),
          _pageWriteCount(0),
          _isInitialized(false)
    {
        memset(_memory, 0xFF, sizeof(_memory));
    }

    bool begin() override {
        _isInitialized = true;
        return true;
    }

    bool read(uint32_t address, uint8_t* buffer, size_t length) override {
        if (!_isInitialized || address + length > FlashSizeBytes || buffer == nullptr) {
            return false;
        }
        memcpy(buffer, &_memory[address], length);
        return true;
    }

    bool writePage(uint32_t address, const uint8_t* buffer, size_t length) override {
        if (!_isInitialized || address + length > FlashSizeBytes || buffer == nullptr || length == 0) {
            return false;
        }
        // Winbond W25Q: Page programming cannot cross a 256-byte page boundary
        if ((address & ~0xFF) != ((address + length - 1) & ~0xFF)) {
            return false;
        }

        // Simulate NOR physics: bits can only be cleared (1 -> 0) without prior sector erase
        for (size_t i = 0; i < length; ++i) {
            _memory[address + i] &= buffer[i];
        }
        _pageWriteCount++;
        return true;
    }

    bool eraseSector4K(uint32_t sectorAddress) override {
        if (!_isInitialized || sectorAddress >= FlashSizeBytes) {
            return false;
        }
        uint32_t base = sectorAddress & ~0xFFF; // Align to 4096-byte boundary
        if (base + 4096 > FlashSizeBytes) {
            return false;
        }
        memset(&_memory[base], 0xFF, 4096);
        _eraseCount++;
        return true;
    }

    uint32_t readJedecId() override {
        // W25Q64: Manufacturer 0xEF, MemoryType 0x40, Capacity 0x17
        return 0xEF4017;
    }

    size_t getCapacityBytes() const override {
        return FlashSizeBytes;
    }

    uint32_t getEraseCount() const { return _eraseCount; }
    uint32_t getPageWriteCount() const { return _pageWriteCount; }
    const uint8_t* rawMemory() const { return _memory; }
};

} // namespace plugins
} // namespace ggg

#endif // GGG_MOCK_SPI_FLASH_HAL_H
