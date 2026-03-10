#include "KVDistributor.hpp"
#include <iostream>

KVDistributor::KVDistributor(KvStore& kv_store, const Config& config)
    : kv(kv_store),
      config(config),  // bind config reference
      count_of_node(config.read_count()),
      protocol(config.read_protocol()),
      provider_id(1),
      kv_client(protocol, 1) {
     //std::cout<<protocol<<"  "<<provider_id<<'\n';
    for (int i = 0; i < count_of_node; ++i) {
        std::string endpoint = config.get_endpoint(i);
        node_to_ip[i] = endpoint;
    }

    local_node_id = getLocalNodeId();
}

int KVDistributor::getNodeId(int key) {
    return key % count_of_node;
}

std::string KVDistributor::getNodeToIP(int node_id) {
    return node_to_ip[node_id];
}

int KVDistributor::getLocalNodeId() {
    std::string local_ip = config.read_ip();
    for (const auto& pair : node_to_ip) {
        if (pair.second == local_ip) {
            return pair.first;
        }
    }
    return 0;
}

int KVDistributor::getNodeCount() {
    return count_of_node;
}

std::string KVDistributor::get(int key) {
    int node_id = getNodeId(key);
    //std::cout<<"get: "<<node_id<<"    "<<local_node_id<<std::endl;
    std::string value;
    if(node_id == local_node_id) {
        value = kv.Find(key);
    } else {
        try {
	    std::string ep = node_to_ip[node_id];
            value = kv_client.fetch(key, ep);
	    //std::cout<<ep<<" Hello"<<'\n';
        } catch (const std::exception& e2) {
            std::cerr << "Error fetching key " << key << ": " << e2.what() << std::endl;
            return "RPC Failed";
        }
    }
    return value;
}

void KVDistributor::insert(int key, const std::string& value) {
    int node_id = getNodeId(key);
    //std::cout<<"insert: "<<node_id<<"    "<<local_node_id<<std::endl;
    if (node_id == local_node_id) {
        try {
            kv.Insert(key, value);
        } catch (const std::exception& e) {
            std::cerr << "Error inserting key " << key << ": " << e.what() << std::endl;
        }
    } else {
        try {
	   std::string ep = node_to_ip[node_id];
	   //std::cout<<ep<<" Hello"<<'\n';
            kv_client.insert(key, value, ep);
        } catch (const std::exception& e) {
            std::cerr << "Error inserting key " << key << " via RPC: " << e.what() << std::endl;
        }
    }
}

void KVDistributor::update(int key, const std::string& value) {
    int node_id = getNodeId(key);
    if (node_id == local_node_id) {
        try {
            kv.Update(key, value);
        } catch (const std::exception& e) {
            std::cerr << "Error updating key " << key << ": " << e.what() << std::endl;
        }
    } else {
        try {
            kv_client.update(key, value, node_to_ip[node_id]);
        } catch (const std::exception& e) {
            std::cerr << "Error updating key " << key << " via RPC: " << e.what() << std::endl;
        }
    }
}

void KVDistributor::deleteKey(int key) {
    int node_id = getNodeId(key);
    if (node_id == local_node_id) {
        try {
            kv.Delete(key);
        } catch (const std::exception& e) {
            std::cerr << "Error deleting key " << key << ": " << e.what() << std::endl;
        }
    } else {
        try {
            kv_client.deleteKey(key, node_to_ip[node_id]);
        } catch (const std::exception& e) {
            std::cerr << "Error deleting key " << key << " via RPC: " << e.what() << std::endl;
        }
    }
}
