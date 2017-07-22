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

#ifndef IROHA_FOLLOWER_HPP
#define IROHA_FOLLOWER_HPP

#include <consensus/model/abort.hpp>
#include <consensus/model/commit.hpp>
#include <consensus/model/proposal.hpp>
#include <consensus/model/vote.hpp>

#include <peer_service/peer_service.hpp>

#include <grpc++/grpc++.h>
#include <common/types.hpp>

namespace consensus {
  namespace role {

    enum Role : uint8_t {
      MEMBER,      // the one who does not participate in validation, but may
                   // receive commits and aborts
      VALIDATOR,   // the one who participates in validation, may receive
                   // proposals and commits
      PROXY_TAIL,  // the one who creates commits, may receive proposals and
                   // votes
      LEADER       // the one who has ordering service
    };

    class Member {
     public:
      Member(peerservice::PeerServiceImpl &ps, std::atomic<bool> &round_started);
      virtual Role self();
      virtual void on_proposal(const model::Proposal &proposal);
      virtual void on_commit(const model::Commit &commit);
      virtual void on_vote(const model::Vote &vote);
      virtual void on_abort(const model::Abort &abort);

     protected:
      peerservice::PeerServiceImpl &ps_;
      std::atomic<bool> &round_started_;
    };
  }
}
#endif  // IROHA_FOLLOWER_HPP
