/*
 * Copyright 2018 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Firestore/core/src/firebase/firestore/remote/grpc_stream.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Firestore/core/src/firebase/firestore/remote/grpc_operation.h"
#include "Firestore/core/src/firebase/firestore/util/async_queue.h"
#include "Firestore/core/src/firebase/firestore/util/executor_std.h"
#include "Firestore/core/test/firebase/firestore/util/grpc_tests_util.h"
#include "absl/memory/memory.h"
#include "grpcpp/client_context.h"
#include "grpcpp/completion_queue.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/generic/generic_stub.h"
#include "grpcpp/support/byte_buffer.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace remote {

using util::AsyncQueue;
using util::GrpcStreamFixture;
using util::OperationResult;
using util::OperationResult::Error;
using util::OperationResult::Ok;
using util::internal::ExecutorStd;

namespace {

class Observer : public GrpcStreamObserver {
 public:
  void OnStreamStart() override {
    observed_states.push_back("OnStreamStart");
  }
  void OnStreamRead(const grpc::ByteBuffer& message) override {
    observed_states.push_back("OnStreamRead");
  }
  void OnStreamError(const util::Status& status) override {
    observed_states.push_back("OnStreamError");
  }

  int generation() const override {
    return gen;
  }

  std::vector<std::string> observed_states;
  int gen = 0;
};

}  // namespace

class GrpcStreamTest : public testing::Test {
 public:
  GrpcStreamTest() : observer{absl::make_unique<Observer>()} {
    fixture.CreateStream(observer.get());
  }

  ~GrpcStreamTest() {
    fixture.Shutdown();
  }

  GrpcStream& stream() {
    return fixture.stream();
  }
  AsyncQueue& async_queue() {
    return fixture.async_queue();
  }

  void ForceFinish(std::initializer_list<OperationResult> results) {
    fixture.ForceFinish(results);
  }
  void KeepPollingGrpcQueue() {
    fixture.KeepPollingGrpcQueue();
  }

  const std::vector<std::string>& observed_states() const {
    return observer->observed_states;
  }

  // This is to make `EXPECT_EQ` a little shorter and work around macro
  // limitations related to initializer lists.
  std::vector<std::string> States(std::initializer_list<std::string> states) {
    return {states};
  }

  bool ObserverHas(const std::string& state) const {
    return std::find(observed_states().begin(), observed_states().end(),
                     state) != observed_states().end();
  }

  void RaiseGeneration() {
    ++observer->gen;
  }

  void StartStream() {
    async_queue().EnqueueBlocking([&] { stream().Start(); });
    ForceFinish({/*Start*/ Ok});
  }

 private:
  GrpcStreamFixture fixture;
  std::unique_ptr<Observer> observer;
};

// State tests

TEST_F(GrpcStreamTest, CannotStartTwice) {
  async_queue().EnqueueBlocking([&] {
    EXPECT_NO_THROW(stream().Start());
    EXPECT_ANY_THROW(stream().Start());
  });
}

TEST_F(GrpcStreamTest, CannotWriteBeforeStreamIsOpen) {
  async_queue().EnqueueBlocking([&] {
    EXPECT_ANY_THROW(stream().Write({}));
    stream().Start();
    EXPECT_ANY_THROW(stream().Write({}));
  });
}

TEST_F(GrpcStreamTest, CanFinishBeforeStarting) {
  async_queue().EnqueueBlocking([&] { EXPECT_NO_THROW(stream().Finish()); });
}

TEST_F(GrpcStreamTest, CanFinishAfterStarting) {
  StartStream();
  KeepPollingGrpcQueue();

  async_queue().EnqueueBlocking([&] { EXPECT_NO_THROW(stream().Finish()); });
}

TEST_F(GrpcStreamTest, CannotFinishTwice) {
  async_queue().EnqueueBlocking([&] {
    EXPECT_NO_THROW(stream().Finish());
    EXPECT_ANY_THROW(stream().Finish());
  });
}

TEST_F(GrpcStreamTest, CannotWriteAndFinishBeforeStarting) {
  async_queue().EnqueueBlocking(
      [&] { EXPECT_ANY_THROW(stream().WriteAndFinish({})); });
}

TEST_F(GrpcStreamTest, CanWriteAndFinishAfterStarting) {
  StartStream();
  KeepPollingGrpcQueue();

  async_queue().EnqueueBlocking(
      [&] { EXPECT_NO_THROW(stream().WriteAndFinish({})); });
}

