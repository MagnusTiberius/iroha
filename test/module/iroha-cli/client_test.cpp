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

#include "module/irohad/ametsuchi/ametsuchi_mocks.hpp"
#include "module/irohad/network/network_mocks.hpp"
#include "module/irohad/validation/validation_mocks.hpp"

#include "client.hpp"

#include "main/server_runner.hpp"
#include "torii/processor/query_processor_impl.hpp"
#include "torii/processor/transaction_processor_impl.hpp"

constexpr const char *Ip = "0.0.0.0";
constexpr int Port = 50051;

using ::testing::Return;
using ::testing::A;
using ::testing::_;
using ::testing::AtLeast;

using namespace iroha::ametsuchi;
using namespace iroha::network;
using namespace iroha::validation;

class ClientTest : public testing::Test {
 public:
  virtual void SetUp() {
    // Run a server
    runner = std::make_unique<ServerRunner>(Ip, Port);
    th = std::thread([this] {
      // ----------- Command Service --------------
      auto tx_processor =
          iroha::torii::TransactionProcessorImpl(pcsMock, svMock);
      iroha::model::converters::PbTransactionFactory pb_tx_factory;
      auto command_service =
          std::make_unique<torii::CommandService>(pb_tx_factory, tx_processor);

      //----------- Query Service ----------
      iroha::model::QueryProcessingFactory qpf(wsv_query, block_query);

      iroha::torii::QueryProcessorImpl qpi(qpf, svMock);

      iroha::model::converters::PbQueryFactory pb_query_factory;
      iroha::model::converters::PbQueryResponseFactory pb_query_resp_factory;

      auto query_service = std::make_unique<torii::QueryService>(
          pb_query_factory, pb_query_resp_factory, qpi);

      //----------- Server run ----------------
      runner->run(std::move(command_service), std::move(query_service));
    });

    runner->waitForServersReady();
  }

  virtual void TearDown() {
    runner->shutdown();
    th.join();
  }

  std::unique_ptr<ServerRunner> runner;
  std::thread th;
  MockPeerCommunicationService pcsMock;
  MockStatelessValidator svMock;
  MockWsvQuery wsv_query;
  MockBlockQuery block_query;
};

TEST_F(ClientTest, SendTxWhenValid) {
  iroha_cli::CliClient client(Ip, Port);
  EXPECT_CALL(svMock, validate(A<const iroha::model::Transaction &>()))
      .WillOnce(Return(true));
  EXPECT_CALL(pcsMock, propagate_transaction(_)).Times(1);

  auto json_tx =
      "{\"signatures\": [ {\n"
      "                    \"pubkey\": "
      "\"2323232323232323232323232323232323232323232323232323232323232323\",\n"
      "                    \"signature\": "
      "\"2323232323232323232323232323232323232323232323232323232323232323232323"
      "2323232323232323232323232323232323232323232323232323232323\"\n"
      "                }], \"created_ts\": 0,\n"
      "            \"creator_account_id\": \"123\",\n"
      "            \"tx_counter\": 0,\n"
      "            \"commands\": [{\n"
      "                    \"command_type\": \"AddPeer\",\n"
      "                    \"address\": \"localhost\",\n"
      "                    \"peer_key\": "
      "\"2323232323232323232323232323232323232323232323232323232323232323\"\n"
      "                }]}";
  std::cout << "Sending  json transaction to Iroha" << std::endl;
  auto status = client.sendTx(json_tx);
  ASSERT_EQ(status, iroha_cli::CliClient::OK);
}

TEST_F(ClientTest, SendTxWhenInvalidJson) {
  iroha_cli::CliClient client(Ip, Port);
  // Must not call stateful validation
  EXPECT_CALL(svMock, validate(A<const iroha::model::Transaction &>()))
      .Times(0);
  // Json with no Transaction
  auto json_tx =
      "{\n"
      "  \"creator_account_id\": \"test\", \n"
      "  \"commands\":[{\n"
      "  \"command_type\": \"AddPeer\",\n"
      "    \"address\": \"localhost\",\n"
      "    \"peer_key\": "
      "\"2323232323232323232323232323232323232323232323232323232323232323\"\n"
      "  }]\n"
      "}";
  std::cout << "Sending  json transaction to Iroha" << std::endl;
  ASSERT_EQ(client.sendTx(json_tx), iroha_cli::CliClient::WRONG_FORMAT);
}

TEST_F(ClientTest, SendTxWhenStatelessInvalid) {
  iroha_cli::CliClient client(Ip, Port);
  EXPECT_CALL(svMock, validate(A<const iroha::model::Transaction &>()))
      .WillOnce(Return(false));
  auto json_tx =
      "{\"signatures\": [ {\n"
      "                    \"pubkey\": "
      "\"2423232323232323232323232323232323232323232323232323232323232323\",\n"
      "                    \"signature\": "
      "\"2323232323232323232323232323232323232323232323232323232323232323232323"
      "2323232323232323232323232323232323232323232323232323232323\"\n"
      "                }], \"created_ts\": 0,\n"
      "            \"creator_account_id\": \"123\",\n"
      "            \"tx_counter\": 0,\n"
      "            \"commands\": [{\n"
      "                    \"command_type\": \"AddPeer\",\n"
      "                    \"address\": \"localhost\",\n"
      "                    \"peer_key\": "
      "\"2323232323232323232323232323232323232323232323232323232323232323\"\n"
      "                }]}";
  std::cout << "Sending  json transaction to Iroha" << std::endl;
  ASSERT_EQ(client.sendTx(json_tx), iroha_cli::CliClient::NOT_VALID);
}