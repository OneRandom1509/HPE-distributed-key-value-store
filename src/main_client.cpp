#include "KVClient.hpp"
#include "KVDistributor.hpp"
#include "KVStore.hpp"
#include "config.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <vector>
#include <spdlog/spdlog.h>

// Helper function to parse command arguments
std::vector<std::string> parseCommand(const std::string &command)
{
  std::vector<std::string> args;
  std::istringstream iss(command);
  std::string arg;
  while(iss >> arg)
    {
      args.push_back(arg);
    }
  return args;
}

// Help function
void printHelp()
{
  std::cout << "\nDistributed Key-Value Store Commands:" << std::endl;
  std::cout << "====================================" << std::endl;
  std::cout << "  put <key> <value>        - Store a key-value pair"
            << std::endl;
  std::cout << "  get <key>                - Get a value for a key"
            << std::endl;
  std::cout << "  update <key> <value>     - Update an existing key-value pair"
            << std::endl;
  std::cout << "  delete <key>             - Delete a key-value pair"
            << std::endl;
  std::cout
    << "  benchmark                - To run with sequential fetch pattern"
    << std::endl;
  std::cout
    << "  benchmark1               - Run benchmark with random fetch pattern"
    << std::endl;
  std::cout << "  help                     - Show this help message"
            << std::endl;
  std::cout << "  exit                     - Exit the program" << std::endl;
}

// Add these function declarations after the printHelp() function and before
// main()

// Function to generate a random string of specified length
std::string generateRandomString(int length)
{
  const std::string charset
    = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, charset.size() - 1);

  std::string result;
  result.reserve(length);
  for(int i = 0; i < length; ++i)
    {
      result += charset[dis(gen)];
    }
  return result;
}

// Benchmark function with sequential fetch pattern
void benchmark(KVDistributor &distributor)
{
  const int NUM_OPERATIONS = 10000;
  const int VALUE_SIZE = 3;

  std::cout << "\n=== BENCHMARK (Sequential Fetch Pattern) ===" << std::endl;
  std::cout << "Inserting " << NUM_OPERATIONS << " key-value pairs..."
            << std::endl;

  // Phase 1: Sequential Insertion
  auto start_insert = std::chrono::high_resolution_clock::now();
  for(int i = 1; i <= NUM_OPERATIONS; ++i)
    {
      try
        {
          std::string value = generateRandomString(VALUE_SIZE);
          distributor.insert(i, value);
        }
      catch(const std::exception &e)
        {
          std::cout << "Insert error for key " << i << ": " << e.what()
                    << std::endl;
        }
    }
  auto end_insert = std::chrono::high_resolution_clock::now();
  auto insert_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    end_insert - start_insert);
  std::cout << "Insertion completed!" << std::endl;

  // Phase 2: Sequential Fetch
  std::cout << "\nFetching " << NUM_OPERATIONS
            << " key-value pairs sequentially..." << std::endl;
  std::vector<double> fetch_times;
  fetch_times.reserve(NUM_OPERATIONS);

  auto start_fetch_all = std::chrono::high_resolution_clock::now();
  for(int i = 1; i <= NUM_OPERATIONS; ++i)
    {
      try
        {
          auto start_single_fetch = std::chrono::high_resolution_clock::now();
          std::string value = distributor.get(i);
          auto end_single_fetch = std::chrono::high_resolution_clock::now();

          auto single_fetch_duration
            = std::chrono::duration_cast<std::chrono::microseconds>(
              end_single_fetch - start_single_fetch);
          double fetch_time_ms
            = static_cast<double>(single_fetch_duration.count()) / 1000.0;

          fetch_times.push_back(fetch_time_ms);
        }
      catch(const std::exception &e)
        {
          std::cout << "Fetch error for key " << i << ": " << e.what()
                    << std::endl;
        }
    }
  auto end_fetch_all = std::chrono::high_resolution_clock::now();
  auto total_fetch_duration
    = std::chrono::duration_cast<std::chrono::milliseconds>(end_fetch_all
                                                            - start_fetch_all);

  // Calculate overall statistics
  if(!fetch_times.empty())
    {
      double total_time
        = std::accumulate(fetch_times.begin(), fetch_times.end(), 0.0);
      double avg_time = total_time / fetch_times.size();
      double min_time
        = *std::min_element(fetch_times.begin(), fetch_times.end());
      double max_time
        = *std::max_element(fetch_times.begin(), fetch_times.end());

      std::cout << "\n=== SEQUENTIAL BENCHMARK RESULTS ===" << std::endl;
      std::cout << std::fixed << std::setprecision(4);
      std::cout << "--- INSERTION METRICS ---" << std::endl;
      std::cout << "Total insertion time: " << insert_duration.count() << " ms"
                << std::endl;
      std::cout << "Average insertion time per operation: "
                << static_cast<double>(insert_duration.count())
                     / NUM_OPERATIONS
                << " ms" << std::endl;

      std::cout << "--- OVERALL FETCH METRICS ---" << std::endl;
      std::cout << "Total fetch time: " << total_fetch_duration.count()
                << " ms" << std::endl;
      std::cout << "Average fetch time: " << avg_time << " ms" << std::endl;
      std::cout << "Minimum fetch time: " << min_time << " ms" << std::endl;
      std::cout << "Maximum fetch time: " << max_time << " ms" << std::endl;
      std::cout << "Successful fetches: " << fetch_times.size() << "/"
                << NUM_OPERATIONS << std::endl;
    }
}

