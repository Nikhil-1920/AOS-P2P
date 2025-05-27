#include "tracker.h"
#include <iomanip>  

#define RESET   "\033[0m"
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"
#define MAGENTA "\033[35m"

Tracker::Tracker(int port, int tracker_number)
    : port(port), tracker_number(tracker_number), server_socket(-1), running(false) {
    signal(SIGPIPE, SIG_IGN);
}

Tracker::~Tracker() {
    running = false;
    if (server_socket != -1) {
        close(server_socket);
    }
}

bool Tracker::initialize(const std::string& tracker_file) {
    std::ifstream file(tracker_file);
    if (!file.is_open()) {
        std::cerr << RED << "Failed to open tracker info file: " << tracker_file << RESET << std::endl;
        return false;
    }
    
    std::string line;
    int current_tracker = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && current_tracker != tracker_number) {
            other_trackers.push_back(line);
        }
        current_tracker++;
    }
    file.close();
    
    std::cout << GREEN << "✓ Tracker " << tracker_number << " initialized on port " << port << RESET << std::endl;
    std::cout << BLUE << "ℹ Found " << other_trackers.size() << " other tracker(s)" << RESET << std::endl;
    return true;
}

void Tracker::run() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << RED << "Failed to create server socket" << RESET << std::endl;
        return;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << RED << "Failed to bind to port " << port << RESET << std::endl;
        close(server_socket);
        return;
    }
    
    if (listen(server_socket, 10) < 0) {
        std::cerr << RED << "Failed to listen on socket" << RESET << std::endl;
        close(server_socket);
        return;
    }
    
    running = true;
    std::cout << BOLD << CYAN << "🚀 Tracker " << tracker_number << " running on port " << port << RESET << std::endl;
    std::cout << YELLOW << "💾 Ready to handle large file uploads (20GB+)" << RESET << std::endl;
    std::cout << YELLOW << "📡 Waiting for client connections..." << RESET << std::endl;
    
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket >= 0) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);
            
            std::cout << GREEN << "📞 New client connected: " << client_ip << ":" << client_port << RESET << std::endl;
            
            std::thread client_thread(&Tracker::handle_client, this, client_socket, std::string(client_ip), client_port);
            client_thread.detach();
        }
    }
}

void Tracker::handle_client(int client_socket, const std::string& client_ip, int client_port) {
    const int LARGE_BUFFER_SIZE = 65536;            // 64KB buffer
    char* buffer = new char[LARGE_BUFFER_SIZE];
    
    try {
        while (true) {
            ssize_t bytes_received = recv(client_socket, buffer, LARGE_BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) break;
            
            buffer[bytes_received] = '\0';
            std::string command(buffer);
            
            // Remove trailing newline
            if (!command.empty() && command.back() == '\n') {
                command.pop_back();
            }
            
            // Log command (truncated for large commands)
            std::string log_command = command.length() > 100 ? 
                command.substr(0, 100) + "... [" + std::to_string(command.length()) + " chars]" : command;
            std::cout << BLUE << "📨 Command from " << client_ip << ": " << log_command << RESET << std::endl;
            
            std::string response = process_command(command, client_ip, client_port);
            
            if (send(client_socket, response.c_str(), response.length(), 0) <= 0) {
                break;
            }
            
            std::string log_response = response.length() > 50 ? 
                response.substr(0, 50) + "... [" + std::to_string(response.length()) + " chars]" : response;
            std::cout << GREEN << "📤 Response sent: " << log_response << RESET << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << RED << "❌ Error handling client " << client_ip << ": " << e.what() << RESET << std::endl;
    }
    
    std::cout << YELLOW << "📞 Client disconnected: " << client_ip << ":" << client_port << RESET << std::endl;
    delete[] buffer;
    close(client_socket);
}

std::vector<std::string> Tracker::split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string Tracker::process_command(const std::string& command, const std::string& client_ip, int client_port) {
    std::vector<std::string> tokens = split_string(command, ' ');
    if (tokens.empty()) return "ERROR: Empty command\n";
    
    std::lock_guard<std::mutex> lock(tracker_mutex);
    
    if (tokens[0] == "CREATE_USER") {
        return handle_create_user(tokens);
    } else if (tokens[0] == "LOGIN") {
        return handle_login(tokens, client_ip, client_port);
    } else if (tokens[0] == "CREATE_GROUP") {
        return handle_create_group(tokens);
    } else if (tokens[0] == "JOIN_GROUP") {
        return handle_join_group(tokens);
    } else if (tokens[0] == "LEAVE_GROUP") {
        return handle_leave_group(tokens);
    } else if (tokens[0] == "LIST_GROUPS") {
        return handle_list_groups(tokens);
    } else if (tokens[0] == "LIST_REQUESTS") {
        return handle_list_requests(tokens);
    } else if (tokens[0] == "ACCEPT_REQUEST") {
        return handle_accept_request(tokens);
    } else if (tokens[0] == "LIST_FILES") {
        return handle_list_files(tokens);
    } else if (tokens[0] == "UPLOAD_FILE") {
        return handle_upload_file(tokens);
    } else if (tokens[0] == "DOWNLOAD_FILE") {
        return handle_download_file(tokens);
    } else if (tokens[0] == "LOGOUT") {
        return handle_logout(tokens);
    }
    
    return "ERROR: Unknown command\n";
}

std::string Tracker::handle_create_user(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        return "ERROR: Invalid CREATE_USER command\n";
    }
    
    std::string user_id = tokens[1];
    std::string password = tokens[2];
    
    if (users.find(user_id) != users.end()) {
        return "ERROR: User already exists\n";
    }
    
    User user;
    user.user_id = user_id;
    user.password = password;
    user.online = false;
    users[user_id] = user;
    
    std::cout << GREEN << "✓ User created: " << user_id << RESET << std::endl;
    return "SUCCESS: User created\n";
}

