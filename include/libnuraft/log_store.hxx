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

//#include "async.hxx"
//#include "basic_types.hxx"
#include "buffer.hxx"
#include "log_entry.hxx"
#include "pp_util.hxx"

#include <vector>

namespace nuraft {

class log_store {
    __interface_body__(log_store);

public:
    /**
     * The first available slot of the store, starts with 1
     *
     * @return Last log index number + 1
     */
    virtual uint64_t next_slot() const = 0;

    /**
     * The start index of the log store, at the very beginning, it must be 1.
     * However, after some compact actions, this could be anything equal to or
     * greater than or equal to one
     */
    virtual uint64_t start_index() const = 0;

    /**
     * The last log entry in store.
     *
     * @return If no log entry exists: a dummy constant entry with
     *         value set to null and term set to zero.
     */
    virtual std::shared_ptr<log_entry> last_entry() const = 0;

    /**
     * Append a log entry to store.
     *
     * @param entry Log entry
     * @return Log index number.
     */
    virtual uint64_t append(std::shared_ptr<log_entry>& entry) = 0;

    /**
     * Overwrite a log entry at the given `index`.
     * This API should make sure that all log entries
     * after the given `index` should be truncated (if exist),
     * as a result of this function call.
     *
     * @param index Log index number to overwrite.
     * @param entry New log entry to overwrite.
     */
    virtual void write_at(uint64_t index, std::shared_ptr<log_entry>& entry) = 0;

    /**
     * Invoked after a batch of logs is written as a part of
     * a single append_entries request.
     *
     * @param start The start log index number (inclusive)
     * @param cnt The number of log entries written.
     */
    virtual void end_of_append_batch([[maybe_unused]] uint64_t start,
                                     [[maybe_unused]] uint64_t cnt) {}

    /**
     * Get log entries with index [start, end).
     *
     * Return nullptr to indicate error if any log entry within the requested range
     * could not be retrieved (e.g. due to external log truncation).
     *
     * @param start The start log index number (inclusive).
     * @param end The end log index number (exclusive).
     * @return The log entries between [start, end).
     */
    virtual std::shared_ptr<std::vector<std::shared_ptr<log_entry>>>
    log_entries(uint64_t start, uint64_t end) = 0;

    /**
     * (Optional)
     * Get log entries with index [start, end).
     *
     * The total size of the returned entries is limited by batch_size_hint.
     *
     * Return nullptr to indicate error if any log entry within the requested range
     * could not be retrieved (e.g. due to external log truncation).
     *
     * @param start The start log index number (inclusive).
     * @param end The end log index number (exclusive).
     * @param batch_size_hint_in_bytes Total size (in bytes) of the returned entries,
     *        see the detailed comment at
     *        `state_machine::get_next_batch_size_hint_in_bytes()`.
     * @return The log entries between [start, end) and limited by the total size
     *         given by the batch_size_hint_in_bytes.
     */
    virtual std::shared_ptr<std::vector<std::shared_ptr<log_entry>>>
    log_entries_ext(uint64_t start,
                    uint64_t end,
                    [[maybe_unused]] int64_t batch_size_hint_in_bytes = 0) {
        return log_entries(start, end);
    }

    /**
     * Get the log entry at the specified log index number.
     *
     * @param index Should be equal to or greater than 1.
     * @return The log entry or null if index >= this->next_slot().
     */
    virtual std::shared_ptr<log_entry> entry_at(uint64_t index) = 0;

    /**
     * Get the term for the log entry at the specified index.
     * Suggest to stop the system if the index >= this->next_slot()
     *
     * @param index Should be equal to or greater than 1.
     * @return The term for the specified log entry, or
     *         0 if index < this->start_index().
     */
    virtual uint64_t term_at(uint64_t index) = 0;

    /**
     * Pack the given number of log items starting from the given index.
     *
     * @param index The start log index number (inclusive).
     * @param cnt The number of logs to pack.
     * @return Packed (encoded) logs.
     */
    virtual std::shared_ptr<buffer> pack(uint64_t index, int32_t cnt) = 0;

    /**
     * Apply the log pack to current log store, starting from index.
     *
     * @param index The start log index number (inclusive).
     * @param Packed logs.
     */
    virtual void apply_pack(uint64_t index, buffer& pack) = 0;

    /**
     * Compact the log store by purging all log entries,
     * including the given log index number.
     *
     * If current maximum log index is smaller than given `last_log_index`,
     * set start log index to `last_log_index + 1`.
     *
     * @param last_log_index Log index number that will be purged up to (inclusive).
     * @return `true` on success.
     */
    virtual bool compact(uint64_t last_log_index) = 0;

    /**
     * Compact the log store by purging all log entries,
     * including the given log index number.
     *
     * Unlike `compact`, this API allows to execute the log compaction in background
     * asynchronously, aiming at reducing the client-facing latency caused by the
     * log compaction.
     *
     * This function call may return immediately, but after this function
     * call, following `start_index` should return `last_log_index + 1` even
     * though the log compaction is still in progress. In the meantime, the
     * actual job incurring disk IO can run in background. Once the job is done,
     * `when_done` should be invoked.
     *
     * @param last_log_index Log index number that will be purged up to (inclusive).
     * @param when_done Callback function that will be called after
     *                  the log compaction is done.
     */
    virtual void compact_async(ulong last_log_index,
                               const async_result<bool>::handler_type& when_done) {
        bool rc = compact(last_log_index);
        ptr<std::exception> exp(nullptr);
        when_done(rc, exp);
    }

    /**
     * Synchronously flush all log entries in this log store to the backing storage
     * so that all log entries are guaranteed to be durable upon process crash.
     *
     * @return `true` on success.
     */
    virtual bool flush() = 0;

    /**
     * (Experimental)
     * This API is used only when `raft_params::parallel_log_appending_` flag is set.
     * Please refer to the comment of the flag.
     *
     * @return The last durable log index.
     */
    virtual uint64_t last_durable_index() { return next_slot() - 1; }
};

} // namespace nuraft
