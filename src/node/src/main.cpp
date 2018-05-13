// Copyright 2016 Chirstopher Torres (Raven), L3nn0x
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <curl/curl.h>
#include <cxxopts.hpp>
#include "crash_report.h"
#include "nodeserver.h"
#include "config.h"
#include "logconsole.h"
#include "version.h"
#include "network_thread_pool.h"

#include "connection.h"
#include "mysqlconnection.h"

#include <chrono>

namespace {
void DisplayTitle()
{
  auto console = Core::CLog::GetLogger(Core::log_type::GENERAL);
  if(auto log = console.lock())
  {
    log->info( "--------------------------------" );
    log->info( "        osIROSE 2 Alpha         " );
    log->info( "  http://forum.dev-osrose.com/  " );
    log->info( "--------------------------------" );
    log->info( "Git Branch/Revision: {}/{}", GIT_BRANCH, GIT_COMMIT_HASH );
  }
}

void CheckUser()
{
#ifndef _WIN32
  auto console = Core::CLog::GetLogger(Core::log_type::GENERAL);
  if(auto log = console.lock())
  {
    if ((getuid() == 0) && (getgid() == 0)) {
      log->warn( "You are running as the root superuser." );
      log->warn( "It is unnecessary and unsafe to run with root privileges." );
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }
#endif
}

struct MemoryStruct {
  char *memory;
  size_t size;
};
 
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  mem->memory = reinterpret_cast<char*>(realloc(mem->memory, mem->size + realsize + 1));
  if(mem->memory == NULL) {
    // out of memory!
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

std::string get_current_net_address()
{
  CURL* curl;
  CURLcode res;
  std::string address = "";
  
  struct MemoryStruct chunk;
 
  chunk.memory = reinterpret_cast<char*>(malloc(1));  // will be grown as needed by the realloc above
  chunk.size = 0;    // no data at this point
  
  curl_global_init(CURL_GLOBAL_DEFAULT);
  
  curl = curl_easy_init();
  
  if(curl) 
  {
    Core::Config& config = Core::Config::getInstance();
    curl_easy_setopt(curl, CURLOPT_URL, config.serverData().autoConfigureUrl.c_str());
    
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "osirose-node-server/1.0");
    
    res = curl_easy_perform(curl);
    
    // Check for errors
    if(res == CURLE_OK)
      address = chunk.memory;
 
    // always cleanup
    curl_easy_cleanup(curl);
  }
 
  curl_global_cleanup();
  
  free(chunk.memory);
  
  return address;
}

void ParseCommandLine(int argc, char** argv)
{
  cxxopts::Options options(argv[0], "osIROSE node server");

  try {
    std::string config_file_path = "";
    options.add_options()
    ("f,config_file",  "Config file path", cxxopts::value<std::string>(config_file_path)
      ->default_value("server.json"), "FILE_PATH")
    ("l,log_level", "Logging level (0-9)", cxxopts::value<int>()
      ->default_value("3"), "LEVEL")
    ("c,core_path", "Path to place minidumps when the app crashes", cxxopts::value<std::string>()
      ->default_value("/tmp/dumps"), "CORE")
    ("h,help",  "Print this help text")
    ;
    
    options.add_options("Networking")
    ("client_ip", "Client listen IP Address", cxxopts::value<std::string>()
      ->default_value("0.0.0.0"), "IP")
    ("client_port", "Client listen port", cxxopts::value<int>()
      ->default_value("29000"), "PORT")
    ("isc_ip", "ISC listen IP Address", cxxopts::value<std::string>()
      ->default_value("127.0.0.1"), "IP")
    ("isc_port", "ISC listen port", cxxopts::value<int>()
      ->default_value("29010"), "PORT")
    ("t,max_threads", "Max thread count", cxxopts::value<int>()
      ->default_value("512"), "COUNT")
    ("url", "Auto configure url", cxxopts::value<std::string>()
      ->default_value("http://ipv4.myexternalip.com/raw"), "URL")
    ;
    
    options.add_options("Database")
    ("db_host", "", cxxopts::value<std::string>()
      ->default_value("127.0.0.1"), "DB_HOST")
    ("db_port", "", cxxopts::value<int>()
      ->default_value("3306"), "DB_PORT")
    ("db_name", "", cxxopts::value<std::string>()
      ->default_value("osirose"), "DB_NAME")
    ("db_user", "", cxxopts::value<std::string>()
      ->default_value("root"), "DB_USER")
    ("db_pass", "", cxxopts::value<std::string>()
      ->default_value(""), "DB_PASS")
    ;

    options.parse(argc, argv);

    // Check to see if the user wants to see the help text
    if (options.count("help"))
    {
      std::cout << options.help({"", "Database", "Networking"}) << std::endl;
      exit(0);
    }

    Core::Config& config = Core::Config::getInstance(config_file_path);

    // We are using if checks here because we only want to override the config file if the option was supplied
    // Since this is a login server startup function we can get away with a little bit of overhead
    if( options.count("log_level") )
      config.loginServer().logLevel = options["log_level"].as<int>();

    if( options.count("client_ip") )
      config.serverData().ip = options["client_ip"].as<std::string>();

    if( options.count("client_port") )
      config.loginServer().clientPort = options["client_port"].as<int>();

    if( options.count("isc_ip") )
      config.serverData().iscListenIp = options["isc_ip"].as<std::string>();

    if( options.count("isc_port") )
      config.loginServer().iscPort = options["isc_port"].as<int>();
      
    if( options.count("url") )
    {
      config.serverData().autoConfigureAddress = true;
      config.serverData().autoConfigureUrl = options["url"].as<std::string>();
    }
    
    if( options.count("max_threads") ) 
    {
      config.serverData().maxThreads = options["max_threads"].as<int>();
      Core::NetworkThreadPool::GetInstance(config.serverData().maxThreads);
    }
    
    if( options.count("core_path") )
      config.serverData().core_dump_path = options["core_path"].as<std::string>();
      
    if( options.count("db_host") )
      config.database().host = options["db_host"].as<std::string>();
    if( options.count("db_port") )
      config.database().port = options["db_port"].as<int>();
    if( options.count("db_name") )
      config.database().database = options["db_name"].as<std::string>();
    if( options.count("db_user") )
      config.database().user = options["db_user"].as<std::string>();
    if( options.count("db_pass") )
      config.database().password = options["db_pass"].as<std::string>();
  }
  catch (const cxxopts::OptionException& ex) {
    std::cout << ex.what() << std::endl;
    std::cout << options.help({"", "Database", "Networking"}) << std::endl;
    exit(1);
  }
}
} // end namespace

int main(int argc, char* argv[]) {
  try {
    ParseCommandLine(argc, argv);
    
    Core::Config& config = Core::Config::getInstance();
    Core::CrashReport(config.serverData().core_dump_path);

    auto console = Core::CLog::GetLogger(Core::log_type::GENERAL);
    if(auto log = console.lock())
      log->info( "Starting up server..." );

    Core::CLog::SetLevel((spdlog::level::level_enum)config.loginServer().logLevel);
    DisplayTitle();
    CheckUser();

    if(auto log = console.lock()) {
      log->set_level((spdlog::level::level_enum)config.loginServer().logLevel);
      log->trace("Trace logs are enabled.");
      log->debug("Debug logs are enabled.");
    }

    Core::NetworkThreadPool threadPool{config.serverData().maxThreads};
    std::string ip_addr = config.serverData().ip;
    if( true == config.serverData().autoConfigureAddress )
    {
      ip_addr = get_current_net_address();
      ip_addr.replace(ip_addr.begin(), ip_addr.end(), '\n', '\0');
      if(auto log = console.lock()) {
        log->info( "IP address is \"{}\"", ip_addr );
      }
    }

    NodeServer loginServer{std::make_unique<Core::CNetwork_Asio>(&threadPool)};
    NodeServer charServer{std::make_unique<Core::CNetwork_Asio>(&threadPool)};
    NodeServer mapServer{std::make_unique<Core::CNetwork_Asio>(&threadPool)};

    loginServer.init(ip_addr, config.loginServer().clientPort);
    loginServer.listen();

    charServer.init(ip_addr, config.charServer().clientPort);
    charServer.listen();

    mapServer.init(ip_addr, config.mapServer().clientPort);
    mapServer.listen();

    while (loginServer.is_active()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if(auto log = console.lock())
      log->info( "Server shutting down..." );
    spdlog::drop_all();

  }
  catch (const spdlog::spdlog_ex& ex) {
     std::cout << "Log failed: " << ex.what() << std::endl;
  }
  return 0;
}
