#include "gtest/gtest.h"

#include "systems/chatsystem.h"
#include "mock/map/mock_systemmanager.h"
#include "mock/map/mock_entitysystem.h"
#include "mock/map/mock_cmapclient.h"
#include "cnetwork_asio.h"

using namespace RoseCommon;
using namespace Systems;

using ::testing::Return;

TEST(TestSystems, TestChatSystemSendMsg) {
    EntitySystem_Mock entitySystem;
    SystemManager_Mock mock(entitySystem);
    ChatSystem chat(mock);
    EntityManager man;
    CNetworkThreadPool threadPool{};
    std::shared_ptr<CMapClient_Mock> cli = std::make_shared<CMapClient_Mock>(std::make_unique<Core::CNetwork_Asio>(&threadPool));
    Entity e = man.create();
    e.assign<SocketConnector>(cli);
    EXPECT_CALL(*cli, send(_)).WillOnce(Return(true));
    chat.sendMsg(e, "test");
    e.remove<SocketConnector>();
    chat.sendMsg(e, "test");
    e.destroy();
    chat.sendMsg(e, "test");
}
