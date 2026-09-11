/**
 * @file IStream.h
 * @brief Hardware Abstraction Layer for sequential input and output streams.
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

#ifndef GGG_ISTREAM_H
#define GGG_ISTREAM_H

#include <stdint.h>
#include <stddef.h>

namespace ggg {
namespace hal {

class IOutputStream {
public:
    virtual ~IOutputStream() = default;
    
    // Writes a single byte. Returns 1 on success, 0 if TX buffer is full.
    virtual size_t write(uint8_t b) = 0;
    
    // Writes a buffer. Returns the number of bytes actually written.
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
    
    // Flushes hardware TX buffer.
    virtual void flush() = 0;
};

class IInputStream {
public:
    virtual ~IInputStream() = default;
    
    // Returns the number of bytes available for reading in RX.
    virtual size_t available() = 0;
    
    // Reads a single byte. Returns -1 if empty, or byte value (0-255).
    virtual int read() = 0;
    
    // Flushes RX buffer discarding pending data.
    virtual void flushRX() = 0;
};

} // namespace hal
} // namespace ggg

#endif // GGG_ISTREAM_H
