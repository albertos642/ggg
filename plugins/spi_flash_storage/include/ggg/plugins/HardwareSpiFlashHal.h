/**
 * @file HardwareSpiFlashHal.h
 * @brief Hardware SPI driver for JEDEC SPI NOR Flash (Winbond W25Q series).
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

#ifndef GGG_HARDWARE_SPI_FLASH_HAL_H
#define GGG_HARDWARE_SPI_FLASH_HAL_H

#include "ggg/plugins/ISpiFlashHal.h"

#if __has_include("autoconf.h")
#include "autoconf.h"
#endif

#if defined(ARDUINO) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)
#include <Arduino.h>
#include <SPI.h>

namespace ggg {
namespace plugins {

class HardwareSpiFlashHal : public ISpiFlashHal {
private:
    uint32_t _csPin;
    uint32_t _sckPin;
    uint32_t _mosiPin;
    uint32_t _misoPin;
    bool _customPins;
    size_t _capacityBytes;
    SPIClass* _spi;

    void select();
    void deselect();
    void writeEnable();
    bool waitNotBusy(uint32_t timeoutMs = 500);

public:
    /**
     * @brief Constructor using default hardware SPI bus.
     */
    explicit HardwareSpiFlashHal(uint32_t csPin, size_t capacityBytes = 8 * 1024 * 1024);

    /**
     * @brief Constructor using custom SPI pins.
     */
    HardwareSpiFlashHal(uint32_t csPin, uint32_t sckPin, uint32_t mosiPin, uint32_t misoPin, size_t capacityBytes = 8 * 1024 * 1024);

    bool begin() override;
    bool read(uint32_t address, uint8_t* buffer, size_t length) override;
    bool writePage(uint32_t address, const uint8_t* buffer, size_t length) override;
    bool eraseSector4K(uint32_t sectorAddress) override;
    uint32_t readJedecId() override;
    uint8_t readStatus();
    bool unprotect();
    size_t getCapacityBytes() const override { return _capacityBytes; }
};

} // namespace plugins
} // namespace ggg

#endif // ARDUINO || !TARGET_NATIVE

#endif // GGG_HARDWARE_SPI_FLASH_HAL_H
