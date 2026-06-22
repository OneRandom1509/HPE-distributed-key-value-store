#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <httplib.h>

static std::vector<std::string> parseCommand(const std::string &cmd)
{
  std::vector<std::string> args;
  std::istringstream iss(cmd);
  std::string arg;
  while(iss >> arg)
    args.push_back(arg);
  return args;
}

static void printHelp()
{
  std::cout << "\nHTTP KV Client Commands:" << std::endl;
  std::cout << "  put <key> <value>     - Store a key-value pair" << std::endl;
  std::cout << "  get <key>             - Get a value for a key" << std::endl;
  std::cout << "  update <key> <value>  - Update a key-value pair"
            << std::endl;
  std::cout << "  delete <key>          - Delete a key-value pair"
            << std::endl;
  std::cout << "  health                - Check gateway health" << std::endl;
  std::cout << "  help                  - Show this help" << std::endl;
  std::cout << "  exit                  - Exit" << std::endl;
}

int main(int argc, char **argv)
{
  std::string host = "127.0.0.1";
  int port = 9090;

  for(int i = 1; i < argc; ++i)
    {
      std::string arg = argv[i];
      if(arg == "--host" && i + 1 < argc)
        host = argv[++i];
      else if(arg == "--port" && i + 1 < argc)
        port = std::stoi(argv[++i]);
      else if(arg == "--help")
        {
          std::cout << "Usage: " << argv[0]
                    << " [--host <addr>] [--port <port>]" << std::endl;
          return 0;
        }
    }

  httplib::Client cli(host, port);

  auto health_res = cli.Get("/health");
  if(!health_res || health_res->status != 200)
    {
      std::cerr << "Cannot reach HTTP gateway at " << host << ":" << port
                << std::endl;
      return 1;
    }

  std::cout << "Connected to KV gateway at " << host << ":" << port
            << std::endl;
  printHelp();

  while(true)
    {
      std::string input;
      std::cout << "\n> ";
      std::getline(std::cin, input);
      if(input.empty())
        continue;

      auto args = parseCommand(input);
      const auto &action = args[0];

      if(action == "exit")
        break;

      if(action == "help")
        {
          printHelp();
          continue;
        }

      if(action == "health")
        {
          auto res = cli.Get("/health");
          std::cout << (res && res->status == 200 ? "Gateway is healthy"
                                                  : "Gateway unreachable")
                    << std::endl;
          continue;
        }

      if(action == "get" && args.size() >= 2)
        {
          auto res = cli.Get("/kv/" + args[1]);
          if(res && res->status == 200)
            std::cout << "Value: " << res->body << std::endl;
          else
            std::cout << "Key not found" << std::endl;
          continue;
        }

      if(action == "put" && args.size() >= 3)
        {
          std::string value = args[2];
          for(size_t i = 3; i < args.size(); ++i)
            value += " " + args[i];
          auto res = cli.Put("/kv/" + args[1], value, "text/plain");
          std::cout << (res && res->status == 201 ? "Inserted"
                                                  : "Insert failed")
                    << std::endl;
          continue;
        }

      if(action == "update" && args.size() >= 3)
        {
          std::string value = args[2];
          for(size_t i = 3; i < args.size(); ++i)
            value += " " + args[i];
          auto res = cli.Post("/kv/" + args[1], value, "text/plain");
          std::cout << (res && res->status == 200 ? "Updated"
                                                  : "Update failed")
                    << std::endl;
          continue;
        }

      if(action == "delete" && args.size() >= 2)
        {
          auto res = cli.Delete("/kv/" + args[1]);
          std::cout << (res && res->status == 204 ? "Deleted"
                                                  : "Delete failed")
                    << std::endl;
          continue;
        }

      std::cout << "Unknown command. Type 'help'." << std::endl;
    }

  std::cout << "Goodbye!" << std::endl;
  return 0;
}
