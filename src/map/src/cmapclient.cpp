// Copyright 2016 Chirstopher Torres (Raven), L3nn0x
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cmath>
#include "cmapclient.h"
#include "cmapisc.h"
#include "cmapserver.h"
#include "config.h"
#include "connection.h"
#include "entitycomponents.h"
#include "epackettype.h"
#include "cli_joinserverreq.h"
#include "srv_inventorydata.h"
#include "srv_joinserverreply.h"
#include "srv_questdata.h"
#include "srv_removeobject.h"
#include "srv_selectcharreply.h"
#include "srv_teleportreply.h"

using namespace RoseCommon;

CMapClient::CMapClient()
    : CRoseClient(), access_rights_(0), login_state_(eSTATE::DEFAULT), sessionId_(0), userid_(0), charid_(0) {}

CMapClient::CMapClient(std::unique_ptr<Core::INetwork> _sock, std::shared_ptr<EntitySystem> entitySystem)
    : CRoseClient(std::move(_sock)),
      access_rights_(0),
      login_state_(eSTATE::DEFAULT),
      sessionId_(0),
      userid_(0),
      charid_(0),
      entitySystem_(entitySystem) {}

CMapClient::~CMapClient() {}

bool CMapClient::HandlePacket(uint8_t* _buffer) {
  switch (CRosePacket::type(_buffer)) {
    case ePacketType::PAKCS_ALIVE:
      if (login_state_ != eSTATE::LOGGEDIN) {
        // logger_->warn("Client {} is attempting to execute an action before logging in.", get_id());
        return true;
      }
      updateSession();
      break;
    case ePacketType::PAKCS_JOIN_SERVER_REQ:
      return JoinServerReply(getPacket<ePacketType::PAKCS_JOIN_SERVER_REQ>(_buffer));  // Allow client to connect
    case ePacketType::PAKCS_CHANGE_CHAR_REQ: {
      logger_->warn("Change character hasn't been implemented yet.");
      // TODO: Send ePacketType::PAKCC_CHAN_CHAR_REPLY to the client with the character/node server ip and port to
      // change their character
      return true;
    }
    case ePacketType::PAKCS_CHANGE_MAP_REQ:
      if (login_state_ != eSTATE::LOGGEDIN) {
        logger_->warn("Client {} is attempting to execute an action before logging in.", get_id());
        return true;
      }
      break;
    default:
      break;
  }
  if (login_state_ != eSTATE::LOGGEDIN) {
    logger_->warn("Client {} is attempting to execute an action befire logging in.", get_id());
    return CRoseClient::HandlePacket(_buffer);
  }
  auto packet = fetchPacket(_buffer);
  if (!packet) {
    CRoseClient::HandlePacket(_buffer);
    return true;
  }
  if (!entitySystem_->dispatch(entity_, std::move(packet))) {
    if(false == CRoseClient::HandlePacket(_buffer)) {  // FIXME : removed the return because I want to be able to
                                         // mess around with unkown packets for the time being
      logger_->warn("There is no system willing to deal with this packet");
    }
  }
  return true;
}

void CMapClient::updateSession() {
  logger_->trace("CMapClient::updateSession() start");
  using namespace std::chrono_literals;
  static std::chrono::steady_clock::time_point time{};

  if (Core::Time::GetTickCount() - time < 2min)
    return;

  time = Core::Time::GetTickCount();
  Core::SessionTable session{};
  auto conn = Core::connectionPool.getConnection(Core::osirose);
  conn(sqlpp::update(session).set(session.time = std::chrono::system_clock::now()).where(session.userid == get_id()));
  logger_->trace("CMapClient::updateSession() end");
}

void CMapClient::OnDisconnected() {
  logger_->trace("CMapClient::OnDisconnected() start");
  if (login_state_ == eSTATE::DEFAULT) return;
  entitySystem_->destroy(entity_, login_state_ != eSTATE::SWITCHING);

  if (login_state_ != eSTATE::SWITCHING) {
      Core::AccountTable table{};
      auto conn = Core::connectionPool.getConnection(Core::osirose);
      conn(sqlpp::update(table).set(table.online = 0).where(table.id == get_id()));
  }
  login_state_ = eSTATE::DEFAULT;
  logger_->trace("CMapClient::OnDisconnected() end");
}

