#include "KVServer.hpp"
#include "KVStore.hpp"
#include "GossipMembership.hpp"
#include "ConsistentHashRing.hpp"
#include "config.hpp"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

namespace
{
  volatile std::sig_atomic_t shutdown_signal = 0;
  GossipMembership *global_gossip = nullptr;

  void handleShutdownSignal(int signal)
  {
    if(shutdown_signal != 0)
      {
        std::cerr << "\n[Server] Forced shutdown (signal " << signal << ")\n";
        _exit(0);
      }
    shutdown_signal = signal;
  }
}

// Helper function to parse memory size from string with unit suffix
std::size_t parseMemorySize(const std::string &size_str)
{
  const std::size_t KB = 1024;
  const std::size_t MB = 1024 * 1024;
  const std::size_t GB = 1024 * 1024 * 1024;

  std::size_t size = 100 * MB;

  try
    {
      std::string num_part = size_str;
      std::transform(num_part.begin(), num_part.end(), num_part.begin(),
                     ::toupper);

      char unit = 'M';
      double num_value = 0;

      if(num_part.back() == 'K' || num_part.back() == 'M'
         || num_part.back() == 'G')
        {
          unit = num_part.back();
          num_part.pop_back();
          num_value = std::stod(num_part);
        }
      else
        {
          num_value = std::stod(num_part);
        }

      switch(unit)
        {
        case 'K': size = static_cast<std::size_t>(num_value * KB); break;
        case 'M': size = static_cast<std::size_t>(num_value * MB); break;
        case 'G': size = static_cast<std::size_t>(num_value * GB); break;
        default: size = static_cast<std::size_t>(num_value * MB); break;
        }
    }
  catch(const std::exception &e)
    {
      std::cerr << "Error parsing memory size: " << e.what() << "\n";
      std::cerr << "Using default size of 100MB\n";
    }

  return size;
}

