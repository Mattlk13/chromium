// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/connection.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/cryptauth/connection_observer.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/wire_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace cryptauth {
namespace {

class MockConnection : public Connection {
 public:
  MockConnection() : Connection(cryptauth::RemoteDevice()) {}
  ~MockConnection() {}

  MOCK_METHOD1(SetPaused, void(bool paused));
  MOCK_METHOD0(Connect, void());
  MOCK_METHOD0(Disconnect, void());
  MOCK_METHOD0(CancelConnectionAttempt, void());
  MOCK_METHOD1(SendMessageImplProxy, void(WireMessage* message));
  MOCK_METHOD1(DeserializeWireMessageProxy,
               WireMessage*(bool* is_incomplete_message));

  // Gmock only supports copyable types, so create simple wrapper methods for
  // ease of mocking.
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override {
    SendMessageImplProxy(message.get());
  }

  std::unique_ptr<WireMessage> DeserializeWireMessage(
      bool* is_incomplete_message) override {
    return base::WrapUnique(DeserializeWireMessageProxy(is_incomplete_message));
  }

  using Connection::status;
  using Connection::SetStatus;
  using Connection::OnDidSendMessage;
  using Connection::OnBytesReceived;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

class MockConnectionObserver : public ConnectionObserver {
 public:
  MockConnectionObserver() {}
  virtual ~MockConnectionObserver() {}

  MOCK_METHOD3(OnConnectionStatusChanged,
               void(Connection* connection,
                    Connection::Status old_status,
                    Connection::Status new_status));
  MOCK_METHOD2(OnMessageReceived,
               void(const Connection& connection, const WireMessage& message));
  MOCK_METHOD3(OnSendCompleted,
               void(const Connection& connection,
                    const WireMessage& message,
                    bool success));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionObserver);
};

// Unlike WireMessage, offers a public constructor.
class TestWireMessage : public WireMessage {
 public:
  TestWireMessage() : WireMessage(std::string()) {}
  ~TestWireMessage() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWireMessage);
};

}  // namespace

TEST(ProximityAuthConnectionTest, IsConnected) {
  StrictMock<MockConnection> connection;
  EXPECT_FALSE(connection.IsConnected());

  connection.SetStatus(Connection::CONNECTED);
  EXPECT_TRUE(connection.IsConnected());

  connection.SetStatus(Connection::DISCONNECTED);
  EXPECT_FALSE(connection.IsConnected());

  connection.SetStatus(Connection::IN_PROGRESS);
  EXPECT_FALSE(connection.IsConnected());
}

TEST(ProximityAuthConnectionTest, SendMessage_FailsWhenNotConnected) {
  StrictMock<MockConnection> connection;
  connection.SetStatus(Connection::IN_PROGRESS);

  EXPECT_CALL(connection, SendMessageImplProxy(_)).Times(0);
  connection.SendMessage(std::unique_ptr<WireMessage>());
}

TEST(ProximityAuthConnectionTest,
     SendMessage_FailsWhenAnotherMessageSendIsInProgress) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::CONNECTED);
  connection.SendMessage(std::unique_ptr<WireMessage>());

  EXPECT_CALL(connection, SendMessageImplProxy(_)).Times(0);
  connection.SendMessage(std::unique_ptr<WireMessage>());
}

TEST(ProximityAuthConnectionTest, SendMessage_SucceedsWhenConnected) {
  StrictMock<MockConnection> connection;
  connection.SetStatus(Connection::CONNECTED);

  EXPECT_CALL(connection, SendMessageImplProxy(_));
  connection.SendMessage(std::unique_ptr<WireMessage>());
}

TEST(ProximityAuthConnectionTest,
     SendMessage_SucceedsAfterPreviousMessageSendCompletes) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::CONNECTED);
  connection.SendMessage(std::unique_ptr<WireMessage>());
  connection.OnDidSendMessage(TestWireMessage(), true /* success */);

  EXPECT_CALL(connection, SendMessageImplProxy(_));
  connection.SendMessage(std::unique_ptr<WireMessage>());
}

TEST(ProximityAuthConnectionTest, SetStatus_NotifiesObserversOfStatusChange) {
  StrictMock<MockConnection> connection;
  EXPECT_EQ(Connection::DISCONNECTED, connection.status());

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(observer,
              OnConnectionStatusChanged(&connection, Connection::DISCONNECTED,
                                        Connection::CONNECTED));
  connection.SetStatus(Connection::CONNECTED);
}

TEST(ProximityAuthConnectionTest,
     SetStatus_DoesntNotifyObserversIfStatusUnchanged) {
  StrictMock<MockConnection> connection;
  EXPECT_EQ(Connection::DISCONNECTED, connection.status());

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(observer, OnConnectionStatusChanged(_, _, _)).Times(0);
  connection.SetStatus(Connection::DISCONNECTED);
}

TEST(ProximityAuthConnectionTest,
     OnDidSendMessage_NotifiesObserversIfMessageSendInProgress) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::CONNECTED);
  connection.SendMessage(std::unique_ptr<WireMessage>());

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(observer, OnSendCompleted(Ref(connection), _, true));
  connection.OnDidSendMessage(TestWireMessage(), true /* success */);
}

TEST(ProximityAuthConnectionTest,
     OnDidSendMessage_DoesntNotifyObserversIfNoMessageSendInProgress) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::CONNECTED);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(observer, OnSendCompleted(_, _, _)).Times(0);
  connection.OnDidSendMessage(TestWireMessage(), true /* success */);
}

TEST(ProximityAuthConnectionTest,
     OnBytesReceived_NotifiesObserversOnValidMessage) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::CONNECTED);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  ON_CALL(connection, DeserializeWireMessageProxy(_))
      .WillByDefault(
          DoAll(SetArgPointee<0>(false), Return(new TestWireMessage)));
  EXPECT_CALL(observer, OnMessageReceived(Ref(connection), _));
  connection.OnBytesReceived(std::string());
}

TEST(ProximityAuthConnectionTest,
     OnBytesReceived_DoesntNotifyObserversIfNotConnected) {
  StrictMock<MockConnection> connection;
  connection.SetStatus(Connection::IN_PROGRESS);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(observer, OnMessageReceived(_, _)).Times(0);
  connection.OnBytesReceived(std::string());
}

TEST(ProximityAuthConnectionTest,
     OnBytesReceived_DoesntNotifyObserversIfMessageIsIncomplete) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::CONNECTED);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  ON_CALL(connection, DeserializeWireMessageProxy(_))
      .WillByDefault(DoAll(SetArgPointee<0>(true), Return(nullptr)));
  EXPECT_CALL(observer, OnMessageReceived(_, _)).Times(0);
  connection.OnBytesReceived(std::string());
}

TEST(ProximityAuthConnectionTest,
     OnBytesReceived_DoesntNotifyObserversIfMessageIsInvalid) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::CONNECTED);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  ON_CALL(connection, DeserializeWireMessageProxy(_))
      .WillByDefault(DoAll(SetArgPointee<0>(false), Return(nullptr)));
  EXPECT_CALL(observer, OnMessageReceived(_, _)).Times(0);
  connection.OnBytesReceived(std::string());
}

}  // namespace cryptauth