bool CMapClient::JoinServerReply(std::unique_ptr<RoseCommon::CliJoinServerReq> P) {
  logger_->trace("CMapClient::JoinServerReply()");

  if (login_state_ != eSTATE::DEFAULT) {
    logger_->warn("Client {} is attempting to login when already logged in.", get_id());
    return true;
  }

  uint32_t sessionID = P->sessionId();
  std::string password = Core::escapeData(P->password());

  auto conn = Core::connectionPool.getConnection(Core::osirose);
  Core::AccountTable accounts{};
  Core::SessionTable sessions{};
  try {
    const auto res = conn(
        sqlpp::select(sessions.userid, sessions.charid, sessions.worldip, accounts.platinium)
            .from(sessions.join(accounts).on(sessions.userid == accounts.id))
            .where(sessions.id == sessionID and accounts.password == sqlpp::verbatim<sqlpp::varchar>(fmt::format(
                                                                         "SHA2(CONCAT('{}', salt), 256)", password))));

    if (!res.empty()) {
      logger_->debug("Client {} auth OK.", get_id());
      login_state_ = eSTATE::LOGGEDIN;
      const auto& row = res.front();
      userid_ = row.userid;
      charid_ = row.charid;
      sessionId_ = sessionID;
      bool platinium = false;
      platinium = row.platinium;
      entity_ = entitySystem_->loadCharacter(charid_, platinium);

      if (entity_) {
        Core::Config& config = Core::Config::getInstance();
        conn(sqlpp::update(sessions)
                 .set(sessions.worldip = config.serverData().ip, sessions.worldport = config.mapServer().clientPort)
                 .where(sessions.id == sessionID));
        entity_.assign<SocketConnector>(shared_from_this());

        auto packet = makePacket<ePacketType::PAKSC_JOIN_SERVER_REPLY>(SrvJoinServerReply::OK, entity_.component<BasicInfo>()->id_);
        send(*packet);

        if (row.worldip.is_null()) { // if there is already a world ip, the client is switching servers so we shouldn't send it the starting data
          // SEND PLAYER DATA HERE!!!!!!
          auto packet2 = makePacket<ePacketType::PAKWC_SELECT_CHAR_REPLY>(entity_);
          send(*packet2);

          auto packet3 = makePacket<ePacketType::PAKWC_INVENTORY_DATA>(entity_);
          send(*packet3);

          auto packet4 = makePacket<ePacketType::PAKWC_QUEST_DATA>(entity_);
          send(*packet4);

          auto packet5 = makePacket<ePacketType::PAKWC_BILLING_MESSAGE>();
          send(*packet5);
        } else {
          send(*makePacket<ePacketType::PAKWC_TELEPORT_REPLY>(entity_));
        }

      } else {
        logger_->debug("Something wrong happened when creating the entity");
        auto packet = makePacket<ePacketType::PAKSC_JOIN_SERVER_REPLY>(SrvJoinServerReply::FAILED, 0);
        send(*packet);
      }
    } else {
      logger_->debug("Client {} auth INVALID_PASS.", get_id());
      auto packet = makePacket<ePacketType::PAKSC_JOIN_SERVER_REPLY>(SrvJoinServerReply::INVALID_PASSWORD, 0);
      send(*packet);
    }
  } catch (const sqlpp::exception& e) {
    logger_->error("Error while accessing the database: {}", e.what());
    auto packet = makePacket<ePacketType::PAKSC_JOIN_SERVER_REPLY>(SrvJoinServerReply::FAILED, 0);
    send(*packet);
  }
  return true;
};

bool CMapClient::is_nearby(const CRoseClient* _otherClient) const {
  logger_->trace("CMapClient::is_nearby()");
  return entitySystem_->isNearby(entity_, _otherClient->getEntity());
}
