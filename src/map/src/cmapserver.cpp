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

#include "cmapserver.h"
#include "cmapclient.h"
#include "cmapisc.h"
#include "epackettype.h"
#include "platform_defines.h"

using namespace RoseCommon;

CMapServer::CMapServer(std::unique_ptr<Core::INetwork> sock, bool _isc, int16_t mapidx)
    : CRoseServer(std::move(sock), _isc), map_idx_(mapidx), client_count_(0), server_count_(0), entitySystem_(std::make_shared<EntitySystem>()) {

  if (mapidx >= 0) {
    // We are a worker thread/process
    // We need to connect to the master thread/process to get data to handle
  } else {
    // We are a master/node process
    // We accept player connections and redirect their packet data to the
    // correct worker
  }
}

void CMapServer::SendPacket(const std::shared_ptr<CMapClient>& sender, CMapServer::eSendType type, CRosePacket &_buffer) {
  CRoseServer::SendPacket(sender.get(), type, _buffer);
}

void CMapServer::SendPacket(const CMapClient& sender, CMapServer::eSendType type, CRosePacket &_buffer) {
    CRoseServer::SendPacket(static_cast<const CRoseClient&>(sender), type, _buffer);
}

CMapServer::~CMapServer() {}

void CMapServer::OnAccepted(std::unique_ptr<Core::INetwork> _sock) {
  //if (_sock->is_active()) {
    // Do Something?
    std::string _address = _sock->get_address();
    if (IsISCServer() == false) {
      std::lock_guard<std::mutex> lock(client_list_mutex_);
      std::shared_ptr<CMapClient> nClient = std::make_shared<CMapClient>(std::move(_sock), entitySystem_);
      nClient->set_id(++client_count_);
      nClient->set_update_time(Core::Time::GetTickCount());
      nClient->set_active(true);
      nClient->start_recv();
      logger_->info("Client connected from: {}", _address.c_str());
      client_list_.push_front(std::move(nClient));
    } else {
      std::lock_guard<std::mutex> lock(isc_list_mutex_);
      std::shared_ptr<CMapISC> nClient = std::make_shared<CMapISC>(std::move(_sock));
      nClient->set_id( server_count_++ );
      nClient->set_update_time(Core::Time::GetTickCount());
      nClient->set_active(true);
      nClient->start_recv();
      logger_->info( "Server connected from: {}", _address.c_str() );
      isc_list_.push_front(std::move(nClient));
    }
  //}
}

void CMapServer::update(double dt) {
    entitySystem_->update(dt);
}