TEST_F(GrpcStreamTest, ObserverReceivesOnStart) {
  StartStream();
  EXPECT_EQ(observed_states(), States({"OnStreamStart"}));
}

TEST_F(GrpcStreamTest, CanWriteAfterStreamIsOpen) {
  StartStream();
  async_queue().EnqueueBlocking([&] { EXPECT_NO_THROW(stream().Write({})); });
}

// Operations tests
TEST_F(GrpcStreamTest, ObserverReceivesOnRead) {
  StartStream();
  ForceFinish({/*Read*/ Ok});
  EXPECT_EQ(observed_states(), States({"OnStreamStart", "OnStreamRead"}));
}

TEST_F(GrpcStreamTest, ReadIsAutomaticallyReadded) {
  StartStream();
  ForceFinish({/*Read*/ Ok});
  EXPECT_EQ(observed_states(), States({"OnStreamStart", "OnStreamRead"}));

  ForceFinish({/*Read*/ Ok});
  EXPECT_EQ(observed_states(),
            States({"OnStreamStart", "OnStreamRead", "OnStreamRead"}));
}

TEST_F(GrpcStreamTest, CanAddSeveralWrites) {
  StartStream();

  async_queue().EnqueueBlocking([&] {
    stream().Write({});
    stream().Write({});
    stream().Write({});
  });
  ForceFinish({/*Read*/ Ok, /*Write*/ Ok, /*Read*/ Ok, /*Write*/ Ok,
               /*Read*/ Ok, /*Write*/ Ok});

  EXPECT_EQ(observed_states(), States({"OnStreamStart", "OnStreamRead",
                                       "OnStreamRead", "OnStreamRead"}));
}

TEST_F(GrpcStreamTest, ObserverReceivesOnError) {
  StartStream();

  // Fail the read, but allow the rest to succeed.
  ForceFinish({/*Read*/ Error});
  KeepPollingGrpcQueue();
  async_queue().EnqueueBlocking([] {});

  EXPECT_EQ(observed_states(), States({"OnStreamStart", "OnStreamError"}));
}

TEST_F(GrpcStreamTest, ObserverDoesNotReceiveOnFinishIfCalledByClient) {
  StartStream();
  KeepPollingGrpcQueue();

  async_queue().EnqueueBlocking([&] { stream().Finish(); });
  EXPECT_FALSE(ObserverHas("OnStreamError"));
}

TEST_F(GrpcStreamTest, WriteAndFinish) {
  StartStream();
  KeepPollingGrpcQueue();

  async_queue().EnqueueBlocking([&] {
    bool did_last_write = stream().WriteAndFinish({});
    EXPECT_TRUE(did_last_write);

    EXPECT_TRUE(ObserverHas("OnStreamStart"));
    EXPECT_FALSE(ObserverHas("OnStreamError"));
  });
}

TEST_F(GrpcStreamTest, ErrorOnStart) {
  async_queue().EnqueueBlocking([&] { stream().Start(); });
  ForceFinish({/*Start*/ Error, /*Finish*/ Ok});
  EXPECT_EQ(observed_states(), States({"OnStreamError"}));
}

TEST_F(GrpcStreamTest, ErrorOnWrite) {
  StartStream();
  async_queue().EnqueueBlocking([&] { stream().Write({}); });

  ForceFinish({/*Read*/ Ok, /*Write*/ Error});
  KeepPollingGrpcQueue();
  async_queue().EnqueueBlocking([] {});

  EXPECT_EQ(observed_states().back(), "OnStreamError");
}

TEST_F(GrpcStreamTest, ErrorWithPendingWrites) {
  StartStream();
  async_queue().EnqueueBlocking([&] {
    stream().Write({});
    stream().Write({});
  });

  ForceFinish({/*Read*/ Ok, /*Write*/ Error});
  KeepPollingGrpcQueue();
  async_queue().EnqueueBlocking([] {});

  EXPECT_EQ(observed_states().back(), "OnStreamError");
}

TEST_F(GrpcStreamTest, RaisingGenerationStopsNotifications) {
  StartStream();

  ForceFinish({/*Read*/ Ok});

  RaiseGeneration();
  async_queue().EnqueueBlocking([&] { stream().Write({}); });
  ForceFinish({/*Read*/ Ok});
  ForceFinish({/*Read*/ Ok});
  EXPECT_EQ(observed_states(), States({"OnStreamStart", "OnStreamRead"}));
}

}  // namespace remote
}  // namespace firestore
}  // namespace firebase