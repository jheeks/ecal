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
 * @brief  eCAL logging class
**/

#include <ecal/ecal.h>
#include <ecal/ecal_os.h>
#include <ecal/ecal_config.h>

#include "ecal_log_impl.h"
#include "io/udp/ecal_udp_configurations.h"

#include <mutex>
#include <cstdio>

#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <iomanip>
#include <ctime>
#include <chrono>

#ifdef _MSC_VER
#pragma warning(push, 0) // disable proto warnings
#endif
#include <ecal/core/pb/monitoring.pb.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef ECAL_OS_WINDOWS
#include "ecal_win_main.h"
#include <ecal_utils/filesystem.h>

namespace
{
  bool isDirectory(const std::string& path_)
  {
    if (path_.empty()) return false;

    return EcalUtils::Filesystem::IsDir(path_, EcalUtils::Filesystem::Current);
  }

  std::string get_time_str()
  {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream tstream;
    tstream << std::put_time(&tm, "%Y-%m-%d-%H-%M-%S");
    return(tstream.str());
  }
}
#endif

#ifdef ECAL_OS_LINUX
#include <sys/stat.h>
#include <sys/time.h>

static bool isDirectory(const std::string& path_)
{
  if (path_.empty()) return false;

  struct stat st;
  if (stat(path_.c_str(), &st) == 0)
    return S_ISDIR(st.st_mode);

  return false;
}

static std::string get_time_str()
{
  char            fmt[64];
  struct timeval  tv;
  struct tm       *tm;
  gettimeofday(&tv, NULL);
  if ((tm = localtime(&tv.tv_sec)) != NULL)
  {
    strftime(fmt, sizeof fmt, "%Y-%m-%d-%H-%M-%S", tm);
  }
  return(std::string(fmt));
}
#endif

namespace eCAL
{
  CLog::CLog() :
          m_created(false),
          m_pid(0),
          m_logfile(nullptr),
          m_level(log_level_none),
          m_filter_mask_con(log_level_info | log_level_warning | log_level_error | log_level_fatal),
          m_filter_mask_file(log_level_info | log_level_warning | log_level_error | log_level_fatal | log_level_debug1 | log_level_debug2),
          m_filter_mask_udp(log_level_info | log_level_warning | log_level_error | log_level_fatal | log_level_debug1 | log_level_debug2),
          m_core_time_start(std::chrono::nanoseconds(0))
  {
    m_core_time = std::chrono::duration<double>(-1.0);
  }

  CLog::~CLog()
  {
    Destroy();
  }

  void CLog::Create()
  {
    m_hname = Process::GetHostName();
    m_pid   = Process::GetProcessID();
    m_pname = Process::GetProcessName();
    m_level = log_level_info;

    // parse logging filter strings
    m_filter_mask_con  = Config::GetConsoleLogFilter();
    m_filter_mask_file = Config::GetFileLogFilter();
    m_filter_mask_udp  = Config::GetUdpLogFilter();

    // create log file
    if(m_filter_mask_file != 0)
    {
      // check ECAL_DATA
      const std::string ecal_log_path = Util::GeteCALLogPath();
      if (!isDirectory(ecal_log_path)) return;

      const std::string tstring = get_time_str();

      m_logfile_name = ecal_log_path + tstring + "_" + eCAL::Process::GetUnitName() + "_" + std::to_string(m_pid) + ".log";
      m_logfile = fopen(m_logfile_name.c_str(), "w");
    }

    if(m_filter_mask_udp != 0)
    {
      // set logging send network attributes
      IO::UDP::SSenderAttr attr;
      attr.address   = UDP::GetLoggingAddress();
      attr.port      = UDP::GetLoggingPort();
      attr.ttl       = UDP::GetMulticastTtl();
      attr.broadcast = UDP::IsBroadcast();
      attr.loopback  = true;
      attr.sndbuf    = Config::GetUdpMulticastSndBufSizeBytes();

      // create udp logging sender
      m_udp_logging_sender = std::make_unique<UDP::CLoggingSender>(attr);
    }

    // set logging receive network attributes
    IO::UDP::SReceiverAttr attr;
    attr.address   = UDP::GetLoggingAddress();
    attr.port      = UDP::GetLoggingPort();
    attr.broadcast = UDP::IsBroadcast();
    attr.loopback  = true;
    attr.rcvbuf    = Config::GetUdpMulticastRcvBufSizeBytes();

    // start logging receiver
    const UDP::CLoggingReceiver::LogMessageCallbackT log_message_callback = std::bind(&CLog::RegisterLogMessage, this, std::placeholders::_1);
    m_log_receiver = std::make_shared<UDP::CLoggingReceiver>(attr, log_message_callback);

    m_created = true;
  }

  void CLog::Destroy()
  {
    if(!m_created) return;

    const std::lock_guard<std::mutex> lock(m_log_sync);

    m_udp_logging_sender.reset();

    if(m_logfile != nullptr) fclose(m_logfile);
    m_logfile = nullptr;

    m_created = false;
  }

