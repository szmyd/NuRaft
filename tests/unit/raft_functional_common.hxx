/************************************************************************
Copyright 2017-2019 eBay Inc.
Author/Developer(s): Jung-Sang Ahn

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

#include "in_memory_log_store.hxx"
#include "logger_wrapper.hxx"

#include "test_common.h"

#include <cassert>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <sstream>

#define INT_UNUSED int
#define VOID_UNUSED void
#define STR_UNUSED std::string
#define _msg(...) TestSuite::_msg(__VA_ARGS__);

using namespace nuraft;

namespace raft_functional_common {

class TestSm : public state_machine {
public:
    TestSm(SimpleLogger* logger = nullptr)
        : customBatchSize(0)
        , lastCommittedConfigIdx(0)
        , targetSnpReadFailures(0)
        , snpDelayMs(0)
        , myLog(logger) {
        (void)myLog;
    }

    ~TestSm() {}

    std::shared_ptr<buffer> commit(const uint64_t log_idx, buffer& data) {
        std::lock_guard<std::mutex> ll(dataLock);
        commits[log_idx] = buffer::copy(data);

        std::shared_ptr<buffer> ret = buffer::alloc(sizeof(uint64_t));
        buffer_serializer bs(ret);
        bs.put_u64(log_idx);
        return ret;
    }

    void commit_config(const uint64_t log_idx,
                       std::shared_ptr<cluster_config>& new_conf) {
        lastCommittedConfigIdx = log_idx;
    }

    std::shared_ptr<buffer> pre_commit(const uint64_t log_idx, buffer& data) {
        std::lock_guard<std::mutex> ll(dataLock);
        preCommits[log_idx] = buffer::copy(data);

        std::shared_ptr<buffer> ret = buffer::alloc(sizeof(uint64_t));
        buffer_serializer bs(ret);
        bs.put_u64(log_idx);
        return ret;
    }

    void rollback(const uint64_t log_idx, buffer& data) {
        std::lock_guard<std::mutex> ll(dataLock);
        rollbacks.push_back(log_idx);
    }

    void save_logical_snp_obj(snapshot& s,
                              uint64_t& obj_id,
                              buffer& data,
                              bool is_first_obj,
                              bool is_last_obj) {
        if (snpDelayMs) {
            TestSuite::sleep_ms(snpDelayMs);
        }

        if (obj_id == 0) {
            // Special object containing metadata.
            // Request next object.
            obj_id++;
            return;
        }

        if (data.size() == sizeof(uint64_t)) {
            // Special object representing config change.
            // Nothing to do for state machine.
            // Request next object.
            obj_id++;
            return;
        }

        buffer_serializer bs(data);
        uint64_t log_idx = bs.get_u64();

        // In this test state machine implementation,
        // obj id should be always the same as log index.
        assert(log_idx == obj_id);

        auto data_size = bs.get_i32();
        std::shared_ptr<buffer> data_commit = buffer::alloc(data_size);
        bs.get_buffer(data_commit);

        commits[log_idx] = data_commit;

        std::shared_ptr<buffer> data_precommit = buffer::copy(*data_commit);
        preCommits[log_idx] = data_precommit;

        // Request next object.
        obj_id++;
    }

    bool apply_snapshot(snapshot& s) {
        std::lock_guard<std::mutex> ll(lastSnapshotLock);
        // NOTE: We only handle logical snapshot.
        std::shared_ptr<buffer> snp_buf = s.serialize();
        lastSnapshot = snapshot::deserialize(*snp_buf);
        return true;
    }

    int read_logical_snp_obj(snapshot& s,
                             void*& user_snp_ctx,
                             uint64_t obj_id,
                             std::shared_ptr<buffer>& data_out,
                             bool& is_last_obj) {
        if (!user_snp_ctx) {
            // Create a dummy context with a magic number.
            int ctx = 0xabcdef;
            user_snp_ctx = malloc(sizeof(ctx));
            memcpy(user_snp_ctx, &ctx, sizeof(ctx));

            std::lock_guard<std::mutex> ll(openedUserCtxsLock);
            openedUserCtxs.insert(user_snp_ctx);
        }

        if (targetSnpReadFailures > 0) {
            targetSnpReadFailures--;
            return -1;
        }

        if (obj_id == 0) {
            // First object contains metadata:
            //   Put first log index and the last log index.
            data_out = buffer::alloc(sizeof(uint64_t) * 2);
            buffer_serializer bs(data_out);

            uint64_t first_idx = 0;
            auto entry = commits.begin();
            if (entry != commits.end()) {
                first_idx = entry->first;
            }

            bs.put_u64(first_idx);
            bs.put_u64(s.get_last_log_idx());

            if (!first_idx) {
                // It means that no data in the state machine.
                is_last_obj = true;
            }
            return 0;
        }

        // Otherwise:
        //   just copy data corresponding to obj id (== log number).
        auto entry = commits.find(obj_id);
        if (entry == commits.end()) {
            // Corresponding log number doesn't exist,
            // it happens when that log number is used for config change.
            data_out = buffer::alloc(sizeof(uint64_t));
            buffer_serializer bs(data_out);
            bs.put_u64(obj_id);
        } else {
            std::shared_ptr<buffer> local_data = entry->second;
            data_out =
                buffer::alloc(sizeof(uint64_t) + sizeof(int32_t) + local_data->size());
            buffer_serializer bs(data_out);
            bs.put_u64(obj_id);
            bs.put_i32((int32_t)local_data->size());
            bs.put_buffer(*local_data);
        }

        if (obj_id == s.get_last_log_idx()) {
            is_last_obj = true;
        }

        return 0;
    }

    void free_user_snp_ctx(void*& user_snp_ctx) {
        if (!user_snp_ctx) return;

        int ctx = 0;
        memcpy(&ctx, user_snp_ctx, sizeof(ctx));
        // Check magic number.
        assert(ctx == 0xabcdef);
        free(user_snp_ctx);

        std::lock_guard<std::mutex> ll(openedUserCtxsLock);
        openedUserCtxs.erase(user_snp_ctx);
    }

    size_t getNumOpenedUserCtxs() const {
        std::lock_guard<std::mutex> ll(openedUserCtxsLock);
        return openedUserCtxs.size();
    }

    std::shared_ptr<snapshot> last_snapshot() {
        std::lock_guard<std::mutex> ll(lastSnapshotLock);
        return lastSnapshot;
    }

    uint64_t last_commit_index() {
        std::lock_guard<std::mutex> ll(dataLock);
        auto entry = commits.rbegin();
        if (entry == commits.rend()) return 0;
        return std::max(entry->first, lastCommittedConfigIdx.load());
    }

    void create_snapshot(snapshot& s, async_result<bool>::handler_type& when_done) {
        {
            std::lock_guard<std::mutex> ll(lastSnapshotLock);
            // NOTE: We only handle logical snapshot.
            std::shared_ptr<buffer> snp_buf = s.serialize();
            lastSnapshot = snapshot::deserialize(*snp_buf);
        }
        std::shared_ptr<std::exception> except(nullptr);
        bool ret = true;
        when_done(ret, except);
    }

    void set_next_batch_size_hint_in_bytes(uint64_t to) { customBatchSize = to; }

    int64_t get_next_batch_size_hint_in_bytes() { return customBatchSize; }

    uint64_t adjust_commit_index(const adjust_commit_index_params& params) {
        std::lock_guard<std::mutex> l(serversForCommitLock);
        if (serversForCommit.empty()) {
            return params.expected_commit_index_;
        }

        uint64_t min_index = std::numeric_limits<uint64_t>::max();
        for (int srv_id: serversForCommit) {
            auto entry = params.peer_index_map_.find(srv_id);
            if (entry == params.peer_index_map_.end()) {
                // Something went wrong.
                assert(0);
                return params.expected_commit_index_;
            }
            min_index = std::min(entry->second, min_index);
        }
        return min_index;
    }

    const std::list<uint64_t>& getRollbackIdxs() const { return rollbacks; }

    bool isSame(const TestSm& with, bool check_precommit = false) {
        // NOTE:
        //   To avoid false alarm by TSAN (regarding lock order inversion),
        //   always grab lock of the smaller address one first.
        if (this < &with) {
            std::lock_guard<std::mutex> ll_mine(dataLock);
            std::lock_guard<std::mutex> ll_with(with.dataLock);
        } else {
            std::lock_guard<std::mutex> ll_with(with.dataLock);
            std::lock_guard<std::mutex> ll_mine(dataLock);
        }

        if (check_precommit) {
            if (preCommits.size() != with.preCommits.size()) return false;
            for (auto& e1: preCommits) {
                auto e2 = with.preCommits.find(e1.first);
                if (e2 == with.preCommits.end()) return false;

                std::shared_ptr<buffer> e1_buf = e1.second;
                std::shared_ptr<buffer> e2_buf = e2->second;
                if (e1_buf->size() != e2_buf->size()) return false;

                e1_buf->pos(0);
                e2_buf->pos(0);
                if (memcmp(e1_buf->data(), e2_buf->data(), e1_buf->size())) return false;
            }
        }

        if (commits.size() != with.commits.size()) return false;
        for (auto& e1: commits) {
            auto e2 = with.commits.find(e1.first);
            if (e2 == with.commits.end()) return false;

            std::shared_ptr<buffer> e1_buf = e1.second;
            std::shared_ptr<buffer> e2_buf = e2->second;
            if (e1_buf->size() != e2_buf->size()) return false;

            e1_buf->pos(0);
            e2_buf->pos(0);
            if (memcmp(e1_buf->data(), e2_buf->data(), e1_buf->size())) return false;
        }

        return true;
    }

    uint64_t isCommitted(const std::string& msg) {
        std::lock_guard<std::mutex> ll(dataLock);
        for (auto& entry: commits) {
            std::shared_ptr<buffer> bb = entry.second;
            bb->pos(0);
            const char* str = bb->get_str();
            bb->pos(0);
            if (std::string(str) == msg) {
                return entry.first;
            }
        }
        return 0;
    }

    std::shared_ptr<buffer> getData(uint64_t log_idx) const {
        std::lock_guard<std::mutex> ll(dataLock);
        auto entry = commits.find(log_idx);
        if (entry == commits.end()) return nullptr;
        return entry->second;
    }

    void truncateData(uint64_t log_idx_upto) {
        auto entry = preCommits.lower_bound(log_idx_upto);
        preCommits.erase(entry, preCommits.end());
        auto entry2 = commits.lower_bound(log_idx_upto);
        commits.erase(entry2, commits.end());

        if (lastCommittedConfigIdx > log_idx_upto) {
            lastCommittedConfigIdx = log_idx_upto;
        }
    }

    void setSnpReadFailure(int num_failures) { targetSnpReadFailures = num_failures; }

    void setSnpDelay(size_t delay_ms) { snpDelayMs = delay_ms; }

    void setServersForCommit(const std::list<int>& src) {
        std::lock_guard<std::mutex> l(serversForCommitLock);
        serversForCommit = src;
    }

private:
    std::map<uint64_t, std::shared_ptr<buffer>> preCommits;
    std::map<uint64_t, std::shared_ptr<buffer>> commits;
    std::list<uint64_t> rollbacks;
    mutable std::mutex dataLock;

    std::shared_ptr<snapshot> lastSnapshot;
    mutable std::mutex lastSnapshotLock;

    std::atomic<uint64_t> customBatchSize;

    std::atomic<uint64_t> lastCommittedConfigIdx;

    std::atomic<int> targetSnpReadFailures;

    std::atomic<size_t> snpDelayMs;

    std::set<void*> openedUserCtxs;
    mutable std::mutex openedUserCtxsLock;

    /**
     * If non-empty, the commit index will be min(log index of servers).
     */
    std::list<int> serversForCommit;
    mutable std::mutex serversForCommitLock;

    SimpleLogger* myLog;
};

