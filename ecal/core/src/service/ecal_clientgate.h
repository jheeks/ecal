/* ========================= eCAL LICENSE =================================
 *
 * Copyright (C) 2016 - 2019 Continental Corporation
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
 *
 * ========================= eCAL LICENSE =================================
*/

/**
 * @brief  eCAL client gateway class
**/

#pragma once

#include "ecal_def.h"
#include "util/ecal_expmap.h"

#include <ecal/ecal_callback.h>

#ifdef _MSC_VER
#pragma warning(push, 0) // disable proto warnings
#endif
#include <ecal/core/pb/ecal.pb.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <atomic>
#include <shared_mutex>
#include <set>

namespace eCAL
{
  class CServiceClientImpl;

  class CClientGate
  {
  public:
    CClientGate();
    ~CClientGate();

    void Create();
    void Destroy();

    bool Register  (CServiceClientImpl* client_);
    bool Unregister(CServiceClientImpl* client_);

    void ApplyServiceRegistration(const eCAL::pb::Sample& ecal_sample_);

    std::vector<SServiceAttr> GetServiceAttr(const std::string& service_name_);

    void RefreshRegistrations();

    bool ApplyServiceToDescGate(const std::string& service_name_
      , const std::string& method_name_
      , const SDataTypeInformation& request_type_information_
      , const SDataTypeInformation& response_type_information_);

  protected:
    static std::atomic<bool>    m_created;

    using ServiceNameServiceImplSetT = std::set<CServiceClientImpl *>;
    std::shared_timed_mutex     m_client_set_sync;
    ServiceNameServiceImplSetT  m_client_set;

    using ConnectedMapT = Util::CExpMap<std::string, SServiceAttr>;
    std::shared_timed_mutex     m_service_register_map_sync;
    ConnectedMapT               m_service_register_map;
  };
}
