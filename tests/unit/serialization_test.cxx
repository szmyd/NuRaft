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

#include "handle_custom_notification.hxx"
#include "nuraft.hxx"
#include "strfmt.hxx"

#include "test_common.h"

#include <random>

using namespace nuraft;

namespace serialization_test {

using std::byte;

int32_t rnd() {
    static uint seed = (uint)std::chrono::system_clock::now().time_since_epoch().count();
    static std::default_random_engine engine(seed);
    static std::uniform_int_distribution<int32_t> distribution(1, 10000);
    static std::function<int32_t()> rnd_func = std::bind(distribution, engine);
    return rnd_func();
}

uint64_t long_val(int val) {
    uint64_t base = std::numeric_limits<uint>::max();
    return base + (uint64_t)val;
}

int srv_config_test() {
    std::shared_ptr<srv_config> srv_conf(
        std::make_shared<srv_config>(rnd(), sstrfmt("server %d").fmt(rnd())));
    std::shared_ptr<buffer> srv_conf_buf(srv_conf->serialize());

    std::shared_ptr<srv_config> srv_conf1(srv_config::deserialize(*srv_conf_buf));

    CHK_EQ(srv_conf->get_endpoint(), srv_conf1->get_endpoint());
    CHK_EQ(srv_conf->get_id(), srv_conf1->get_id());

    return 0;
}

std::shared_ptr<cluster_config> generate_random_config() {
    std::shared_ptr<cluster_config> conf(
        std::make_shared<cluster_config>(long_val(rnd()), long_val(rnd())));
    conf->get_servers().push_back(std::make_shared<srv_config>(rnd(), "server 1"));
    conf->get_servers().push_back(std::make_shared<srv_config>(rnd(), "server 2"));
    conf->get_servers().push_back(std::make_shared<srv_config>(rnd(), "server 3"));
    conf->get_servers().push_back(std::make_shared<srv_config>(rnd(), "server 4"));
    conf->get_servers().push_back(std::make_shared<srv_config>(rnd(), "server 5"));
    return conf;
}

int cluster_config_test() {
    std::shared_ptr<cluster_config> conf = generate_random_config();
    std::shared_ptr<buffer> conf_buf(conf->serialize());

    std::shared_ptr<cluster_config> conf1(cluster_config::deserialize(*conf_buf));

    CHK_EQ(conf->get_log_idx(), conf1->get_log_idx());
    CHK_EQ(conf->get_prev_log_idx(), conf1->get_prev_log_idx());
    CHK_EQ(conf->get_servers().size(), conf1->get_servers().size());
    for (auto it = conf->get_servers().begin(), it1 = conf1->get_servers().begin();
         it != conf->get_servers().end() && it1 != conf1->get_servers().end();
         ++it, ++it1) {
        CHK_EQ((*it)->get_id(), (*it1)->get_id());
        CHK_EQ((*it)->get_endpoint(), (*it1)->get_endpoint());
    }

    return 0;
}

std::shared_ptr<snapshot> generate_random_snapshot() {
    std::shared_ptr<snapshot> snp(std::make_shared<snapshot>(
        long_val(rnd()), long_val(rnd()), generate_random_config(), long_val(rnd())));
    return snp;
}

int snapshot_test() {
    std::shared_ptr<snapshot> snp = generate_random_snapshot();
    std::shared_ptr<buffer> snp_buf(snp->serialize());

    std::shared_ptr<snapshot> snp1(snapshot::deserialize(*snp_buf));
    CHK_EQ(snp->get_last_log_idx(), snp1->get_last_log_idx());
    CHK_EQ(snp->get_last_log_term(), snp1->get_last_log_term());
    CHK_EQ(snp->get_last_config()->get_servers().size(),
           snp1->get_last_config()->get_servers().size());
    CHK_EQ(snp->get_last_config()->get_log_idx(), snp1->get_last_config()->get_log_idx());
    CHK_EQ(snp->get_last_config()->get_prev_log_idx(),
           snp1->get_last_config()->get_prev_log_idx());
    for (auto it = snp->get_last_config()->get_servers().begin(),
              it1 = snp1->get_last_config()->get_servers().begin();
         it != snp->get_last_config()->get_servers().end()
         && it1 != snp1->get_last_config()->get_servers().end();
         ++it, ++it1) {
        CHK_EQ((*it)->get_id(), (*it1)->get_id());
        CHK_EQ((*it)->get_endpoint(), (*it1)->get_endpoint());
    }
    return 0;
}

int snapshot_sync_req_test(bool done) {
    std::shared_ptr<buffer> rnd_buf(buffer::alloc(rnd()));
    for (size_t i = 0; i < rnd_buf->size(); ++i) {
        rnd_buf->put((byte)(rnd()));
    }
    rnd_buf->pos(0);

    std::shared_ptr<snapshot> snp = generate_random_snapshot();
    std::shared_ptr<snapshot_sync_req> sync_req(
        std::make_shared<snapshot_sync_req>(snp, long_val(rnd()), rnd_buf, done));
    std::shared_ptr<buffer> sync_req_buf(sync_req->serialize());

    std::shared_ptr<snapshot_sync_req> sync_req1(
        snapshot_sync_req::deserialize(*sync_req_buf));
    CHK_EQ(sync_req->get_offset(), sync_req1->get_offset());
    CHK_EQ(done, sync_req1->is_done());

    snapshot& snp2(sync_req1->get_snapshot());
    CHK_EQ(snp->get_last_log_idx(), snp2.get_last_log_idx());
    CHK_EQ(snp->get_last_log_term(), snp2.get_last_log_term());
    CHK_EQ(snp->get_last_config()->get_servers().size(),
           snp2.get_last_config()->get_servers().size());
    CHK_EQ(snp->get_last_config()->get_log_idx(), snp2.get_last_config()->get_log_idx());
    CHK_EQ(snp->get_last_config()->get_prev_log_idx(),
           snp2.get_last_config()->get_prev_log_idx());

    buffer& buf1 = sync_req1->get_data();
    CHK_Z(buf1.pos());
    CHK_EQ(rnd_buf->size(), buf1.size());

    for (size_t i = 0; i < buf1.size(); ++i) {
        auto const d = std::to_integer<uint8_t>(*(rnd_buf->data() + 1));
        auto const d1 = std::to_integer<uint8_t>(*(buf1.data() + 1));
        CHK_EQ(d, d1);
    }

    return 0;
}

int snapshot_sync_req_zero_buffer_test(bool done) {
    std::shared_ptr<snapshot> snp = generate_random_snapshot();
    std::shared_ptr<snapshot_sync_req> sync_req(std::make_shared<snapshot_sync_req>(
        snp, long_val(rnd()), buffer::alloc(0), done));
    std::shared_ptr<buffer> sync_req_buf(sync_req->serialize());

    std::shared_ptr<snapshot_sync_req> sync_req1(
        snapshot_sync_req::deserialize(*sync_req_buf));
    CHK_EQ(sync_req->get_offset(), sync_req1->get_offset());
    CHK_EQ(done, sync_req1->is_done());
    CHK_Z(sync_req1->get_data().size());

    snapshot& snp3(sync_req1->get_snapshot());
    CHK_EQ(snp->get_last_log_idx(), snp3.get_last_log_idx());
    CHK_EQ(snp->get_last_log_term(), snp3.get_last_log_term());
    CHK_EQ(snp->get_last_config()->get_servers().size(),
           snp3.get_last_config()->get_servers().size());
    CHK_EQ(snp->get_last_config()->get_log_idx(), snp3.get_last_config()->get_log_idx());
    CHK_EQ(snp->get_last_config()->get_prev_log_idx(),
           snp3.get_last_config()->get_prev_log_idx());

    return 0;
}

int log_entry_test() {
    std::shared_ptr<buffer> data = buffer::alloc(24 + rnd() % 100);
    for (size_t i = 0; i < data->size(); ++i) {
        data->put(static_cast<byte>(rnd() % 255));
    }

    std::shared_ptr<log_entry> entry = std::make_shared<log_entry>(
        long_val(rnd()), data, static_cast<log_val_type>(1 + rnd() % 5));
    std::shared_ptr<buffer> buf2 = entry->serialize();

    std::shared_ptr<log_entry> entry1 = log_entry::deserialize(*buf2);

    CHK_EQ(entry->get_term(), entry1->get_term());
    CHK_EQ(static_cast<uint8_t>(entry->get_val_type()),
           static_cast<uint8_t>(entry1->get_val_type()));
    CHK_EQ(entry->get_buf().size(), entry1->get_buf().size());
    for (size_t i = 0; i < entry->get_buf().size(); ++i) {
        auto const b1 = std::to_integer<uint8_t>(entry->get_buf().get_byte());
        auto const b2 = std::to_integer<uint8_t>(entry1->get_buf().get_byte());
        CHK_EQ(b1, b2);
    }
    return 0;
}

int custom_notification_msg_test(bool empty_context) {
    custom_notification_msg orig_msg;
    orig_msg.type_ = custom_notification_msg::out_of_log_range_warning;

    const std::string MSG_STR = "test_message";
    if (empty_context) {
        orig_msg.ctx_ = nullptr;
    } else {
        std::shared_ptr<buffer> ctx = buffer::alloc(sizeof(uint32_t) + MSG_STR.size());
        buffer_serializer bs(ctx);
        bs.put_str(MSG_STR);
        orig_msg.ctx_ = ctx;
    }

    std::shared_ptr<buffer> enc_msg = orig_msg.serialize();
    std::shared_ptr<custom_notification_msg> dec_msg =
        custom_notification_msg::deserialize(*enc_msg);

    CHK_EQ(orig_msg.type_, dec_msg->type_);
    if (empty_context) {
        CHK_NULL(dec_msg->ctx_.get());
    } else {
        buffer_serializer bs(dec_msg->ctx_);
        std::string result_str = bs.get_str();
        CHK_EQ(MSG_STR, result_str);
    }

    return 0;
}

int out_of_log_msg_test() {
    out_of_log_msg orig_msg;
    orig_msg.start_idx_of_leader_ = 1234;

    std::shared_ptr<buffer> enc_msg = orig_msg.serialize();
    std::shared_ptr<out_of_log_msg> dec_msg = out_of_log_msg::deserialize(*enc_msg);

    CHK_EQ(orig_msg.start_idx_of_leader_, dec_msg->start_idx_of_leader_);
    return 0;
}

} // namespace serialization_test
using namespace serialization_test;

int main(int argc, char** argv) {
    TestSuite ts(argc, argv);

    ts.options.printTestMessage = false;

    ts.doTest("srv_config test", srv_config_test);
    ts.doTest("cluster_config test", cluster_config_test);
    ts.doTest("snapshot test", snapshot_test);
    ts.doTest(
        "snapshot_sync_req test", snapshot_sync_req_test, TestRange<bool>({true, false}));
    ts.doTest("snapshot_sync_req zero buffer test",
              snapshot_sync_req_zero_buffer_test,
              TestRange<bool>({true, false}));
    ts.doTest("log_entry test", log_entry_test);
    ts.doTest("custom_notification_msg test",
              custom_notification_msg_test,
              TestRange<bool>({true, false}));
    ts.doTest("out_of_log_msg test", out_of_log_msg_test);

    return 0;
}