class TestMgr : public state_mgr {
public:
    TestMgr(int srv_id, const std::string& endpoint)
        : myId(srv_id)
        , myEndpoint(endpoint)
        , curLogStore(std::make_shared<inmem_log_store>()) {
        mySrvConfig = std::make_shared<srv_config>(
            srv_id, 1, endpoint, "server " + std::to_string(srv_id), false, 50);

        savedConfig = std::make_shared<cluster_config>();
        savedConfig->get_servers().push_back(mySrvConfig);
    }
    ~TestMgr() {}

    std::shared_ptr<cluster_config> load_config() { return savedConfig; }
    void save_config(const cluster_config& config) {
        std::shared_ptr<buffer> buf = config.serialize();
        savedConfig = cluster_config::deserialize(*buf);
    }
    void save_state(const srv_state& state) {
        std::shared_ptr<buffer> buf = state.serialize();
        savedState = srv_state::deserialize(*buf);
    }
    std::shared_ptr<srv_state> read_state() { return savedState; }
    std::shared_ptr<log_store> load_log_store() { return curLogStore; }
    int32_t server_id() override { return myId; }
    void system_exit(const int exit_code) { abort(); }

    std::shared_ptr<srv_config> get_srv_config() const { return mySrvConfig; }

    void set_disk_delay(raft_server* raft, size_t delay_ms) {
        curLogStore->set_disk_delay(raft, delay_ms);
    }

    std::shared_ptr<inmem_log_store> get_inmem_log_store() const { return curLogStore; }

private:
    int32_t myId;
    std::string myEndpoint;
    std::shared_ptr<inmem_log_store> curLogStore;
    std::shared_ptr<srv_config> mySrvConfig;
    std::shared_ptr<cluster_config> savedConfig;
    std::shared_ptr<srv_state> savedState;
};

[[maybe_unused]] static VOID_UNUSED reset_log_files() {
    std::stringstream ss;
    ss << "srv*.log ";

#if defined(__linux__) || defined(__APPLE__)
    std::string cmd = "rm -f base.log " + ss.str() + "2> /dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    int r = pclose(fp);
    (void)r;

#elif defined(WIN32) || defined(_WIN32)
    std::string cmd = "del /s /f /q base.log " + ss.str() + "> NUL";
    int r = system(cmd.c_str());
    (void)r;

#else
#makeerror "not supported platform"
#endif
}

} // namespace raft_functional_common