// Benchmark function with random fetch pattern
void benchmark1(KVDistributor &distributor)
{
  const int NUM_OPERATIONS = 10000;
  const int VALUE_SIZE = 3;

  std::cout << "\n=== BENCHMARK1 (Random Fetch Pattern) ===" << std::endl;
  std::cout << "Inserting " << NUM_OPERATIONS << " key-value pairs..."
            << std::endl;

  // Phase 1: Sequential Insertion (same as benchmark)
  auto start_insert = std::chrono::high_resolution_clock::now();
  for(int i = 1; i <= NUM_OPERATIONS; ++i)
    {
      try
        {
          std::string value = generateRandomString(VALUE_SIZE);
          distributor.insert(i, value);
        }
      catch(const std::exception &e)
        {
          std::cout << "Insert error for key " << i << ": " << e.what()
                    << std::endl;
        }
    }
  auto end_insert = std::chrono::high_resolution_clock::now();
  auto insert_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    end_insert - start_insert);
  std::cout << "Insertion completed!" << std::endl;

  // Phase 2: Random Fetch
  std::cout << "\nFetching " << NUM_OPERATIONS
            << " key-value pairs randomly..." << std::endl;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> key_dist(1, NUM_OPERATIONS);

  std::vector<double> fetch_times;
  fetch_times.reserve(NUM_OPERATIONS);

  auto start_fetch_all = std::chrono::high_resolution_clock::now();
  for(int i = 0; i < NUM_OPERATIONS; ++i)
    {
      try
        {
          int random_key = key_dist(gen);

          auto start_single_fetch = std::chrono::high_resolution_clock::now();
          std::string value = distributor.get(random_key);
          auto end_single_fetch = std::chrono::high_resolution_clock::now();

          auto single_fetch_duration
            = std::chrono::duration_cast<std::chrono::microseconds>(
              end_single_fetch - start_single_fetch);
          double fetch_time_ms
            = static_cast<double>(single_fetch_duration.count()) / 1000.0;

          fetch_times.push_back(fetch_time_ms);
        }
      catch(const std::exception &e)
        {
          std::cout << "Fetch error for random key: " << e.what() << std::endl;
        }
    }
  auto end_fetch_all = std::chrono::high_resolution_clock::now();
  auto total_fetch_duration
    = std::chrono::duration_cast<std::chrono::milliseconds>(end_fetch_all
                                                            - start_fetch_all);

  if(!fetch_times.empty())
    {
      double total_time
        = std::accumulate(fetch_times.begin(), fetch_times.end(), 0.0);
      double avg_time = total_time / fetch_times.size();
      double min_time
        = *std::min_element(fetch_times.begin(), fetch_times.end());
      double max_time
        = *std::max_element(fetch_times.begin(), fetch_times.end());

      std::cout << "\n=== RANDOM BENCHMARK RESULTS ===" << std::endl;
      std::cout << std::fixed << std::setprecision(4);
      std::cout << "Total insertion time: " << insert_duration.count() << " ms"
                << std::endl;
      std::cout << "Average insertion time per operation: "
                << static_cast<double>(insert_duration.count())
                     / NUM_OPERATIONS
                << " ms" << std::endl;
      std::cout << "Total fetch time: " << total_fetch_duration.count()
                << " ms" << std::endl;
      std::cout << "Average fetch time: " << avg_time << " ms" << std::endl;
      std::cout << "Minimum fetch time: " << min_time << " ms" << std::endl;
      std::cout << "Maximum fetch time: " << max_time << " ms" << std::endl;
      std::cout << "Successful fetches: " << fetch_times.size() << "/"
                << NUM_OPERATIONS << std::endl;
    }
}

