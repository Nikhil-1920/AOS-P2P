#ifndef TRACKER_H
#define TRACKER_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>

#define MAX_BUFFER_SIZE 65536
#define MAX_CLIENTS 100

struct User {
    std::string user_id;
    std::string password;
    std::string ip;
    int port;
    bool online;
    std::set<std::string> groups;
};

struct Group {
    std::string group_id;
    std::string owner;
    std::set<std::string> members;
    std::set<std::string> pending_requests;
    std::map<std::string, std::vector<std::string>> shared_files;
};

struct FileEntry {
    std::string filename;
    std::string file_hash;
    std::vector<std::string> piece_hashes;
    long file_size;
    std::string owner;
    std::string group_id;
};

class Tracker {
private:
    int port;
    int tracker_number;
    int server_socket;
    std::vector<std::string> other_trackers;
    std::map<std::string, User> users;
    std::map<std::string, Group> groups;
    std::map<std::string, FileEntry> files;
    std::mutex tracker_mutex;
    bool running;
    
    std::vector<std::string> split_string(const std::string& str, char delimiter);
    std::string process_command(const std::string& command, const std::string& client_ip, int client_port);
    
    std::string handle_create_user(const std::vector<std::string>& tokens);
    std::string handle_login(const std::vector<std::string>& tokens, const std::string& client_ip, int client_port);
    std::string handle_create_group(const std::vector<std::string>& tokens);
    std::string handle_join_group(const std::vector<std::string>& tokens);
    std::string handle_leave_group(const std::vector<std::string>& tokens);
    std::string handle_list_groups(const std::vector<std::string>& tokens);
    std::string handle_list_requests(const std::vector<std::string>& tokens);
    std::string handle_accept_request(const std::vector<std::string>& tokens);
    std::string handle_list_files(const std::vector<std::string>& tokens);
    std::string handle_upload_file(const std::vector<std::string>& tokens);
    std::string handle_download_file(const std::vector<std::string>& tokens);
    std::string handle_logout(const std::vector<std::string>& tokens);
    
public:
    Tracker(int port, int tracker_number);
    ~Tracker();
    
    bool initialize(const std::string& tracker_file);
    void run();
    void handle_client(int client_socket, const std::string& client_ip, int client_port);
};

#endif