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

#include "buffer.hxx"
#include "buffer_serializer.hxx"
#include "pp_util.hxx"

#include <string>

namespace nuraft {

class srv_config {
public:
    // WARNING: Please see the comment at raft_server::raft_server(...).
    const static int32_t INIT_PRIORITY = 1;

    srv_config(int32_t id, const std::string& endpoint)
        : id_(id)
        , dc_id_(0)
        , endpoint_(endpoint)
        , learner_(false)
        , priority_(INIT_PRIORITY) {}

    srv_config(int32_t id,
               int32_t dc_id,
               const std::string& endpoint,
               const std::string& aux,
               bool learner,
               int32_t priority = INIT_PRIORITY)
        : id_(id)
        , dc_id_(dc_id)
        , endpoint_(endpoint)
        , aux_(aux)
        , learner_(learner)
        , priority_(priority) {}

    __nocopy__(srv_config);

public:
    static std::shared_ptr<srv_config> deserialize(buffer& buf);

    static std::shared_ptr<srv_config> deserialize(buffer_serializer& bs);

    int32_t get_id() const { return id_; }

    int32_t get_dc_id() const { return dc_id_; }

    const std::string& get_endpoint() const { return endpoint_; }

    const std::string& get_aux() const { return aux_; }

    bool is_learner() const { return learner_; }

    int32_t get_priority() const { return priority_; }

    void set_priority(const int32_t new_val) { priority_ = new_val; }

    std::shared_ptr<buffer> serialize() const;

private:
    /**
     * ID of this server, should be positive number.
     */
    int32_t id_;

    /**
     * ID of datacenter where this server is located.
     * 0 if not used.
     */
    int32_t dc_id_;

    /**
     * Endpoint (address + port).
     */
    std::string endpoint_;

    /**
     * Custom string given by user.
     * WARNING: It SHOULD NOT contain NULL character,
     *          as it will be stored as a C-style string.
     */
    std::string aux_;

    /**
     * `true` if this node is learner.
     * Learner will not initiate or participate in leader election.
     */
    bool learner_;

    /**
     * Priority of this node.
     * 0 will never be a leader.
     */
    int32_t priority_;
};

} // namespace nuraft
