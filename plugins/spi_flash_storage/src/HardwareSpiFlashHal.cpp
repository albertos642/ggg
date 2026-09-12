/**
 * @file HardwareSpiFlashHal.cpp
 * @brief Implementation of hardware SPI driver for Winbond W25Q Flash.
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

#include "ggg/plugins/HardwareSpiFlashHal.h"

#if defined(ARDUINO) && !defined(GGG_TARGET_NATIVE) && !defined(TARGET_NATIVE)

#define CMD_WRITE_ENABLE    0x06
#define CMD_WRITE_STATUS_1  0x01
#define CMD_READ_STATUS_1   0x05
#define CMD_READ_DATA       0x03
#define CMD_PAGE_PROGRAM    0x02
#define CMD_SECTOR_ERASE_4K 0x20
#define CMD_JEDEC_ID        0x9F

#define STATUS_BUSY_MASK    0x01

extern "C" {
__attribute__((weak)) void muonSpiLock() {}
__attribute__((weak)) void muonSpiUnlock() {}
}

namespace ggg {
namespace plugins {

HardwareSpiFlashHal::HardwareSpiFlashHal(uint32_t csPin, size_t capacityBytes)
    : _csPin(csPin),
      _sckPin(0),
      _mosiPin(0),
      _misoPin(0),
      _customPins(false),
      _capacityBytes(capacityBytes),
      _spi(&SPI)
{
}

HardwareSpiFlashHal::HardwareSpiFlashHal(uint32_t csPin, uint32_t sckPin, uint32_t mosiPin, uint32_t misoPin, size_t capacityBytes)
    : _csPin(csPin),
      _sckPin(sckPin),
      _mosiPin(mosiPin),
      _misoPin(misoPin),
      _customPins(true),
      _capacityBytes(capacityBytes),
      _spi(&SPI)
{
}

void HardwareSpiFlashHal::select() {
    muonSpiLock();
    _spi->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);
}

void HardwareSpiFlashHal::deselect() {
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
    muonSpiUnlock();
}

bool HardwareSpiFlashHal::begin() {
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);

    if (_customPins) {
        // If board core supports custom SPI pins, configure here
        _spi->begin();
    } else {
        _spi->begin();
    }

    // Release from Deep Power-Down (instruction 0xAB) in case chip is in sleep mode
    select();
    _spi->transfer(0xAB);
    deselect();
    delayMicroseconds(50); // tRES1 recovery delay

    // Retry reading JEDEC ID up to 3 times
    uint32_t id = 0;
    for (int retry = 0; retry < 3; ++retry) {
        id = readJedecId();
        if (id != 0x000000 && id != 0xFFFFFF) {
            break;
        }
        delay(10);
    }

    if (id == 0x000000 || id == 0xFFFFFF) {
        return false; // Hardware unresponsive
    }

    // Clear any hardware block protection bits (BP0..BP2) so that sectors can be erased and written
    unprotect();

    return true;
}

void HardwareSpiFlashHal::writeEnable() {
    waitNotBusy(100);
    select();
    _spi->transfer(CMD_WRITE_ENABLE);
    deselect();
}

bool HardwareSpiFlashHal::waitNotBusy(uint32_t timeoutMs) {
    uint32_t start = millis();
    while ((millis() - start) < timeoutMs) {
        select();
        _spi->transfer(CMD_READ_STATUS_1);
        uint8_t status = _spi->transfer(0x00);
        deselect();

        if ((status & STATUS_BUSY_MASK) == 0) {
            return true;
        }
        delay(1);
    }
    return false;
}

uint8_t HardwareSpiFlashHal::readStatus() {
    select();
    _spi->transfer(CMD_READ_STATUS_1);
    uint8_t status = _spi->transfer(0x00);
    deselect();
    return status;
}

bool HardwareSpiFlashHal::unprotect() {
    writeEnable();
    select();
    _spi->transfer(CMD_WRITE_STATUS_1);
    _spi->transfer(0x00); // Status Register 1 (BP0..BP2 = 0)
    _spi->transfer(0x00); // Status Register 2
    deselect();
    return waitNotBusy(100);
}

bool HardwareSpiFlashHal::read(uint32_t address, uint8_t* buffer, size_t length) {
    if (address + length > _capacityBytes || buffer == nullptr || length == 0) {
        return false;
    }

    if (!waitNotBusy(100)) {
        return false;
    }

    select();
    _spi->transfer(CMD_READ_DATA);
    _spi->transfer((address >> 16) & 0xFF);
    _spi->transfer((address >> 8) & 0xFF);
    _spi->transfer(address & 0xFF);

    for (size_t i = 0; i < length; ++i) {
        buffer[i] = _spi->transfer(0x00);
    }
    deselect();

    return true;
}

bool HardwareSpiFlashHal::writePage(uint32_t address, const uint8_t* buffer, size_t length) {
    if (address + length > _capacityBytes || buffer == nullptr || length == 0 || length > 256) {
        return false;
    }

    if (!waitNotBusy(100)) {
        return false;
    }

    writeEnable();

    select();
    _spi->transfer(CMD_PAGE_PROGRAM);
    _spi->transfer((address >> 16) & 0xFF);
    _spi->transfer((address >> 8) & 0xFF);
    _spi->transfer(address & 0xFF);

    for (size_t i = 0; i < length; ++i) {
        _spi->transfer(buffer[i]);
    }
    deselect();

    return waitNotBusy(50); // Up to 50ms for page program
}

bool HardwareSpiFlashHal::eraseSector4K(uint32_t sectorAddress) {
    if (sectorAddress >= _capacityBytes) {
        return false;
    }

    if (!waitNotBusy(100)) {
        return false;
    }

    writeEnable();

    select();
    _spi->transfer(CMD_SECTOR_ERASE_4K);
    _spi->transfer((sectorAddress >> 16) & 0xFF);
    _spi->transfer((sectorAddress >> 8) & 0xFF);
    _spi->transfer(sectorAddress & 0xFF);
    deselect();

    return waitNotBusy(500); // 4KB sector erase can take up to ~400ms on W25Q
}

uint32_t HardwareSpiFlashHal::readJedecId() {
    select();
    _spi->transfer(CMD_JEDEC_ID);
    uint8_t mfg = _spi->transfer(0x00);
    uint8_t memType = _spi->transfer(0x00);
    uint8_t cap = _spi->transfer(0x00);
    deselect();

    return ((uint32_t)mfg << 16) | ((uint32_t)memType << 8) | (uint32_t)cap;
}

} // namespace plugins
} // namespace ggg

#endif // ARDUINO || !TARGET_NATIVE