std::string Tracker::handle_login(const std::vector<std::string>& tokens, const std::string& client_ip, int client_port) {
    (void)client_ip;
    (void)client_port;
    
    if (tokens.size() < 5) {
        return "ERROR: Invalid LOGIN command\n";
    }
    
    std::string user_id = tokens[1];
    std::string password = tokens[2];
    std::string ip = tokens[3];
    int port = std::stoi(tokens[4]);
    
    std::cout << BLUE << "📝 Login attempt: " << user_id << " from " << ip << ":" << port << RESET << std::endl;
    
    if (users.find(user_id) == users.end()) {
        return "ERROR: User not found\n";
    }
    
    if (users[user_id].password != password) {
        return "ERROR: Invalid password\n";
    }
    
    users[user_id].online = true;
    users[user_id].ip = ip;
    users[user_id].port = port;
    
    std::cout << GREEN << "✓ " << user_id << " logged in successfully at " << ip << ":" << port << RESET << std::endl;
    return "SUCCESS: Login successful\n";
}

std::string Tracker::handle_create_group(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        return "ERROR: Invalid CREATE_GROUP command\n";
    }
    
    std::string user_id = tokens[1];
    std::string group_id = tokens[2];
    
    if (users.find(user_id) == users.end() || !users[user_id].online) {
        return "ERROR: User not logged in\n";
    }
    
    if (groups.find(group_id) != groups.end()) {
        return "ERROR: Group already exists\n";
    }
    
    Group group;
    group.group_id = group_id;
    group.owner = user_id;
    group.members.insert(user_id);
    groups[group_id] = group;
    users[user_id].groups.insert(group_id);
    
    std::cout << GREEN << "✓ Group created: " << group_id << " by " << user_id << RESET << std::endl;
    return "SUCCESS: Group created\n";
}

std::string Tracker::handle_join_group(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        return "ERROR: Invalid JOIN_GROUP command\n";
    }
    
    std::string user_id = tokens[1];
    std::string group_id = tokens[2];
    
    if (users.find(user_id) == users.end() || !users[user_id].online) {
        return "ERROR: User not logged in\n";
    }
    
    if (groups.find(group_id) == groups.end()) {
        return "ERROR: Group not found\n";
    }
    
    if (groups[group_id].members.find(user_id) != groups[group_id].members.end()) {
        return "ERROR: Already a member\n";
    }
    
    groups[group_id].pending_requests.insert(user_id);
    return "SUCCESS: Join request sent\n";
}

std::string Tracker::handle_leave_group(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        return "ERROR: Invalid LEAVE_GROUP command\n";
    }
    
    std::string user_id = tokens[1];
    std::string group_id = tokens[2];
    
    if (users.find(user_id) == users.end() || !users[user_id].online) {
        return "ERROR: User not logged in\n";
    }
    
    if (groups.find(group_id) == groups.end()) {
        return "ERROR: Group not found\n";
    }
    
    if (groups[group_id].members.find(user_id) == groups[group_id].members.end()) {
        return "ERROR: Not a member\n";
    }
    
    groups[group_id].members.erase(user_id);
    users[user_id].groups.erase(group_id);
    
    if (groups[group_id].owner == user_id && !groups[group_id].members.empty()) {
        groups[group_id].owner = *groups[group_id].members.begin();
    }
    
    return "SUCCESS: Left group\n";
}

