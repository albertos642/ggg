/**
 * @file IStorage.h
 * @brief Hardware Abstraction Layer for non-volatile and RAM block/stream storage.
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

#ifndef GGG_ISTORAGE_H
#define GGG_ISTORAGE_H

#include <stdint.h>
#include <stddef.h>

namespace ggg {
namespace hal {

typedef uint16_t StorageHandle_t;
#define GGG_INVALID_HANDLE 0xFFFF

class IStorage {
public:
    virtual ~IStorage() = default;

    // -- Stream-to-Storage Write Flow --
    
    // Requests opening a new record. Returns a temporary unique handle.
    virtual StorageHandle_t beginWrite() = 0;
    
    // Appends data to an opened record.
    virtual size_t writeData(StorageHandle_t handle, const uint8_t* data, size_t len) = 0;
    
    // Commits the record, making it durable and accessible for reading.
    virtual bool commitWrite(StorageHandle_t handle) = 0;
    
    // Aborts write operation, invalidates handle and releases allocated resources.
    virtual void abortWrite(StorageHandle_t handle) = 0;

    // -- Read Flow --
    
    // Reads data from a committed record at specified offset into destination buffer.
    virtual size_t readData(StorageHandle_t handle, size_t offset, uint8_t* dest, size_t len) = 0;
    
    // Returns total byte size of a committed record.
    virtual size_t getSize(StorageHandle_t handle) = 0;
    
    // Permanently deletes record from storage.
    virtual bool deleteRecord(StorageHandle_t handle) = 0;

    // Optional diagnostic: Returns count of committed active records.
    virtual size_t getCommittedCount() const { return 0; }
};

} // namespace hal
} // namespace ggg

#endif // GGG_ISTORAGE_H
