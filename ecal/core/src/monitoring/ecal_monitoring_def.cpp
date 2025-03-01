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
 * @brief  Global monitoring class
**/

#include "ecal_monitoring_def.h"
#include "ecal_monitoring_impl.h"
#include "logging/ecal_log_impl.h"
#include "ecal_global_accessors.h"

namespace eCAL
{
  CMonitoring::CMonitoring()
  {
    m_monitoring_impl = new CMonitoringImpl;
  }

  CMonitoring::~CMonitoring()
  {
    delete m_monitoring_impl;
    m_monitoring_impl = nullptr;
  }

  void CMonitoring::Create()
  {
    m_monitoring_impl->Create();
  }

  void CMonitoring::Destroy()
  {
    m_monitoring_impl->Destroy();
  }

  void CMonitoring::SetExclFilter(const std::string& filter_)
  {
    m_monitoring_impl->SetExclFilter(filter_);
  }

  void CMonitoring::SetInclFilter(const std::string& filter_)
  {
    m_monitoring_impl->SetInclFilter(filter_);
  }

  void CMonitoring::SetFilterState(bool state_)
  {
    m_monitoring_impl->SetFilterState(state_);
  }

  void CMonitoring::GetMonitoring(eCAL::pb::Monitoring& monitoring_, unsigned int entities_)
  {
    m_monitoring_impl->GetMonitoringPb(monitoring_, entities_);
  }

  void CMonitoring::GetMonitoring(eCAL::Monitoring::SMonitoring& monitoring_, unsigned int entities_)
  {
    m_monitoring_impl->GetMonitoringStructs(monitoring_, entities_);
  }

  namespace Monitoring
  {
    ////////////////////////////////////////////////////////
    // static library interface
    ////////////////////////////////////////////////////////
    int SetExclFilter(const std::string& filter_)
    {
      if (g_monitoring() != nullptr) g_monitoring()->SetExclFilter(filter_);
      return(0);
    }

    int SetInclFilter(const std::string& filter_)
    {
      if (g_monitoring() != nullptr) g_monitoring()->SetInclFilter(filter_);
      return(0);
    }

    int SetFilterState(const bool state_)
    {
      if (g_monitoring() != nullptr) g_monitoring()->SetFilterState(state_);
      return(0);
    }

    int GetMonitoring(std::string& mon_)
    {
      eCAL::pb::Monitoring monitoring;
      if (g_monitoring() != nullptr) g_monitoring()->GetMonitoring(monitoring);

      mon_ = monitoring.SerializeAsString();
      return((int)mon_.size());
    }

    int GetMonitoring(std::string& mon_, unsigned int entities_)
    {
      eCAL::pb::Monitoring monitoring;
      if (g_monitoring() != nullptr) g_monitoring()->GetMonitoring(monitoring, entities_);

      mon_ = monitoring.SerializeAsString();
      return((int)mon_.size());
    }

    int GetMonitoring(eCAL::Monitoring::SMonitoring& mon_, unsigned int entities_)
    {
      if (g_monitoring() != nullptr)
      {
        g_monitoring()->GetMonitoring(mon_, entities_);
        return(static_cast<int>(mon_.process.size() + mon_.publisher.size() + mon_.subscriber.size() + mon_.server.size() + mon_.clients.size()));
      }
      return(0);
    }

    int GetLogging(std::string& log_)
    {
      eCAL::pb::Logging logging;
      if (g_log() != nullptr) g_log()->GetLogging(logging);

      log_ = logging.SerializeAsString();
      return((int)log_.size());
    }

    int PubMonitoring(bool /*state_*/, std::string /*name_*/)
    {
      // TODO: Remove this function from the API
      return 0;
    }

    int PubLogging(bool /*state_*/, std::string /*name_*/)
    {
      // TODO: Remove this function from the API
      return 0;
    }
  }
}
