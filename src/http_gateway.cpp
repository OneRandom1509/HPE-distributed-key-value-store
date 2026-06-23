#include "KVDistributor.hpp"
#include "KVStore.hpp"
#include "config.hpp"
#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include <httplib.h>
#include <spdlog/spdlog.h>

static std::atomic<bool> shutdown_flag{false};
static httplib::Server *http_server = nullptr;

static void handleSignal(int) { shutdown_flag.store(true); }

static std::string nodeTagFromConfig(const Config &config)
{
  std::string local = config.read_ip();
  auto pos = local.rfind(':');
  if(pos != std::string::npos && pos + 1 < local.size())
    return local.substr(pos + 1);
  return "";
}

int main(int argc, char **argv)
{
  const char *env_log = std::getenv("LOG_LEVEL");
  if(env_log)
    {
      std::string lvl(env_log);
      std::transform(lvl.begin(), lvl.end(), lvl.begin(), ::tolower);
      if(lvl == "trace")
        spdlog::set_level(spdlog::level::trace);
      else if(lvl == "debug")
        spdlog::set_level(spdlog::level::debug);
      else if(lvl == "info")
        spdlog::set_level(spdlog::level::info);
      else if(lvl == "warn" || lvl == "warning")
        spdlog::set_level(spdlog::level::warn);
      else if(lvl == "err" || lvl == "error")
        spdlog::set_level(spdlog::level::err);
      else if(lvl == "critical")
        spdlog::set_level(spdlog::level::critical);
      else if(lvl == "off")
        spdlog::set_level(spdlog::level::off);
      else
        spdlog::set_level(spdlog::level::info);
    }
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  spdlog::info("HTTP Gateway starting");

  std::string config_path = "config/config.json";
  StorageMode storage_mode = StorageMode::MEMORY;
  int http_port = 9090;

  // Parse --config flag and collect positional args
  std::vector<std::string> args;
  for(int i = 1; i < argc; i++)
    {
      std::string arg = argv[i];
      if(arg == "--config" && i + 1 < argc)
        config_path = argv[++i];
      else if(arg == "--port" && i + 1 < argc)
        http_port = std::stoi(argv[++i]);
      else
        args.push_back(argv[i]);
    }

  size_t ai = 0;
  if(args.size() > ai)
    {
      std::string mode = args[ai++];
      std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
      if(mode == "persistent")
        storage_mode = StorageMode::PERSISTENT;
    }

  if(args.size() > ai)
    http_port = std::stoi(args[ai++]);

  Config config(config_path);

  try
    {
      std::string node_tag = nodeTagFromConfig(config);
      KvStore &kv_store = KvStore::get_instance(
        0, storage_mode, ConnectionMode::CLIENT, node_tag);

      spdlog::info("Connected to server storage, mode={}",
                   storage_mode == StorageMode::PERSISTENT ? "persistent"
                                                           : "memory");

      KVDistributor distributor(kv_store, config);

      spdlog::info("Using local ring with {} node(s)",
                   distributor.getNodeCount());

      // Background thread to refresh cluster membership every 10s
      std::atomic<bool> membership_active{true};
      std::thread membership_thread([&distributor, &membership_active]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        while(membership_active.load())
          {
            auto endpoints = distributor.getAllEndpoints();
            bool refreshed = false;
            for(const auto &ep : endpoints)
              {
                if(!membership_active.load())
                  break;
                spdlog::info("Refreshing membership from {}", ep);
                if(distributor.fetchMembershipFromServer(ep))
                  {
                    spdlog::info("Membership refreshed, {} nodes in ring",
                                 distributor.getNodeCount());
                    refreshed = true;
                    break;
                  }
                spdlog::warn(
                  "Failed to refresh membership from {}, trying next", ep);
              }
            if(!refreshed)
              spdlog::warn(
                "Could not refresh membership from any known server");
            for(int i = 0; i < 10 && membership_active.load(); ++i)
              std::this_thread::sleep_for(std::chrono::seconds(1));
          }
      });

      httplib::Server svr;
      http_server = &svr;

      svr.Put(R"(/kv/(\d+))", [&distributor](const httplib::Request &req,
                                             httplib::Response &res) {
        try
          {
            int key = std::stoi(req.matches[1]);
            spdlog::info("PUT /kv/{} ({} bytes)", key, req.body.size());
            distributor.insert(key, req.body);
            res.status = 201;
            res.set_content("inserted", "text/plain");
            spdlog::debug("PUT /kv/{} succeeded", key);
          }
        catch(const std::exception &e)
          {
            spdlog::error("PUT /kv/{} failed: {}", req.matches[1].str(),
                          e.what());
            res.status = 500;
            res.set_content(e.what(), "text/plain");
          }
      });

      svr.Get(R"(/kv/(\d+))", [&distributor](const httplib::Request &req,
                                             httplib::Response &res) {
        try
          {
            int key = std::stoi(req.matches[1]);
            spdlog::info("GET /kv/{}", key);
            std::string value = distributor.get(key);
            if(value == "RPC Failed" || value == "key not found")
              {
                res.status = 404;
                res.set_content("not found", "text/plain");
                spdlog::debug("GET /kv/{} -> not found", key);
                return;
              }
            res.set_content(value, "text/plain");
            spdlog::debug("GET /kv/{} succeeded ({} bytes)", key,
                          value.size());
          }
        catch(const std::exception &e)
          {
            spdlog::error("GET /kv/{} failed: {}", req.matches[1].str(),
                          e.what());
            res.status = 500;
            res.set_content(e.what(), "text/plain");
          }
      });

      svr.Post(R"(/kv/(\d+))", [&distributor](const httplib::Request &req,
                                              httplib::Response &res) {
        try
          {
            int key = std::stoi(req.matches[1]);
            spdlog::info("POST /kv/{} ({} bytes)", key, req.body.size());
            distributor.update(key, req.body);
            res.set_content("updated", "text/plain");
            spdlog::debug("POST /kv/{} succeeded", key);
          }
        catch(const std::exception &e)
          {
            spdlog::error("POST /kv/{} failed: {}", req.matches[1].str(),
                          e.what());
            res.status = 500;
            res.set_content(e.what(), "text/plain");
          }
      });

      svr.Delete(R"(/kv/(\d+))", [&distributor](const httplib::Request &req,
                                                httplib::Response &res) {
        try
          {
            int key = std::stoi(req.matches[1]);
            spdlog::info("DELETE /kv/{}", key);
            distributor.deleteKey(key);
            res.status = 204;
            spdlog::debug("DELETE /kv/{} succeeded", key);
          }
        catch(const std::exception &e)
          {
            spdlog::error("DELETE /kv/{} failed: {}", req.matches[1].str(),
                          e.what());
            res.status = 500;
            res.set_content(e.what(), "text/plain");
          }
      });

      svr.Get("/health", [](const httplib::Request &, httplib::Response &res) {
        spdlog::debug("Health check");
        res.set_content("OK", "text/plain");
      });

      spdlog::info("HTTP Gateway listening on 0.0.0.0:{}", http_port);
      std::thread http_thread(
        [&svr, http_port]() { svr.listen("0.0.0.0", http_port); });

      while(!shutdown_flag.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

      spdlog::info("Shutting down HTTP Gateway...");
      membership_active = false;
      if(membership_thread.joinable())
        membership_thread.join();
      svr.stop();
      if(http_thread.joinable())
        http_thread.join();

      kv_store.Sync();
      spdlog::info("HTTP Gateway stopped");
    }
  catch(const std::exception &e)
    {
      std::cerr << "Fatal: " << e.what() << std::endl;
      spdlog::critical("Failed to start: {}", e.what());
      return 1;
    }

  return 0;
}