std::string Tracker::handle_list_groups(const std::vector<std::string>& tokens) {
    (void)tokens;
    
    if (groups.empty()) {
        return "No groups available\n";
    }
    
    std::string result;
    for (const auto& group : groups) {
        result += group.first + " (Owner: " + group.second.owner +
                  ", Members: " + std::to_string(group.second.members.size()) + ")\n";
    }
    return result;
}

std::string Tracker::handle_list_requests(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        return "ERROR: Invalid LIST_REQUESTS command\n";
    }
    
    std::string user_id = tokens[1];
    std::string group_id = tokens[2];
    
    if (users.find(user_id) == users.end() || !users[user_id].online) {
        return "ERROR: User not logged in\n";
    }
    
    if (groups.find(group_id) == groups.end()) {
        return "ERROR: Group not found\n";
    }
    
    if (groups[group_id].owner != user_id) {
        return "ERROR: Not group owner\n";
    }
    
    if (groups[group_id].pending_requests.empty()) {
        return "No pending requests\n";
    }
    
    std::string result;
    for (const auto& request : groups[group_id].pending_requests) {
        result += request + "\n";
    }
    return result;
}

std::string Tracker::handle_accept_request(const std::vector<std::string>& tokens) {
    if (tokens.size() < 4) {
        return "ERROR: Invalid ACCEPT_REQUEST command\n";
    }
    
    std::string owner_id = tokens[1];
    std::string group_id = tokens[2];
    std::string user_id = tokens[3];
    
    if (users.find(owner_id) == users.end() || !users[owner_id].online) {
        return "ERROR: Owner not logged in\n";
    }
    
    if (groups.find(group_id) == groups.end()) {
        return "ERROR: Group not found\n";
    }
    
    if (groups[group_id].owner != owner_id) {
        return "ERROR: Not group owner\n";
    }
    
    if (groups[group_id].pending_requests.find(user_id) == groups[group_id].pending_requests.end()) {
        return "ERROR: No pending request from user\n";
    }
    
    groups[group_id].pending_requests.erase(user_id);
    groups[group_id].members.insert(user_id);
    users[user_id].groups.insert(group_id);
    
    return "SUCCESS: Request accepted\n";
}

std::string Tracker::handle_list_files(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        return "ERROR: Invalid LIST_FILES command\n";
    }
    
    std::string user_id = tokens[1];
    std::string group_id = tokens[2];
    
    if (users.find(user_id) == users.end() || !users[user_id].online) {
        return "ERROR: User not logged in\n";
    }
    
    if (groups.find(group_id) == groups.end()) {
        return "ERROR: Group not found\n";
    }
    
    if (groups[group_id].members.find(user_id) == groups[group_id].members.end()) {
        return "ERROR: Not a group member\n";
    }
    
    if (groups[group_id].shared_files.empty()) {
        return "No files shared in this group\n";
    }
    
    std::string result;
    for (const auto& file : groups[group_id].shared_files) {
        result += file.first + " (Shared by: ";
        for (size_t i = 0; i < file.second.size(); ++i) {
            if (i > 0) result += ", ";
            result += file.second[i];
        }
        result += ")\n";
    }
    return result;
}

