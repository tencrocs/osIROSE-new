#include "gtest/gtest.h"

#include <stdint.h>
#include "epackettype.h"
#include "crosepacket.h"
#include "cloginserver.h"
#include "ccharisc.h"
#include "mock/login/mock_cloginclient.h"
#include "mock/login/mock_cloginisc.h"
#include "cnetwork_asio.h"
#include "cli_acceptreq.h"
#include "cli_loginreq.h"
#include "cli_channellistreq.h"
#include "cli_srvselectreq.h"
#include "cli_alive.h"
#include "connection.h"
#include "config.h"

using namespace RoseCommon;

TEST(TestLoginServer, TestClientPacketPath) {
  Core::Config& config = Core::Config::getInstance();
  Core::connectionPool.addConnector(Core::osirose, std::bind(
                                    Core::mysqlFactory,
                                    config.database().user,
                                    config.database().password,
                                    config.database().database,
                                    config.database().host,
                                    config.database().port));
CLoginServer network;
std::unique_ptr<CLoginISC> iscServ = std::make_unique<CLoginISC>();
CLoginClient_Mock netConnect;
CNetworkThreadPool threadPool{};

  iscServ->set_socket(std::make_unique<Core::CNetwork_Asio>(&threadPool));
  netConnect.set_socket(std::make_unique<Core::CNetwork_Asio>(&threadPool));

  EXPECT_EQ(true, network.init("127.0.0.1", 29110));
  EXPECT_EQ(true, network.listen());

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  EXPECT_EQ(true, netConnect.init("127.0.0.1", 29110));
  EXPECT_EQ(true, netConnect.connect());


  std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
  auto pak = std::unique_ptr<CliAcceptReq>(new CliAcceptReq());
  netConnect.send(*pak);

  //TODO(raven): Move this into a static function so we can just call the function
  //TODO(raven): SendLogin(&netConnect, "test2", "cc03e747a6afbbcbf8be7668acfebee5");
  // cc03e747a6afbbcbf8be7668acfebee5 == test123
  auto pak2 = std::unique_ptr<CliLoginReq>(new CliLoginReq("test2", "cc03e747a6afbbcbf8be7668acfebee5"));
  netConnect.send(*pak2);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  iscServ->set_id(0);
  iscServ->set_type(1);
  CLoginServer::GetISCList().push_front(std::move(iscServ));


  //-----------------------------------------
  // We aren't logged in yet
  // We should get a warning
  //-----------------------------------------
  auto pak3 = std::unique_ptr<CRosePacket>(new CliChannelListReq(1));
  netConnect.send(*pak3);

  auto pak4 = std::unique_ptr<CRosePacket>(new CliSrvSelectReq(0, 0));
  netConnect.send(*pak4);
  //-----------------------------------------

  //Incorrect Password
  pak2 = std::unique_ptr<CliLoginReq>(new CliLoginReq("test", "cc03e747a6afbbcbf8be7668acfebee6"));
  netConnect.send(*pak2);

  //Correct password
  pak2 = std::unique_ptr<CliLoginReq>(new CliLoginReq("test", "cc03e747a6afbbcbf8be7668acfebee5"));
  netConnect.send(*pak2);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  pak3 = std::unique_ptr<CRosePacket>(new CliChannelListReq(1));
  netConnect.send(*pak3);

  pak4 = std::unique_ptr<CRosePacket>(new CliSrvSelectReq(0,0));
  netConnect.send(*pak4);

  auto pak5 = std::unique_ptr<CRosePacket>(new CliAlive());
  netConnect.send(*pak5);


  std::this_thread::sleep_for(
      std::chrono::milliseconds(1000));  // Change this to condition variables

  EXPECT_NO_FATAL_FAILURE(netConnect.disconnect());

  CLoginServer::GetISCList().clear();

  EXPECT_NO_FATAL_FAILURE(netConnect.shutdown());
  EXPECT_NO_FATAL_FAILURE(network.shutdown());
}

TEST(TestLoginServer, TestISCRosePacketPath) {

  CLoginServer network(true);
  CCharISC netConnect;
  CNetworkThreadPool threadPool{};
  netConnect.set_socket(std::make_unique<Core::CNetwork_Asio>(&threadPool));

  EXPECT_EQ(true, network.init("127.0.0.1", 29111));
  EXPECT_NO_FATAL_FAILURE(network.listen());

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_EQ(true, netConnect.init("127.0.0.1", 29111));
  EXPECT_NO_FATAL_FAILURE(netConnect.connect());

  std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
//  CRosePacket* pak = new CRosePacket( ePacketType::ISC_ALIVE );
//  netConnect.send_data( pak );
//  {
//    uint8_t serverCount = 0;
//    std::lock_guard<std::mutex> lock(CLoginServer::GetISCListMutex());
//    for (auto& server : CLoginServer::GetISCList()) {
//      if (server->get_type() == 1) serverCount++;
//    }
//
//    EXPECT_EQ(1, serverCount);
//  }

  std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
//  CRosePacket* pak2 = new CRosePacket( ePacketType::ISC_ALIVE );
//  netConnect.send_data( pak2 );

  std::this_thread::sleep_for(
      std::chrono::milliseconds(500));  // Change this to condition variables
  //EXPECT_NO_FATAL_FAILURE(netConnect.disconnect());
  //EXPECT_NO_FATAL_FAILURE(netConnect.shutdown());

  EXPECT_NO_FATAL_FAILURE(network.shutdown(true));
}
