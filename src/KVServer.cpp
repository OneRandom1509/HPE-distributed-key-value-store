#include "KVServer.hpp"
#include <thallium/serialization/stl/string.hpp>
#include <stdexcept>
#include <chrono>
#include "KVStore.hpp"
KVServer::KVServer(tl::engine &e, KvStore& kv_ref, uint16_t provider_id)
    : tl::provider<KVServer>(e, provider_id),
      kv(kv_ref)
{
    define("kv_fetch", &KVServer::kv_fetch);
    define("kv_insert", &KVServer::kv_insert);
    define("kv_update", &KVServer::kv_update);
    define("kv_delete", &KVServer::kv_delete);  // Register delete method
}
void KVServer::kv_fetch(const tl::request& req, int key) {
    auto start = std::chrono::high_resolution_clock::now();  // Start timing
    std::cout << "[Fetch] key=" << key << std::endl;
    try {
        std::string value = kv.Find(key);
        // Calculate and print elapsed time
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << "[Fetch] Key: " << key << ", Value: " << value << std::endl;
        std::cout << "[Fetch] Server-side operation completed in " << elapsed.count() << " ms" << std::endl;
        req.respond(value);
    } catch (const std::exception& e) {
        std::cerr << "[Fetch Error] " << e.what() << std::endl;
        req.respond();
    }
}
void KVServer::kv_insert(const tl::request& req, int key, std::string value) {
    auto start = std::chrono::high_resolution_clock::now();  // Start timing
    std::cout << "[Insert] " << key << " -> " << value << std::endl;
    try {
        kv.Insert(key, value);
        // Calculate and print elapsed time
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << "[Insert] Server-side operation completed in " << elapsed.count() << " ms" << std::endl;
        req.respond(1);
    } catch (const std::exception& e) {
        std::cerr << "[Insert Error] " << e.what() << std::endl;
        req.respond(0);
    }
}
void KVServer::kv_update(const tl::request& req, int key, std::string value) {
    auto start = std::chrono::high_resolution_clock::now();  // Start timing
    std::cout << "[Update] " << key << " -> " << value << std::endl;
    try {
        kv.Update(key, value);
        // Calculate and print elapsed time
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << "[Update] Server-side operation completed in " << elapsed.count() << " ms" << std::endl;
        req.respond(1);
    } catch (const std::exception& e) {
        std::cerr << "[Update Error] " << e.what() << std::endl;
        req.respond(0);
    }
}
// Add delete method implementation
void KVServer::kv_delete(const tl::request& req, int key) {
    auto start = std::chrono::high_resolution_clock::now();  // Start timing
    std::cout << "[Delete] key=" << key << std::endl;
    try {
        kv.Delete(key);
        // Calculate and print elapsed time
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << "[Delete] Server-side operation completed in " << elapsed.count() << " ms" << std::endl;
        req.respond(1);
    } catch (const std::exception& e) {
        std::cerr << "[Delete Error] " << e.what() << std::endl;
        req.respond(0);
    }
}