  void CLog::SetLogLevel(const eCAL_Logging_eLogLevel level_)
  {
    const std::lock_guard<std::mutex> lock(m_log_sync);
    m_level = level_;
  }

  eCAL_Logging_eLogLevel CLog::GetLogLevel()
  {
    const std::lock_guard<std::mutex> lock(m_log_sync);
    return(m_level);
  }

  void CLog::Log(const eCAL_Logging_eLogLevel level_, const std::string& msg_)
  {
    const std::lock_guard<std::mutex> lock(m_log_sync);

    if(!m_created) return;
    if(msg_.empty()) return;

    const eCAL_Logging_Filter log_con  = level_ & m_filter_mask_con;
    const eCAL_Logging_Filter log_file = level_ & m_filter_mask_file;
    const eCAL_Logging_Filter log_udp  = level_ & m_filter_mask_udp;
    if((log_con | log_file | log_udp) == 0) return;

    static eCAL::pb::LogMessage ecal_msg;
    static std::string        ecal_msg_s;
    auto                      log_time = eCAL::Time::ecal_clock::now();

    if(log_con != 0)
    {
      std::cout << msg_ << std::endl;
    }

    if((log_file != 0) && (m_logfile != nullptr))
    {
      std::stringstream msg_stream;
      msg_stream << std::chrono::duration_cast<std::chrono::milliseconds>(log_time.time_since_epoch()).count();
      msg_stream << " ms";
      msg_stream << " | ";
      msg_stream << m_hname;
      msg_stream << " | ";
      msg_stream << eCAL::Process::GetUnitName();
      msg_stream << " | ";
      msg_stream << m_pid;
      msg_stream << " | ";
      switch(level_)
      {
      case log_level_none:
      case log_level_all:
        break;
      case log_level_info:
        msg_stream << "info";
        break;
      case log_level_warning:
        msg_stream << "warning";
        break;
      case log_level_error:
        msg_stream << "error";
        break;
      case log_level_fatal:
        msg_stream << "fatal";
        break;
      case log_level_debug1:
        msg_stream << "debug1";
        break;
      case log_level_debug2:
        msg_stream << "debug2";
        break;
      case log_level_debug3:
        msg_stream << "debug3";
        break;
      case log_level_debug4:
        msg_stream << "debug4";
        break;
      }
      msg_stream << " | ";
      msg_stream << msg_;

      fprintf(m_logfile, "%s\n", msg_stream.str().c_str());
      fflush(m_logfile);
    }

    if((log_udp != 0) && m_udp_logging_sender)
    {
      // set up log message
      ecal_msg.Clear();
      ecal_msg.set_time(std::chrono::duration_cast<std::chrono::microseconds>(log_time.time_since_epoch()).count());
      ecal_msg.set_hname(m_hname);
      ecal_msg.set_pid(m_pid);
      ecal_msg.set_pname(m_pname);
      ecal_msg.set_uname(eCAL::Process::GetUnitName());
      ecal_msg.set_level(level_);
      ecal_msg.set_content(msg_);

      // sent it
      m_udp_logging_sender->Send(ecal_msg);
    }
  }

  void CLog::Log(const std::string& msg_)
  {
    Log(m_level, msg_);
  }

  void CLog::StartCoreTimer()
  {
    const std::lock_guard<std::mutex> lock(m_log_sync);

    m_core_time_start = std::chrono::steady_clock::now();
  }

  void CLog::StopCoreTimer()
  {
    const std::lock_guard<std::mutex> lock(m_log_sync);

    m_core_time = std::chrono::steady_clock::now() - m_core_time_start;
  }

  void CLog::SetCoreTime(const std::chrono::duration<double>& time_)
  {
    const std::lock_guard<std::mutex> lock(m_log_sync);

    m_core_time = time_;
  }

  std::chrono::duration<double> CLog::GetCoreTime()
  {
    const std::lock_guard<std::mutex> lock(m_log_sync);

    return(m_core_time);
  }

  void CLog::GetLogging(eCAL::pb::Logging& logging_)
  {
    // clear protobuf object
    logging_.Clear();

    // acquire access
    const std::lock_guard<std::mutex> lock(m_log_msglist_sync);

    LogMessageListT::const_iterator siter = m_log_msglist.begin();
    while (siter != m_log_msglist.end())
    {
      // add log message
      eCAL::pb::LogMessage* pMonLogMessage = logging_.add_logs();

      // copy content
      pMonLogMessage->CopyFrom(*siter);

      ++siter;
    }

    // empty message list
    m_log_msglist.clear();
  }

  void CLog::RegisterLogMessage(const eCAL::pb::LogMessage& log_msg_)
  {
    // in "network mode" we accept all log messages
    // in "local mode" we accept log messages from this host only
    if ((m_hname == log_msg_.hname()) || Config::IsNetworkEnabled())
    {
      const std::lock_guard<std::mutex> lock(m_log_msglist_sync);
      m_log_msglist.emplace_back(log_msg_);
    }
  }
}
