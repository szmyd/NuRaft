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

#include "asio_service_options.hxx"
#include "delayed_task.hxx"
#include "delayed_task_scheduler.hxx"
#include "rpc_cli_factory.hxx"

namespace nuraft {

/**
 * Declaring this to hide the dependency of asio.hpp
 * from root header file, which can boost the compilation time.
 */
class asio_service_impl;
class logger;
class rpc_listener;
class asio_service : public delayed_task_scheduler, public rpc_client_factory {
public:
    using meta_cb_params = asio_service_meta_cb_params;
    using options = asio_service_options;

    asio_service(const options& _opt = options(), std::shared_ptr<logger> _l = nullptr);

    ~asio_service();

    __nocopy__(asio_service);

public:
    void schedule(std::shared_ptr<delayed_task>& task, int32_t milliseconds) override;

    std::shared_ptr<rpc_client> create_client(const std::string& endpoint) override;

    std::shared_ptr<rpc_listener> create_rpc_listener(uint16_t listening_port,
                                                      std::shared_ptr<logger>& l);

    void stop();

    uint32_t get_active_workers();

private:
    void cancel_impl(std::shared_ptr<delayed_task>& task) override;

    asio_service_impl* impl_;

    std::shared_ptr<logger> l_;
};

} // namespace nuraft
