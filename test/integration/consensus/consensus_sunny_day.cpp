/**
 * Copyright Soramitsu Co., Ltd. 2017 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "module/irohad/consensus/yac/yac_mocks.hpp"

#include <grpc++/grpc++.h>
#include "consensus/yac/impl/network_impl.hpp"
#include "consensus/yac/impl/timer_impl.hpp"
#include "framework/test_subscriber.hpp"

using ::testing::Return;
using ::testing::An;

using namespace iroha::model;
using namespace iroha::consensus::yac;
using namespace framework::test_subscriber;

Peer mk_local_peer(uint64_t num) {
  Peer peer;
  peer.address = "0.0.0.0:" + std::to_string(num);
  return peer;
}

class FixedCryptoProvider : public MockYacCryptoProvider {
 public:
  explicit FixedCryptoProvider(const std::string &public_key) {
    pubkey.fill(0);
    std::copy(public_key.begin(), public_key.end(), pubkey.begin());
  }

  VoteMessage getVote(YacHash hash) override {
    auto vote = MockYacCryptoProvider::getVote(hash);
    vote.signature.pubkey = pubkey;
    return vote;
  }

  decltype(VoteMessage().signature.pubkey) pubkey;
};

class ConsensusSunnyDayTest : public ::testing::Test {
 public:
  std::thread thread, loop_thread;
  std::unique_ptr<grpc::Server> server;
  std::shared_ptr<uvw::Loop> loop;
  std::shared_ptr<NetworkImpl> network;
  std::shared_ptr<MockYacCryptoProvider> crypto;
  std::shared_ptr<TimerImpl> timer;
  uint64_t delay = 3 * 1000;
  std::shared_ptr<Yac> yac;

  void SetUp() override {
    network = std::make_shared<NetworkImpl>(my_peer.address, default_peers);
    crypto = std::make_shared<FixedCryptoProvider>(std::to_string(my_num));
    timer = std::make_shared<TimerImpl>();
    yac = Yac::create(std::move(YacVoteStorage()), network, crypto, timer,
                      ClusterOrdering(default_peers), delay);
    network->subscribe(yac);

    std::mutex mtx;
    std::condition_variable cv;

    thread = std::thread([&cv, this] {
      grpc::ServerBuilder builder;
      int port = 0;
      builder.AddListeningPort(my_peer.address,
                               grpc::InsecureServerCredentials(), &port);
      builder.RegisterService(network.get());
      server = builder.BuildAndStart();
      ASSERT_TRUE(server);
      ASSERT_NE(port, 0);
      cv.notify_one();
      server->Wait();
    });

    // wait until server woke up
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock);

    loop = uvw::Loop::create();
    loop_thread = std::thread([this] { loop->run(); });
  }

  void TearDown() override {
    loop->stop();
    if (loop_thread.joinable()) {
      loop_thread.join();
    }

    server->Shutdown();
    if (thread.joinable()) {
      thread.join();
    }
  }

  static uint64_t my_num, delay_before, delay_after;
  static Peer my_peer;
  static std::vector<Peer> default_peers;

  static void init(uint64_t num_peers, uint64_t num) {
    my_num = num;
    my_peer = mk_local_peer(10000 + my_num);
    for (decltype(num_peers) i = 0; i < num_peers; ++i) {
      default_peers.push_back(mk_local_peer(10000 + i));
    }
    if (num_peers == 1) {
      delay_before = 0;
      delay_after = 50;
    }
    else {
      delay_before = 10 * 1000;
      delay_after = 3 * default_peers.size() + 10 * 1000;
    }
  }
};

uint64_t ConsensusSunnyDayTest::my_num;
uint64_t ConsensusSunnyDayTest::delay_before;
uint64_t ConsensusSunnyDayTest::delay_after;
Peer ConsensusSunnyDayTest::my_peer;
std::vector<Peer> ConsensusSunnyDayTest::default_peers;

TEST_F(ConsensusSunnyDayTest, SunnyDayTest) {
  auto wrapper = make_test_subscriber<CallExact>(yac->on_commit(), 1);
  wrapper.subscribe(
      [](auto hash) { std::cout << "^_^ COMMITTED!!!" << std::endl; });

  EXPECT_CALL(*crypto, verify(An<CommitMessage>()))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*crypto, verify(An<VoteMessage>())).WillRepeatedly(Return(true));

  // Wait for other peers to start
  std::this_thread::sleep_for(std::chrono::milliseconds(delay_before));

  YacHash my_hash("proposal_hash", "block_hash");
  yac->vote(my_hash, ClusterOrdering(default_peers));
  std::this_thread::sleep_for(
      std::chrono::milliseconds(delay_after));

  ASSERT_TRUE(wrapper.validate());
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  uint64_t num_peers = 1, my_num = 0;
  if (argc == 3) {
    num_peers = std::stoul(argv[1]);
    my_num = std::stoul(argv[2]);
  }
  ConsensusSunnyDayTest::init(num_peers, my_num);
  return RUN_ALL_TESTS();
}