std::string Tracker::handle_upload_file(const std::vector<std::string>& tokens) {
    std::cout << BOLD << MAGENTA << "📤 LARGE FILE UPLOAD REQUEST" << RESET << std::endl;
    std::cout << BLUE << "📤 Upload request received with " << tokens.size() << " tokens" << RESET << std::endl;
    
    if (tokens.size() < 7) {
        std::cout << RED << "❌ Invalid token count: " << tokens.size() << RESET << std::endl;
        return "ERROR: Invalid UPLOAD_FILE command - insufficient parameters\n";
    }
    
    std::string user_id = tokens[1];
    std::string group_id = tokens[2];
    std::string filename = tokens[3];
    std::string file_hash = tokens[4];
    std::string piece_hashes_str = tokens[5];
    
    long file_size;
    try {
        file_size = std::stol(tokens[6]);
    } catch (const std::exception& e) {
        std::cout << RED << "❌ Invalid file size: " << tokens[6] << RESET << std::endl;
        return "ERROR: Invalid file size\n";
    }
    
    // Calculate file size in different units for display
    double file_size_mb = file_size / (1024.0 * 1024.0);
    double file_size_gb = file_size_mb / 1024.0;
    
    std::cout << BOLD << CYAN << "📤 LARGE FILE UPLOAD DETAILS:" << RESET << std::endl;
    std::cout << "   👤 User: " << user_id << std::endl;
    std::cout << "   👥 Group: " << group_id << std::endl;
    std::cout << "   📁 File: " << filename << std::endl;
    std::cout << "   📊 Size: " << file_size << " bytes";
    
    if (file_size_gb >= 1.0) {
        std::cout << " (" << std::fixed << std::setprecision(2) << file_size_gb << " GB)" << std::endl;
    } else {
        std::cout << " (" << std::fixed << std::setprecision(2) << file_size_mb << " MB)" << std::endl;
    }
    
    std::cout << "   🔐 Hash: " << file_hash.substr(0, 16) << "..." << std::endl;
    std::cout << "   🧩 Piece hashes length: " << piece_hashes_str.length() << " chars" << std::endl;
    
    // Estimate number of pieces
    const long PIECE_SIZE = 524288;     // 512KB
    long estimated_pieces = (file_size + PIECE_SIZE - 1) / PIECE_SIZE;
    std::cout << "   🧩 Estimated pieces: " << estimated_pieces << std::endl;
    
    // Validate user and group
    if (users.find(user_id) == users.end() || !users[user_id].online) {
        std::cout << RED << "❌ User not logged in: " << user_id << RESET << std::endl;
        return "ERROR: User not logged in\n";
    }
    
    if (groups.find(group_id) == groups.end()) {
        std::cout << RED << "❌ Group not found: " << group_id << RESET << std::endl;
        return "ERROR: Group not found\n";
    }
    
    if (groups[group_id].members.find(user_id) == groups[group_id].members.end()) {
        std::cout << RED << "❌ User not in group: " << user_id << RESET << std::endl;
        return "ERROR: Not a group member\n";
    }
    
    // Add file to group's shared files
    if (groups[group_id].shared_files.find(filename) == groups[group_id].shared_files.end()) {
        groups[group_id].shared_files[filename] = std::vector<std::string>();
    }
    
    // Add user to the list of users who have this file (avoid duplicates)
    auto& file_users = groups[group_id].shared_files[filename];
    if (std::find(file_users.begin(), file_users.end(), user_id) == file_users.end()) {
        file_users.push_back(user_id);
    }
    
    // Store file entry with optimized hash handling
    FileEntry file_entry;
    file_entry.filename = filename;
    file_entry.file_hash = file_hash;
    file_entry.file_size = file_size;
    file_entry.owner = user_id;
    file_entry.group_id = group_id;
    
    // Parse piece hashes intelligently
    try {
        bool is_truncated = piece_hashes_str.find("TRUNCATED") != std::string::npos;
        
        if (is_truncated) {
            std::string clean_hashes = piece_hashes_str;
            size_t truncated_pos = clean_hashes.find("TRUNCATED");
            if (truncated_pos != std::string::npos) {
                clean_hashes = clean_hashes.substr(0, truncated_pos);
            }
            
            // Parse available hashes (8 characters each)
            for (size_t i = 0; i < clean_hashes.length(); i += 8) {
                if (i + 8 <= clean_hashes.length()) {
                    file_entry.piece_hashes.push_back(clean_hashes.substr(i, 8));
                }
            }
            
            std::cout << YELLOW << "⚠ Hash info truncated. Stored " << file_entry.piece_hashes.size() 
                      << " piece hashes out of " << estimated_pieces << " total pieces" << RESET << std::endl;
        } else {
            // Check hash length to determine format
            if (piece_hashes_str.length() % 8 == 0) {
                // 8-character hashes
                for (size_t i = 0; i < piece_hashes_str.length(); i += 8) {
                    file_entry.piece_hashes.push_back(piece_hashes_str.substr(i, 8));
                }
            } else if (piece_hashes_str.length() % 20 == 0) {
                // 20-character hashes (legacy format)
                for (size_t i = 0; i < piece_hashes_str.length(); i += 20) {
                    file_entry.piece_hashes.push_back(piece_hashes_str.substr(i, 20));
                }
            } else {
                // Flexible parsing - try to extract what we can
                std::cout << YELLOW << "⚠ Non-standard hash format, using flexible parsing" << RESET << std::endl;
                
                for (size_t i = 0; i < piece_hashes_str.length(); i += 8) {
                    if (i + 8 <= piece_hashes_str.length()) {
                        file_entry.piece_hashes.push_back(piece_hashes_str.substr(i, 8));
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << RED << "❌ Error parsing piece hashes: " << e.what() << RESET << std::endl;
        return "ERROR: Failed to parse piece hashes\n";
    }
    
    files[file_hash] = file_entry;
    
    // Success message with detailed stats
    std::cout << BOLD << GREEN << "✅ LARGE FILE UPLOAD SUCCESSFUL:" << RESET << std::endl;
    std::cout << GREEN << "   📁 File: " << filename << RESET << std::endl;
    std::cout << GREEN << "   📊 Size: ";
    if (file_size_gb >= 1.0) {
        std::cout << std::fixed << std::setprecision(2) << file_size_gb << " GB";
    } else {
        std::cout << std::fixed << std::setprecision(2) << file_size_mb << " MB";
    }
    std::cout << " (" << file_size << " bytes)" << RESET << std::endl;
    std::cout << GREEN << "   🧩 Piece hashes stored: " << file_entry.piece_hashes.size() << RESET << std::endl;
    std::cout << GREEN << "   🧩 Estimated total pieces: " << estimated_pieces << RESET << std::endl;
    std::cout << GREEN << "   👥 Available in group: " << group_id << RESET << std::endl;
    
    return "SUCCESS: Large file uploaded successfully\n";
}

std::string Tracker::handle_download_file(const std::vector<std::string>& tokens) {
    if (tokens.size() < 4) {
        return "ERROR: Invalid DOWNLOAD_FILE command\n";
    }
    
    std::string user_id = tokens[1];
    std::string group_id = tokens[2];
    std::string filename = tokens[3];
    
    std::cout << BLUE << "📥 Download request for " << filename << " from " << user_id << RESET << std::endl;
    
    if (users.find(user_id) == users.end() || !users[user_id].online) {
        return "ERROR: User not logged in\n";
    }
    
    if (groups.find(group_id) == groups.end()) {
        return "ERROR: Group not found\n";
    }
    
    if (groups[group_id].members.find(user_id) == groups[group_id].members.end()) {
        return "ERROR: Not a group member\n";
    }
    
    if (groups[group_id].shared_files.find(filename) == groups[group_id].shared_files.end()) {
        return "ERROR: File not found in group\n";
    }
    
    // Build peer list with correct format
    std::string result = "PEERS: ";
    int peer_count = 0;
    
    for (const auto& peer_id : groups[group_id].shared_files[filename]) {
        auto user_it = users.find(peer_id);
        if (user_it != users.end() && user_it->second.online) {
            // Format: IP PORT USERNAME (space-separated)
            result += user_it->second.ip + " " + std::to_string(user_it->second.port) + " " + peer_id + " ";
            peer_count++;
            
            std::cout << GREEN << "✓ Added peer: " << peer_id
                      << " (" << user_it->second.ip << ":" << user_it->second.port << ")"
                      << RESET << std::endl;
        } else {
            std::cout << YELLOW << "⚠ Peer offline: " << peer_id << RESET << std::endl;
        }
    }
    
    if (peer_count == 0) {
        return "ERROR: No online peers available\n";
    }
    
    // Remove trailing space and add newline
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    result += "\n";
    
    std::cout << CYAN << "📤 Sending " << peer_count << " peer(s) for " << filename << RESET << std::endl;
    return result;
}

std::string Tracker::handle_logout(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        return "ERROR: Invalid LOGOUT command\n";
    }
    
    std::string user_id = tokens[1];
    if (users.find(user_id) != users.end()) {
        users[user_id].online = false;
        std::cout << YELLOW << "👋 User logged out: " << user_id << RESET << std::endl;
    }
    
    return "SUCCESS: Logged out\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <tracker_info.txt> <tracker_number>" << std::endl;
        return 1;
    }
    
    std::string tracker_file(argv[1]);
    int tracker_number = std::stoi(argv[2]);
    
    std::ifstream file(tracker_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open tracker info file" << std::endl;
        return 1;
    }
    
    std::string line;
    int current = 0;
    int port = 0;
    while (std::getline(file, line) && current <= tracker_number) {
        if (current == tracker_number) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                port = std::stoi(line.substr(colon_pos + 1));
            }
            break;
        }
        current++;
    }
    file.close();
    
    if (port == 0) {
        std::cerr << "Invalid tracker number or port not found" << std::endl;
        return 1;
    }
    
    Tracker tracker(port, tracker_number);
    if (!tracker.initialize(tracker_file)) {
        std::cerr << "Failed to initialize tracker" << std::endl;
        return 1;
    }
    
    tracker.run();
    return 0;
}