int main(int argc, char **argv)
{
  // Initialize logging level from environment
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
    {
      spdlog::set_level(spdlog::level::info);
    }

  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
  spdlog::flush_on(spdlog::level::info);

  std::cout << "\nModulo-based Key-Value Store CLIENT" << std::endl;
  std::cout << "===================================" << std::endl;

  std::string mode;
  std::cout << "Enter server storage mode (memory/persistent): ";
  std::getline(std::cin, mode);
  std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

  StorageMode storage_mode;
  if(mode == "persistent")
    {
      storage_mode = StorageMode::PERSISTENT;
    }
  else if(mode == "memory")
    {
      storage_mode = StorageMode::MEMORY;
    }
  else
    {
      std::cout << "Invalid storage mode. Defaulting to memory mode."
                << std::endl;
      storage_mode = StorageMode::MEMORY;
    }

  Config config = Config("config/config.json");
  size_t mem_size = config.read_size();

  try
    {
      // CLIENT mode - connect to existing storage instead of creating new
      // Determine node_tag from configured first endpoint port so the client
      // connects to the same per-node shared memory the server created.
      Config config = Config("config/config.json");
      std::string first_ep = config.get_endpoint(0);
      std::string node_tag = "";
      auto pos = first_ep.rfind(':');
      if(pos != std::string::npos && pos + 1 < first_ep.size())
        node_tag = first_ep.substr(pos + 1);

      KvStore &kv_store = KvStore::get_instance(
        mem_size, storage_mode, ConnectionMode::CLIENT, node_tag);

      std::cout << "Storage Mode: "
                << (storage_mode == StorageMode::PERSISTENT ? "PERSISTENT"
                                                            : "IN-MEMORY")
                << std::endl;
      std::cout << "Connected to existing server storage successfully!"
                << std::endl;

      // Pass the local_store to ThalliumDistributor
      KVDistributor distributor(kv_store, config);

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

      printHelp();

      while(true)
        {
          std::string input;
          std::cout << "\n> ";
          std::getline(std::cin, input);

          if(input.empty())
            continue;

          std::vector<std::string> args = parseCommand(input);
          std::string action = args[0];

          if(action == "exit")
            {
              break;
            }
          else if(action == "help")
            {
              printHelp();
            }
          else if(action == "put" && args.size() >= 3)
            {
              try
                {
                  int key = std::stoi(args[1]);
                  std::string value = args[2];
                  for(size_t i = 3; i < args.size(); i++)
                    {
                      value += " " + args[i];
                    }
                  distributor.insert(key, value);
                  std::cout << "Put operation completed successfully"
                            << std::endl;
                }
              catch(const std::exception &e)
                {
                  std::cout << "Error: " << e.what() << std::endl;
                }
            }
          else if(action == "get" && args.size() >= 2)
            {
              try
                {
                  int key = std::stoi(args[1]);
                  std::string value = distributor.get(key);
                  std::cout << "Value: " << value << std::endl;
                }
              catch(const std::exception &e)
                {
                  std::cout << "Error: " << e.what() << std::endl;
                }
            }
          else if(action == "update" && args.size() >= 3)
            {
              try
                {
                  int key = std::stoi(args[1]);
                  std::string value = args[2];
                  for(size_t i = 3; i < args.size(); i++)
                    {
                      value += " " + args[i];
                    }
                  distributor.update(key, value);
                  std::cout << "Update operation completed successfully"
                            << std::endl;
                }
              catch(const std::exception &e)
                {
                  std::cout << "Error: " << e.what() << std::endl;
                }
            }
          else if(action == "delete" && args.size() >= 2)
            {
              try
                {
                  int key = std::stoi(args[1]);
                  distributor.deleteKey(key);
                  std::cout << "Delete operation completed successfully"
                            << std::endl;
                }
              catch(const std::exception &e)
                {
                  std::cout << "Error: " << e.what() << std::endl;
                }
            }
          else if(action == "benchmark")
            {
              std::cout << "Starting sequential benchmark..." << std::endl;
              try
                {
                  benchmark(distributor);
                }
              catch(const std::exception &e)
                {
                  std::cout << "Benchmark error: " << e.what() << std::endl;
                }
            }
          else if(action == "benchmark1")
            {
              std::cout << "Starting random benchmark..." << std::endl;
              try
                {
                  benchmark1(distributor);
                }
              catch(const std::exception &e)
                {
                  std::cout << "Benchmark1 error: " << e.what() << std::endl;
                }
            }
          else
            {
              std::cout
                << "Unknown command. Type 'help' for available commands."
                << std::endl;
            }
        }
      membership_active = false;
      if(membership_thread.joinable())
        membership_thread.join();
    }
  catch(const std::exception &e)
    {
      std::cout << "Failed to connect to server storage: " << e.what()
                << std::endl;
      std::cout
        << "Make sure the server is running before starting the client."
        << std::endl;
      return 1;
    }

  std::cout << "Goodbye!" << std::endl;
  return 0;
}