int main(int argc, char **argv)
{
  // Default values
  std::string protocol = "ofi+tcp";
  int port = 8080;
  std::size_t mem_size = 100 * 1024 * 1024;
  StorageMode storage_mode = StorageMode::MEMORY;
  std::string config_path = "config/server.json";
  int node_id = 0;

  // Parse --config and --node-id flags and collect positional args
  std::vector<std::string> args;
  for(int i = 1; i < argc; i++)
    {
      std::string arg = argv[i];
      if(arg == "--config" && i + 1 < argc)
        config_path = argv[++i];
      else if(arg == "--node-id" && i + 1 < argc)
        node_id = std::stoi(argv[++i]);
      else
        args.push_back(argv[i]);
    }

  std::cout << "\n=== SERVER STARTUP DEBUG ===\n";
  std::cout << "Config:        " << config_path << "\n";
  std::cout << "Arguments received: " << argc << std::endl;
  for(int i = 0; i < argc; i++)
    {
      std::cout << "argv[" << i << "]: " << argv[i] << std::endl;
    }
  std::cout << "============================\n";

  size_t ai = 0;
  if(args.size() > ai)
    protocol = args[ai++];

  if(args.size() > ai)
    {
      try
        {
          port = std::stoi(args[ai++]);
        }
      catch(...)
        {
          std::cerr << "Invalid port number. Using default: " << port << "\n";
        }
    }

  if(args.size() > ai)
    mem_size = parseMemorySize(args[ai++]);

  if(args.size() > ai)
    {
      std::string mode_arg = args[ai++];
      std::transform(mode_arg.begin(), mode_arg.end(), mode_arg.begin(),
                     ::tolower);

      if(mode_arg == "persistent")
        storage_mode = StorageMode::PERSISTENT;
      else if(mode_arg == "memory")
        storage_mode = StorageMode::MEMORY;
      else
        storage_mode = StorageMode::MEMORY;
    }

  // Get server IP address
  char ip_buffer[128];
  FILE *fp = popen("hostname -I | awk '{print $1}'", "r");
  if(fp)
    {
      fgets(ip_buffer, sizeof(ip_buffer), fp);
      pclose(fp);
    }
  else
    {
      std::cerr << "Warning: could not determine IP, using 127.0.0.1\n";
      snprintf(ip_buffer, sizeof(ip_buffer), "127.0.0.1");
    }

  std::string local_ip(ip_buffer);
  local_ip.erase(std::remove(local_ip.begin(), local_ip.end(), '\n'),
                 local_ip.end());

  std::string address
    = protocol + "://" + local_ip + ":" + std::to_string(port);

  uint16_t provider_id = 1;

  // Print startup configuration
  std::cout << "\n==== KV Server Configuration ====\n";
  std::cout << "Protocol:      " << protocol << "\n";
  std::cout << "Port:          " << port << "\n";
  std::cout << "Shared Memory: " << (mem_size / (1024 * 1024)) << "MB\n";
  std::cout << "Storage Mode:  "
            << (storage_mode == StorageMode::PERSISTENT ? "PERSISTENT"
                                                        : "MEMORY")
            << "\n";
  std::cout << "Address:       " << address << "\n";
  std::cout << "================================\n\n";

  // Read config for gossip settings
  Config config(config_path);

  GossipConfig gossip_cfg = config.read_gossip_config();
  std::cout << "Gossip interval: " << gossip_cfg.interval_ms << "ms\n";
  std::cout << "Suspect threshold: " << gossip_cfg.suspect_threshold_ms
            << "ms\n";
  std::cout << "Dead threshold: " << gossip_cfg.dead_threshold_ms << "ms\n";
  if(!gossip_cfg.seed_nodes.empty())
    {
      std::cout << "Seed nodes:\n";
      for(const auto &s : gossip_cfg.seed_nodes)
        std::cout << "  " << s << "\n";
    }

  // Start the Thallium engine
  tl::engine myEngine(address, THALLIUM_SERVER_MODE);
  std::cout << "Server running at " << myEngine.self() << std::endl;

  std::signal(SIGINT, handleShutdownSignal);
  std::signal(SIGTERM, handleShutdownSignal);

  KvStore &kv = KvStore::get_instance(
    mem_size, storage_mode, ConnectionMode::SERVER, std::to_string(port));
  std::cout << "KvStore initialized successfully" << std::endl;

  // Create ConsistentHashRing that will be rebuilt on membership changes
  auto server_ring = std::make_shared<ConsistentHashRing>();

  // Create GossipMembership
  GossipMembership membership(node_id, address, myEngine, provider_id,
                              gossip_cfg.seed_nodes, gossip_cfg.interval_ms,
                              gossip_cfg.suspect_threshold_ms,
                              gossip_cfg.dead_threshold_ms);
  global_gossip = &membership;

  // Register callback to rebuild ring when membership changes
  membership.setChangeCallback(
    [server_ring](const std::unordered_map<int, std::string> &live) -> void {
      std::cout << "[Gossip] Ring rebuild triggered, " << live.size()
                << " live nodes\n";
      server_ring->rebuild(live);
      for(const auto &[id, ep] : live)
        std::cout << "  Node " << id << " -> " << ep << "\n";

      auto ranges = server_ring->getKeyRanges();
      for(const auto &[node_id, node_ranges] : ranges)
        {
          std::cout << "  Node " << node_id << " key ranges:\n";
          for(const auto &kr : node_ranges)
            std::cout << "    [0x" << std::hex << kr.start << std::dec
                      << ", 0x" << std::hex << kr.end << std::dec << ")\n";
        }
    });

  // Create and start KVServer (with membership pointer for gossip RPC)
  KVServer server(myEngine, kv, provider_id, &membership);
  std::cout << "KVServer started with provider ID: " << provider_id
            << std::endl;

  // Start gossip membership (announce self, begin periodic gossip)
  membership.start();
  std::cout << "Gossip membership started\n";

  std::cout << "\n=== SERVER READY ===\n";
  std::cout << "Node ID:       " << node_id << "\n";
  std::cout << "Endpoint:      " << address << "\n";
  std::cout << "Storage:       "
            << (storage_mode == StorageMode::PERSISTENT ? "PERSISTENT"
                                                        : "MEMORY")
            << "\n";
  std::cout << "Membership:    Gossip (interval=" << gossip_cfg.interval_ms
            << "ms)\n";
  std::cout << "===================\n\n";

  // Register prefinalize callback so gossip stops if finalize is triggered
  // externally (e.g. remote shutdown)
  myEngine.push_prefinalize_callback([&]() -> void {
    if(shutdown_signal == 0)
      shutdown_signal = 1;
  });

  // Wait for shutdown signal in a watcher thread
  std::thread shutdown_watcher([&]() -> void {
    while(shutdown_signal == 0)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }

    std::cout << "\n[Server] Received signal " << shutdown_signal
              << ", starting clean shutdown...\n";

    // Announce departure via gossip before stopping
    membership.stop();
    global_gossip = nullptr;
    std::cout << "[Server] Gossip membership stopped\n";

    std::cout << "[Server] Flushing store and releasing resources...\n";
    kv.Sync();
    std::cout << "[Server] Finalizing Thallium engine...\n";
    myEngine.finalize();
  });

  std::cout << "\n[Server] Ready. Press CTRL+C to shut down cleanly.\n";

  myEngine.wait_for_finalize();
  if(shutdown_watcher.joinable())
    shutdown_watcher.join();
  std::cout << "[Server] Shutdown complete.\n";
  return 0;
}
