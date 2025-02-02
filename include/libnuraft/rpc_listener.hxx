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

namespace nuraft {

class raft_server;

class rpc_listener {
    __interface_body__(rpc_listener);

public:
    virtual void listen(std::shared_ptr<raft_server>& handler) = 0;
    virtual void stop() = 0;
    virtual void shutdown() {}
};

} // namespace nuraft
