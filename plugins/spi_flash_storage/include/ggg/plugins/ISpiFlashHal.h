/**
 * @file ISpiFlashHal.h
 * @brief Low-level Hardware Abstraction Layer interface for SPI NOR Flash chips.
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

#ifndef GGG_I_SPI_FLASH_HAL_H
#define GGG_I_SPI_FLASH_HAL_H

#include <stdint.h>
#include <stddef.h>

namespace ggg {
namespace plugins {

/**
 * @brief Abstract interface for low-level SPI Flash operations.
 */
class ISpiFlashHal {
public:
    virtual ~ISpiFlashHal() = default;

    /**
     * @brief Initialises the SPI bus and chip select GPIO line.
     */
    virtual bool begin() = 0;

    /**
     * @brief Reads raw bytes from a flash address.
     */
    virtual bool read(uint32_t address, uint8_t* buffer, size_t length) = 0;

    /**
     * @brief Programs a page (up to 256 bytes) at a flash address.
     * Address + length must not cross a 256-byte page boundary.
     */
    virtual bool writePage(uint32_t address, const uint8_t* buffer, size_t length) = 0;

    /**
     * @brief Erases a 4KB sector starting at sectorAddress.
     */
    virtual bool eraseSector4K(uint32_t sectorAddress) = 0;

    /**
     * @brief Reads 3-byte JEDEC manufacturer and device identification.
     */
    virtual uint32_t readJedecId() = 0;

    /**
     * @brief Returns total capacity in bytes.
     */
    virtual size_t getCapacityBytes() const = 0;
};

} // namespace plugins
} // namespace ggg

#endif // GGG_I_SPI_FLASH_HAL_H
