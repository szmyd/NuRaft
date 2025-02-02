/************************************************************************
Modifications Copyright 2017-2019 eBay Inc.
Author/Developer(s): Jung-Sang Ahn

Original Copyright:
See URL: https://github.com/datatechnology/cornerstone

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#pragma once

#include "pp_util.hxx"

#include <cstddef>
#include <memory>
#include <string>

namespace nuraft {

class buffer {
    buffer() = delete;
    __nocopy__(buffer);

public:
    /**
     * Allocate a new memory buffer with given size.
     *
     * @param size Size of memory to allocate.
     * @return buffer instance.
     */
    static std::shared_ptr<buffer> alloc(const size_t size);

    /**
     * Copy the data in the given buffer starting from the current position.
     * It will allocate a new memory buffer.
     *
     * WARNING: It will move the position of the given `buf`.
     *
     * @param buf Buffer to copy.
     * @return buffer instance.
     */
    static std::shared_ptr<buffer> copy(const buffer& buf);

    /**
     * Clone the given buffer, starting from the beginning.
     * It will allocate a new memory buffer.
     *
     * WARNING: It WILL NOT move the position of given `buf`.
     *
     * @param buf Buffer to copy.
     * @return buffer instance.
     */
    static std::shared_ptr<buffer> clone(const buffer& buf);

    /**
     * Expand the current buffer to new size (which is expected to
     * bigger than current size) and return a new buffer. It will copy
     * the existing buffer.
     *
     * WARNING: It will still retain the current position of the passed
     * buffer and the new ptr buffer (i.e. ret_buf->pos() == buf->pos())
     *
     * @param buf Pointer to buffer which will be realloced
     * @param new_size New size of the buffer
     * @return Expanded buffer instance
     */
    static std::shared_ptr<buffer> expand(const buffer& buf, uint32_t new_size);

    /**
     * Get total size of entire buffer container, including meta section.
     *
     * @return Size of container.
     */
    size_t container_size() const;

    /**
     * Get the size that this buffer can contain (i.e., user data size).
     *
     * @return Size of buffer capacity.
     */
    size_t size() const;

    /**
     * Get the current position of cursor inside buffer.
     *
     * @return Position of cursor.
     */
    size_t pos() const;

    /**
     * Set the position of cursor.
     *
     * @param p New position.
     */
    void pos(size_t p);

    /**
     * Get the memory pointer to the current position.
     *
     * @return Pointer to the current position.
     */
    std::byte* data() const;

    /**
     * Get the memory pointer to the beginning of the data,
     * regardless of the current position.
     *
     * @return Pointer to the begining of the data.
     */
    std::byte* data_begin() const;

    /*********************************************************
     *
     * NOTE:
     *   We don't recommend using below get/put APIs, as they
     *   modify the position field embedded in `buffer` instance itself,
     *   which prevents concurrent reads to the same `buffer`.
     *   It is also mistake-prone if someone moves the position
     *   and forgets to reset it, since moved position remains with the
     *   buffer.
     *
     *   Instead, you can use `buffer_serializer`.
     *
     *********************************************************/

    /**
     * Get 4-byte signed integer.
     *
     * @return 4-byte singed integer.
     */
    int32_t get_int();

    /**
     * Get 8-byte unsigned integer.
     *
     * @return 8-byte unsigned integer.
     */
    uint64_t get_uint64();

    /**
     * Get 1-byte unsigned integer.
     *
     * @return 1-byte unsigned integer.
     */
    std::byte get_byte();

    /**
     * Read 4-byte length followed by byte array, and then return them.
     * It will NOT allocate a new memory, but return the
     * reference to the memory inside buffer only.
     *
     * @param[out] len Size of returned byte array.
     * @return Pointer to the starting point of byte array.
     */
    const std::byte* get_bytes(size_t& len);

    /**
     * Read byte array of given buffer's size,
     * and copy it to the given buffer.
     *
     * @param dst Buffer where the data will be stored.
     */
    void get(std::shared_ptr<buffer>& dst);

    /**
     * Read C-style string (null terminated).
     * It will NOT allocate a new memory, but return the
     * reference to the memory inside buffer only.
     *
     * @return C-style string.
     */
    const char* get_str();

    /**
     * Read byte array of given size.
     * It will NOT allocate a new memory, but return the
     * reference to the memory inside buffer only.
     *
     * @param len Size to read.
     * @return Pointer to the starting point of byte array.
     */
    std::byte* get_raw(size_t len);

    /**
     * Put an 1-byte unsigned integer.
     *
     * @param b 1-byte unsigned integer.
     */
    void put(std::byte b);

    /**
     * Put a byte array.
     * This function will put 4-byte length first, and then
     * the actual byte array next.
     *
     * @param ba Pointer to the byte array.
     * @param len Length of the byte array.
     */
    void put(const char* ba, size_t len);
    void put(const std::byte* ba, size_t len);

    /**
     * Put a 4-byte signed integer.
     *
     * @param val 4-byte signed integer.
     */
    void put(int32_t val);

    /**
     * Put an 8-byte unsigned integer.
     *
     * @param val 8-byte unsigned integer.
     */
    void put(uint64_t val);

    /**
     * Put a C-style string.
     *
     * WARNING:
     *   Given string SHOULD NOT contain any NULL character
     *   in the middle.
     *
     *   If it contains, please use `put(const byte*, size_t)`.
     *
     * @param str String.
     */
    void put(const std::string& str);

    /**
     * Put given buffer.
     *
     * WARNING:
     *   It does not put length info of given buffer,
     *   so caller should know the length of buffer in advance
     *   before calling `get(std::shared_ptr<buffer>)`.
     *
     *   If not, please use `put(const byte*, size_t)`.
     *
     * @param buf Buffer.
     */
    void put(const buffer& buf);

    /**
     * Put a byte array.
     *
     * WARNING:
     *   It does not put the given length info,
     *   so caller should know the length of byte array in advance
     *   before calling `get_raw(size_t)`.
     *
     *   If not, please use `put(const byte*, size_t)`.
     *
     * @param ba Pointer to the byte array.
     * @param len Length of the byte array.
     */
    void put_raw(const std::byte* ba, size_t len);
};
using buffer_ptr = std::shared_ptr<buffer>;

std::ostream& operator<<(std::ostream& out, buffer& buf);
std::istream& operator>>(std::istream& in, buffer& buf);

} // namespace nuraft
