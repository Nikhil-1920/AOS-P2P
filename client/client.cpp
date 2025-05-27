#include "client.h"
#include "ui.h"

// #define CHECK "✓"

//=================================================================================================
// CONSTRUCTOR & DESTRUCTOR
//=================================================================================================
P2PClient::P2PClient(const std::string& ip, int port) 
    : my_ip(ip), my_port(port), logged_in(false), server_socket(-1), running(false) {
    signal(SIGPIPE, SIG_IGN); 
}

P2PClient::~P2PClient() {
    running = false;
    if (server_socket != -1) {
        close(server_socket);
    }
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

//=================================================================================================
// UI HELPER FUNCTIONS
//=================================================================================================

void P2PClient::print_menu() {
    std::cout << BOLD << BLUE << "┌─────────────────── MAIN MENU ───────────────────┐\n" << RESET;
    
    if (!logged_in) {
        std::cout << YELLOW << "│  1. Create User Account                         │\n";
        std::cout << "│  2. Login                                       │\n";
        std::cout << "│  0. Exit                                        │\n";
    } else {
        std::cout << YELLOW;
        std::cout << "│  1. Create Group                                │\n";
        std::cout << "│  2. Join Group                                  │\n";
        std::cout << "│  3. Leave Group                                 │\n";
        std::cout << "│  4. List All Groups                             │\n";
        std::cout << "│  5. List Pending Requests                       │\n";
        std::cout << "│  6. Accept Group Request                        │\n";
        std::cout << "│  7. List Files in Group                         │\n";
        std::cout << "│  8. Upload File                                 │\n";
        std::cout << "│  9. Download File                               │\n";
        std::cout << "│ 10. Show Downloads                              │\n";
        std::cout << "│ 11. Stop Sharing File                           │\n";
        std::cout << "│ 12. Logout                                      │\n";
        std::cout << "│  0. Exit                                        │\n";
    }
    
    std::cout << BLUE << "└─────────────────────────────────────────────────┘\n" << RESET;
}
void P2PClient::clear_screen() {
    std::cout << "\033[2J\033[1;1H";
}
void P2PClient::print_colored(const std::string& text, const std::string& color) {
    std::cout << color << text << RESET;
}
void P2PClient::print_separator() {     
    std::cout << CYAN << "─────────────────────────────────────────────────────────\n" << RESET;
}
void P2PClient::print_success(const std::string& message) {
    std::cout << GREEN << "✓ " << message << RESET << "\n";
}
void P2PClient::print_error(const std::string& message) {
    std::cout << RED << "✗ " << message << RESET << "\n";
}
void P2PClient::print_info(const std::string& message) {
    std::cout << BLUE << "ℹ " << message << RESET << "\n";
}

//=================================================================================================
// INITIALIZATION & NETWORK SETUP
//=================================================================================================
bool P2PClient::initialize(const std::string& tracker_file) {
    std::ifstream file(tracker_file);
    if (!file.is_open()) {
        print_error("Failed to open tracker info file: " + tracker_file);
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                TrackerInfo tracker;
                tracker.ip = line.substr(0, colon_pos);
                tracker.port = std::stoi(line.substr(colon_pos + 1));
                trackers.push_back(tracker);
            }
        }
    }
    file.close();
    
    if (trackers.empty()) {
        print_error("No tracker information found in file");
        return false;
    }
    
    // Start server for peer-to-peer connections
    start_server();
    
    print_success("Client initialized successfully");
    print_info("Your IP: " + my_ip + ":" + std::to_string(my_port));
    print_info("Found " + std::to_string(trackers.size()) + " tracker(s)");
    
    return true;
}
void P2PClient::start_server() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        print_error("Failed to create server socket");
        return;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(my_port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        print_error("Failed to bind server socket");
        close(server_socket);
        return;
    }
    
    if (listen(server_socket, 10) < 0) {
        print_error("Failed to listen on server socket");
        close(server_socket);
        return;
    }
    
    running = true;
    server_thread = std::thread([this]() {
        while (running) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_socket, &read_fds);
            
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            
            int result = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
            if (result > 0 && FD_ISSET(server_socket, &read_fds)) {
                int peer_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
                if (peer_socket >= 0) {
                    std::thread peer_thread(&P2PClient::handle_peer_connection, this, peer_socket);
                    peer_thread.detach();
                }
            }
        }
    });
}
//=================================================================================================
// PEER CONNECTION HANDLING
//=================================================================================================
void P2PClient::handle_peer_connection(int peer_socket) {
    print_info("New peer connection received");
    
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_received = recv(peer_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::string request(buffer);
        
        if (!request.empty() && request.back() == '\n') {
            request.pop_back();
        }
        
        print_info("Received request: " + request);
        
        // Parse request: "GET_PIECE <filename> <piece_index>"
        std::vector<std::string> tokens = split_string(request, ' ');
        
        if (tokens.size() >= 3 && tokens[0] == "GET_PIECE") {
            std::string filename = tokens[1];
            int piece_index = std::stoi(tokens[2]);
            
            print_info("Request for piece " + std::to_string(piece_index) + " of file " + filename);
            
            std::vector<std::string> possible_paths = {
                filename,                       // Current directory
                "client/" + filename,           // Client directory  
                "./" + filename,                // Explicit current
                "../" + filename,               // Parent directory
            };
            
            std::string file_path;
            bool file_found = false;
            
            for (const auto& path : possible_paths) {
                std::ifstream test_file(path, std::ios::binary);
                if (test_file.is_open()) {
                    file_path = path;
                    file_found = true;
                    test_file.close();
                    print_info("Found file at: " + file_path);
                    break;
                }
            }
            
            if (!file_found) {
                print_error("File not found: " + filename);
                std::string response = "PIECE_NOT_FOUND\n";
                send(peer_socket, response.c_str(), response.length(), 0);
                close(peer_socket);
                return;
            }
            
            std::ifstream file(file_path, std::ios::binary);
            if (!file.is_open()) {
                print_error("Failed to open file: " + file_path);
                std::string response = "PIECE_NOT_FOUND\n";
                send(peer_socket, response.c_str(), response.length(), 0);
                close(peer_socket);
                return;
            }
            
            // Get file size first
            file.seekg(0, std::ios::end);
            long file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            print_info("File size: " + std::to_string(file_size) + " bytes");
            
            // Calculate piece offset and size
            const size_t ACTUAL_PIECE_SIZE = 524288; // 512KB
            long piece_offset = (long)piece_index * ACTUAL_PIECE_SIZE;
            
            // Check if this piece exists in the file
            if (piece_offset >= file_size) {
                print_info("Piece " + std::to_string(piece_index) + " is beyond file size");
                std::string response = "PIECE_NOT_FOUND\n";
                send(peer_socket, response.c_str(), response.length(), 0);
                file.close();
                close(peer_socket);
                return;
            }
            
            // Seek to piece location
            file.seekg(piece_offset);
            if (file.fail()) {
                print_error("Failed to seek to piece " + std::to_string(piece_index));
                std::string response = "PIECE_NOT_FOUND\n";
                send(peer_socket, response.c_str(), response.length(), 0);
                file.close();
                close(peer_socket);
                return;
            }
            
            // Calculate how much data to read
            long remaining_file_size = file_size - piece_offset;
            int piece_data_size = std::min((long)ACTUAL_PIECE_SIZE, remaining_file_size);
            
            if (piece_data_size <= 0) {
                print_info("No data to read for piece " + std::to_string(piece_index));
                std::string response = "PIECE_NOT_FOUND\n";
                send(peer_socket, response.c_str(), response.length(), 0);
                file.close();
                close(peer_socket);
                return;
            }
            
            // Read the piece data
            std::vector<char> piece_buffer(piece_data_size);
            file.read(piece_buffer.data(), piece_data_size);
            int actual_read = file.gcount();
            file.close();
            
            if (actual_read <= 0) {
                print_error("No data read for piece " + std::to_string(piece_index));
                std::string response = "PIECE_NOT_FOUND\n";
                send(peer_socket, response.c_str(), response.length(), 0);
                close(peer_socket);
                return;
            }
            
            print_info("Sending piece " + std::to_string(piece_index) + 
                      " (" + std::to_string(actual_read) + " bytes)");
            
            // Send response header FIRST
            std::string response_header = "PIECE_DATA " + std::to_string(actual_read) + "\n";
            ssize_t header_sent = send(peer_socket, response_header.c_str(), response_header.length(), 0);
            
            if (header_sent <= 0) {
                print_error("Failed to send response header");
                close(peer_socket);
                return;
            }
            
            print_info("Sent header: '" + response_header.substr(0, response_header.length()-1) + "'");
            
            // THEN send piece data in chunks
            int total_sent = 0;
            int chunk_size = 1024;
            
            while (total_sent < actual_read) {
                int to_send = std::min(chunk_size, actual_read - total_sent);
                ssize_t sent = send(peer_socket, piece_buffer.data() + total_sent, to_send, 0);
                
                if (sent <= 0) {
                    print_error("Failed to send piece data at offset " + std::to_string(total_sent));
                    break;
                }
                
                total_sent += sent;
                print_info("Sent " + std::to_string(sent) + " bytes, total: " + std::to_string(total_sent));
            }
            
            if (total_sent == actual_read) {
                print_success("Successfully sent piece " + std::to_string(piece_index) + 
                             " (" + std::to_string(total_sent) + " bytes)");
            } else {
                print_error("Incomplete piece send: " + std::to_string(total_sent) + 
                           "/" + std::to_string(actual_read));
            }
            
        } else {
            print_error("Invalid request format: " + request);
            std::string response = "INVALID_REQUEST\n";
            send(peer_socket, response.c_str(), response.length(), 0);
        }
    } else {
        print_error("Failed to receive request from peer");
    }
    
    close(peer_socket);
}

//=================================================================================================
// TRACKER COMMUNICATION
//=================================================================================================
bool P2PClient::connect_to_tracker(int& tracker_socket) {
    for (const auto& tracker : trackers) {
        tracker_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (tracker_socket < 0) continue;
        
        struct sockaddr_in tracker_addr;
        tracker_addr.sin_family = AF_INET;
        tracker_addr.sin_port = htons(tracker.port);
        inet_pton(AF_INET, tracker.ip.c_str(), &tracker_addr.sin_addr);
        
        if (connect(tracker_socket, (struct sockaddr*)&tracker_addr, sizeof(tracker_addr)) == 0) {
            return true;
        }
        
        close(tracker_socket);
    }
    return false;
}
bool P2PClient::send_to_tracker(int socket, const std::string& message) {
    return send(socket, message.c_str(), message.length(), 0) > 0;
}
std::string P2PClient::receive_from_tracker(int socket) {
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        return std::string(buffer);
    }
    return "";
}
//=================================================================================================
// UTILITY FUNCTIONS
//=================================================================================================
std::vector<std::string> P2PClient::split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}
std::string P2PClient::calculate_file_hash(const std::string& filepath) {
    print_info("Calculating file hash for: " + filepath);
    
    try {
        // Use the safer from_file method
        std::string hash = SHA1::from_file(filepath);
        if (hash.empty()) {
            print_error("Failed to calculate hash - file may not exist");
            return "";
        }
        
        print_info("Hash calculation completed successfully");
        return hash;
        
    } catch (const std::exception& e) {
        print_error("Exception during hash calculation: " + std::string(e.what()));
        return "";
    } catch (...) {
        print_error("Unknown error during hash calculation");
        return "";
    }
}
std::vector<std::string> P2PClient::calculate_piece_hashes(const std::string& filepath) {
    std::vector<std::string> piece_hashes;
    
    print_info("Calculating piece hashes...");
    
    try {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            print_error("Cannot open file for piece hash calculation");
            return piece_hashes;
        }
        
        // Use the same piece size as defined in PIECE_SIZE (512KB)
        char buffer[PIECE_SIZE];  // Use the original PIECE_SIZE, not 4KB
        int piece_number = 0;
        
        while (file.read(buffer, PIECE_SIZE) || file.gcount() > 0) {
            SHA1 sha1;
            sha1.update(buffer, file.gcount());
            std::string hash = sha1.final();
            
            if (!hash.empty()) {
                piece_hashes.push_back(hash);
                print_info("Piece " + std::to_string(piece_number++) + " hash calculated");
            }
        }
        
        file.close();
        print_info("All piece hashes calculated successfully");
        return piece_hashes;
        
    } catch (const std::exception& e) {
        print_error("Exception during piece hash calculation: " + std::string(e.what()));
        return std::vector<std::string>();
    } catch (...) {
        print_error("Unknown error during piece hash calculation");
        return std::vector<std::string>();
    }
}
//=================================================================================================
// USER MANAGEMENT FUNCTIONS
//=================================================================================================
bool P2PClient::create_user(const std::string& username, const std::string& password) {
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    std::string command = "CREATE_USER " + username + " " + password + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (response.find("SUCCESS") != std::string::npos) {
        print_success("User account created successfully!");
        return true;
    } else {
        print_error("Failed to create user: " + response);
        return false;
    }
}
bool P2PClient::login(const std::string& username, const std::string& password) {
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    print_info("Sending login with IP: " + my_ip + " and port: " + std::to_string(my_port));
    
    std::string command = "LOGIN " + username + " " + password + " " + my_ip + " " + std::to_string(my_port) + "\n";
    
    print_info("Login command: " + command.substr(0, command.length()-1)); // Remove \n for display
    
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (response.find("SUCCESS") != std::string::npos) {
        logged_in = true;
        user_id = username;
        print_success("Logged in successfully! Welcome, " + username + "!");
        return true;
    } else {
        print_error("Login failed: " + response);
        return false;
    }
}
bool P2PClient::logout() {
    if (!logged_in) {
        print_error("Not logged in");
        return false;
    }
    
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    std::string command = "LOGOUT " + user_id + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    logged_in = false;
    user_id = "";
    shared_files.clear();
    active_downloads.clear();
    
    print_success("Logged out successfully!");
    return true;
}
//=================================================================================================
// GROUP MANAGEMENT FUNCTIONS
//=================================================================================================
bool P2PClient::create_group(const std::string& group_id) {
    if (!logged_in) {
        print_error("Please login first");
        return false;
    }
    
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    std::string command = "CREATE_GROUP " + user_id + " " + group_id + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (response.find("SUCCESS") != std::string::npos) {
        print_success("Group '" + group_id + "' created successfully!");
        return true;
    } else {
        print_error("Failed to create group: " + response);
        return false;
    }
}
bool P2PClient::join_group(const std::string& group_id) {
    if (!logged_in) {
        print_error("Please login first");
        return false;
    }
    
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    std::string command = "JOIN_GROUP " + user_id + " " + group_id + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (response.find("SUCCESS") != std::string::npos) {
        print_success("Join request sent for group '" + group_id + "'");
        return true;
    } else {
        print_error("Failed to join group: " + response);
        return false;
    }
}
bool P2PClient::leave_group(const std::string& group_id) {
    if (!logged_in) {
        print_error("Please login first");
        return false;
    }
    
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    std::string command = "LEAVE_GROUP " + user_id + " " + group_id + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (response.find("SUCCESS") != std::string::npos) {
        print_success("Left group '" + group_id + "' successfully");
        return true;
    } else {
        print_error("Failed to leave group: " + response);
        return false;
    }
}
bool P2PClient::list_groups() {
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    std::string command = "LIST_GROUPS\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (!response.empty()) {
        print_info("Available Groups:");
        print_separator();
        std::cout << YELLOW << response << RESET;
        print_separator();
        return true;
    } else {
        print_info("No groups available");
        return false;
    }
}
bool P2PClient::list_requests(const std::string& group_id) {
    if (!logged_in) {
        print_error("Please login first");
        return false;
    }
    
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    std::string command = "LIST_REQUESTS " + user_id + " " + group_id + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (!response.empty() && response.find("ERROR") == std::string::npos) {
        print_info("Pending requests for group '" + group_id + "':");
        print_separator();
        std::cout << YELLOW << response << RESET;
        print_separator();
        return true;
    } else {
        print_info("No pending requests or you're not the owner");
        return false;
    }
}
bool P2PClient::accept_request(const std::string& group_id, const std::string& username) {
    if (!logged_in) {
        print_error("Please login first");
        return false;
    }
    
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    std::string command = "ACCEPT_REQUEST " + user_id + " " + group_id + " " + username + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (response.find("SUCCESS") != std::string::npos) {
        print_success("Accepted join request from '" + username + "' for group '" + group_id + "'");
        return true;
    } else {
        print_error("Failed to accept request: " + response);
        return false;
    }
}
//=================================================================================================
// FILE MANAGEMENT FUNCTIONS
//=================================================================================================
bool P2PClient::list_files(const std::string& group_id) {
    if (!logged_in) {
        print_error("Please login first");
        return false;
    }
    
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    std::string command = "LIST_FILES " + user_id + " " + group_id + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (!response.empty() && response.find("ERROR") == std::string::npos) {
        print_info("Files available in group '" + group_id + "':");
        print_separator();
        std::cout << GREEN << response << RESET;
        print_separator();
        return true;
    } else {
        print_info("No files available or access denied");
        return false;
    }
}

bool P2PClient::upload_file(const std::string& filepath, const std::string& group_id) {
    if (!logged_in) {
        print_error("Please login first");
        return false;
    }
    
    print_info("Attempting to upload file: " + filepath);
    
    // Check if file exists and is readable
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        print_error("File not found or cannot be opened: " + filepath);
        print_info("Please check:");
        print_info("1. File exists in current directory");
        print_info("2. File has read permissions");
        print_info("3. Use absolute path if file is elsewhere");
        return false;
    }
    
    // Get file size first
    file.seekg(0, std::ios::end);
    long file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    file.close();
    
    if (file_size == 0) {
        print_error("File is empty: " + filepath);
        return false;
    }
   
    print_info("File size: " + std::to_string(file_size) + " bytes");
    
    // Get file stats using safer method
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        print_error("Failed to get file statistics");
        return false;
    }
    
    // Extract filename from path
    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    if (filename.empty()) {
        print_error("Invalid filename");
        return false;
    }
   
    print_info("Calculating file hash...");
    
    // Calculate file hash with error handling
    std::string file_hash;
    try {
        file_hash = calculate_file_hash(filepath);
        if (file_hash.empty()) {
            print_error("Failed to calculate file hash");
            return false;
        }
    } catch (const std::exception& e) {
        print_error("Error calculating file hash: " + std::string(e.what()));
        return false;
    }
   
    print_info("File hash calculated successfully");
    print_info("Calculating piece hashes...");
    
    // Calculate piece hashes with error handling
    std::vector<std::string> piece_hashes;
    try {
        piece_hashes = calculate_piece_hashes(filepath);
        if (piece_hashes.empty()) {
            print_error("Failed to calculate piece hashes");
            return false;
        }
    } catch (const std::exception& e) {
        print_error("Error calculating piece hashes: " + std::string(e.what()));
        return false;
    }
   
    print_info("Calculated " + std::to_string(piece_hashes.size()) + " piece hashes");
    
    // Connect to tracker
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
    
    // Create hash string (first 20 chars of each piece hash)
    std::string hash_string = "";
    try {
        for (const auto& hash : piece_hashes) {
            if (hash.length() >= 20) {
                hash_string += hash.substr(0, 20);
            } else {
                hash_string += hash;
            }
        }
    } catch (const std::exception& e) {
        print_error("Error creating hash string: " + std::string(e.what()));
        close(tracker_socket);
        return false;
    }
   
    // Create command
    std::string command;
    try {
        command = "UPLOAD_FILE " + user_id + " " + group_id + " " + filename + " " + 
                    file_hash + " " + hash_string + " " + std::to_string(file_stat.st_size) + "\n";
    } catch (const std::exception& e) {
        print_error("Error creating upload command: " + std::string(e.what()));
        close(tracker_socket);
        return false;
    }
    
    print_info("Sending upload request to tracker...");
    
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
   
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (response.find("SUCCESS") != std::string::npos) {
        shared_files.insert(filepath);
        print_success("File '" + filename + "' uploaded successfully to group '" + group_id + "'");
        print_info("File hash: " + file_hash.substr(0, 16) + "...");
        print_info("File size: " + std::to_string(file_stat.st_size) + " bytes");
        print_info("Total pieces: " + std::to_string(piece_hashes.size()));
        return true;
    } else {
        print_error("Failed to upload file: " + response);
        return false;
    }
}

bool P2PClient::stop_share(const std::string& group_id, const std::string& filename) {
    if (!logged_in) {
        print_error("Please login first");
        return false;
    }
   
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
   
    std::string command = "STOP_SHARE " + user_id + " " + group_id + " " + filename + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
    
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    if (response.find("SUCCESS") != std::string::npos) {
        print_success("Stopped sharing '" + filename + "' in group '" + group_id + "'");
        return true;
    } else {
        print_error("Failed to stop sharing: " + response);
        return false;
    }
}

//=================================================================================================
// DOWNLOAD FUNCTIONS
//=================================================================================================
bool P2PClient::download_file(const std::string& group_id, const std::string& filename, 
                            const std::string& dest_path) {
    if (!logged_in) {
        print_error("Please login first");
        return false;
    }
   
    int tracker_socket;
    if (!connect_to_tracker(tracker_socket)) {
        print_error("Failed to connect to tracker");
        return false;
    }
   
    std::string command = "DOWNLOAD_FILE " + user_id + " " + group_id + " " + filename + "\n";
    if (!send_to_tracker(tracker_socket, command)) {
        print_error("Failed to send command to tracker");
        close(tracker_socket);
        return false;
    }
   
    std::string response = receive_from_tracker(tracker_socket);
    close(tracker_socket);
    
    // DEBUG: Print raw response
    print_info("Raw tracker response: '" + response + "'");
    
    if (response.find("ERROR") != std::string::npos) {
        print_error("Failed to get file info: " + response);
        return false;
    }
   
    // Parse response to get peer information
    std::vector<std::string> lines = split_string(response, '\n');
    
    if (lines.empty()) {
        print_error("Invalid response from tracker");
        return false;
    }
   
    FileInfo file_info;
    file_info.filename = filename;
    file_info.file_size = 0; // Will be estimated
   
    // Parse file information and peer list
    for (const auto& line : lines) {
        print_info("Processing line: '" + line + "'");
        
        if (line.find("PEERS:") != std::string::npos) {
            // Get everything after "PEERS: "
            std::string peer_data = line.substr(7); // Skip "PEERS: "
            print_info("Peer data: '" + peer_data + "'");
            
            // Remove any extra spaces
            while (!peer_data.empty() && peer_data.back() == ' ') {
                peer_data.pop_back();
            }
            
            if (peer_data.empty()) {
                print_error("No peer data found");
                continue;
            }
           
            // Parse peer information: IP PORT USERNAME IP PORT USERNAME ...
            std::vector<std::string> peer_tokens = split_string(peer_data, ' ');
            print_info("Found " + std::to_string(peer_tokens.size()) + " peer tokens");
            
            // Parse tokens in groups of 3
            for (size_t i = 0; i < peer_tokens.size(); i += 3) {
                if (i + 2 < peer_tokens.size()) {
                    std::string ip = peer_tokens[i];
                    std::string port_str = peer_tokens[i + 1];
                    std::string username = peer_tokens[i + 2];
                    
                    // Skip empty tokens
                    if (ip.empty() || port_str.empty() || username.empty()) {
                        print_error("Skipping empty peer tokens at position " + std::to_string(i));
                        continue;
                    }
                    
                    try {
                        PeerInfo peer;
                        peer.ip = ip;
                        peer.port = std::stoi(port_str);
                        peer.user_id = username;
                        
                        print_info("Parsed peer: " + peer.user_id + " at " + peer.ip + ":" + std::to_string(peer.port));
                        file_info.peers.push_back(peer);
                    } catch (const std::exception& e) {
                        print_error("Error parsing peer info: " + std::string(e.what()));
                    }
                } else {
                    print_error("Incomplete peer token group at position " + std::to_string(i));
                }
            }
        }
    }
   
    if (file_info.peers.empty()) {
        print_error("No peers available for this file");
        return false;
    }
    
    print_info("Found " + std::to_string(file_info.peers.size()) + " peer(s) for file '" + filename + "'");
    
    // Start piece selection algorithm in a separate thread
    std::thread download_thread(&P2PClient::piece_selection_algorithm, this, file_info, dest_path);
    download_thread.detach();
    
    print_success("Download started for '" + filename + "'");
    return true;
}

//=================================================================================================
// HELPER FUNCTIONS FOR PROGRESS DISPLAY
//=================================================================================================
std::string P2PClient::format_speed(long bytes_per_sec) {
    if (bytes_per_sec < 1024) {
        return std::to_string(bytes_per_sec) + " B/s";
    } else if (bytes_per_sec < 1024 * 1024) {
        double kb_per_sec = bytes_per_sec / 1024.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << kb_per_sec << " KB/s";
        return oss.str();
    } else {
        double mb_per_sec = bytes_per_sec / (1024.0 * 1024.0);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << mb_per_sec << " MB/s";
        return oss.str();
    }
}

std::string P2PClient::format_bytes_static(long bytes) {
   const char* units[] = {"B", "KB", "MB", "GB"};
   double size = bytes;
   int unit = 0;
   
    while (size >= 1024 && unit < 3) {
        size /= 1024;
        unit++;
    }
   
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return oss.str();
}

void P2PClient::show_download_progress_inline(const std::string& filename, int percentage, 
                                           long downloaded_bytes, long total_bytes, 
                                           const std::string& speed) {
    // Clear the current line and move cursor to beginning
    std::cout << "\r\033[K";
   
    // Show filename
    std::cout << BRIGHT_CYAN << "📥 " << filename << RESET << " ";
    
    // Progress bar with colors (compact version)
    std::cout << "[";
    int filled = percentage / 4; // 25 chars total for compact display
    for (int i = 0; i < 25; i++) {
        if (i < filled) {
            if (percentage < 30) {
                std::cout << BRIGHT_RED << "█";
            } else if (percentage < 70) {
                std::cout << BRIGHT_YELLOW << "█";
            } else {
                std::cout << BRIGHT_GREEN << "█";
            }
        } else {
            std::cout << BRIGHT_BLACK << "░";
        }
    }
    std::cout << RESET << "] " << BRIGHT_WHITE << percentage << "%" << RESET;
    
    // Size and speed information
    std::cout << " | " << BRIGHT_BLUE << format_bytes_static(downloaded_bytes) 
                << "/" << format_bytes_static(total_bytes) << RESET;
    std::cout << " | " << BRIGHT_MAGENTA << speed << RESET;
    
    if (percentage >= 100) {
        std::cout << " " << BRIGHT_GREEN << CHECK << " Complete!" << RESET << std::endl;
    }
    
    std::cout.flush();
}

void P2PClient::piece_selection_algorithm(const FileInfo& file_info, const std::string& dest_path) {
    print_info("Starting download for " + file_info.filename);
    
    // Test all peer connections first
    std::vector<PeerInfo> working_peers;
    for (const auto& peer : file_info.peers) {
        if (test_peer_connection(peer)) {
            working_peers.push_back(peer);
        }
    }
    
    if (working_peers.empty()) {
        print_error("No working peers available for download");
        return;
    }
   
    print_success("Found " + std::to_string(working_peers.size()) + " working peer(s)");
    
    // Initialize download state
    DownloadState download_state;
    download_state.filename = file_info.filename;
    download_state.successful_pieces = 0;
    download_state.failed_pieces = 0;
    download_state.total_bytes = 0;
    download_state.downloaded_bytes = 0;
    download_state.start_time = std::chrono::steady_clock::now();
    download_state.show_detailed_logs = false; // Set to true for debug mode
    
    // Create download info
    DownloadInfo download_info;
    download_info.group_id = "";
    download_info.filename = file_info.filename;
    download_info.dest_path = dest_path;
    download_info.is_complete = false;
    download_info.downloaded_size = 0;
    download_info.total_size = 0;
   
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        active_downloads.emplace(file_info.filename, download_info);
    }
    
    // Clear screen section for download progress
    std::cout << "\n" << BRIGHT_CYAN << BOLD << "  📥 DOWNLOADING: " << file_info.filename << RESET << std::endl;
    std::cout << "  " << std::string(60, '-') << std::endl;
    
    // Reserve space for progress display
    std::cout << "\n\n\n";
    
    // Download pieces sequentially
    std::vector<int> successful_pieces;
    int piece_index = 0;
    int consecutive_failures = 0;
    const int MAX_CONSECUTIVE_FAILURES = 3;
    const int MAX_PIECES = 1000;
   
    // Variables for throttling progress updates
    auto last_update_time = std::chrono::steady_clock::now();
    const auto update_interval = std::chrono::milliseconds(100); // Update every 100ms max
    
    while (piece_index < MAX_PIECES && consecutive_failures < MAX_CONSECUTIVE_FAILURES) {
        const PeerInfo& selected_peer = working_peers[piece_index % working_peers.size()];
        
        bool success = download_piece_from_peer(selected_peer, file_info.filename, piece_index, dest_path);
        
        if (success) {
            successful_pieces.push_back(piece_index);
            consecutive_failures = 0;
            download_state.successful_pieces++;
            
            // Update download state
            long estimated_piece_size = 524288; // 512KB
            download_state.downloaded_bytes += estimated_piece_size;
            download_state.total_bytes = std::max(download_state.total_bytes, 
                                                    (long)(successful_pieces.size() + 5) * estimated_piece_size);
            
            // Update active downloads
            {
                std::lock_guard<std::mutex> lock(client_mutex);
                auto it = active_downloads.find(file_info.filename);
                if (it != active_downloads.end()) {
                    it->second.downloaded_size = download_state.downloaded_bytes;
                    it->second.total_size = download_state.total_bytes;
                }
            }
            
            // Check if we should update the progress display
            auto now = std::chrono::steady_clock::now();
            if (now - last_update_time >= update_interval || piece_index == 0) {
                last_update_time = now;
                
                // Calculate progress
                int progress = download_state.total_bytes > 0 ? 
                                (download_state.downloaded_bytes * 100) / download_state.total_bytes : 0;
                progress = std::min(progress, 95); // Cap at 95% until complete
                
                // Calculate speed
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - download_state.start_time);
                std::string speed = "0 KB/s";
                if (duration.count() > 0) {
                    long bytes_per_sec = download_state.downloaded_bytes / duration.count();
                    speed = format_speed(bytes_per_sec);
                }
                
                // Move cursor up to overwrite previous display
                std::cout << "\033[3A"; // Move up 3 lines
                
                // Show progress bar
                std::cout << "\033[K"; // Clear line
                show_download_progress_inline(
                    file_info.filename, 
                    progress, 
                    download_state.downloaded_bytes, 
                    download_state.total_bytes, 
                    speed
                );
               
                // Show piece info
                std::cout << "\n\033[K"; // New line and clear
                std::cout << "  " << BRIGHT_WHITE << "Pieces: " << BRIGHT_GREEN 
                            << download_state.successful_pieces << " downloaded" << RESET;
                if (download_state.failed_pieces > 0) {
                    std::cout << ", " << BRIGHT_RED << download_state.failed_pieces 
                                << " failed" << RESET;
                }
                std::cout << std::endl;
                
                // Show current activity
                std::cout << "\033[K"; // Clear line
                std::cout << "  " << BRIGHT_YELLOW << "Downloading piece " << piece_index 
                            << " from " << selected_peer.user_id << "..." << RESET << std::endl;
            }
           
        } else {
            consecutive_failures++;
            download_state.failed_pieces++;
            
            if (successful_pieces.empty() && consecutive_failures < MAX_CONSECUTIVE_FAILURES) {
                // Continue trying if no pieces downloaded yet
            } else if (!successful_pieces.empty()) {
                break; // End of file reached
            }
        }
        
        piece_index++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
   
    // Final display update
    std::cout << "\033[3A"; // Move up 3 lines
    std::cout << "\033[K";  // Clear line
    show_download_progress_inline(
        file_info.filename, 
        100, 
        download_state.downloaded_bytes, 
        download_state.downloaded_bytes, 
        "Complete"
    );
    std::cout << "\n\033[K\n\033[K\n"; 
    
    if (successful_pieces.empty()) {
        print_error("Download failed: No pieces were successfully downloaded");
        
        // Update download status
        {
            std::lock_guard<std::mutex> lock(client_mutex);
            auto it = active_downloads.find(file_info.filename);
            if (it != active_downloads.end()) {
                it->second.is_complete = false;
            }
        }
        return;
    }
   
    print_info("Combining " + std::to_string(successful_pieces.size()) + " pieces into final file...");
    
    // File combination logic
    std::string final_path = dest_path + "/" + file_info.filename;
    std::ofstream final_file(final_path, std::ios::binary);
    
    if (!final_file.is_open()) {
        print_error("Failed to create final file: " + final_path);
        return;
    }
    
    long total_bytes_written = 0;
    int pieces_combined = 0;
    
    // Sort pieces to ensure correct order
    std::sort(successful_pieces.begin(), successful_pieces.end());
   
    for (int piece_num : successful_pieces) {
        std::string piece_file = dest_path + "/" + file_info.filename + ".piece" + std::to_string(piece_num);
        std::ifstream piece_stream(piece_file, std::ios::binary);
        
        if (piece_stream.is_open()) {
            piece_stream.seekg(0, std::ios::end);
            long piece_size = piece_stream.tellg();
            piece_stream.seekg(0, std::ios::beg);
            
            if (piece_size > 0) {
                char buffer[4096];
                long bytes_copied = 0;
                
                while (bytes_copied < piece_size) {
                    int to_read = std::min((long)sizeof(buffer), piece_size - bytes_copied);
                    piece_stream.read(buffer, to_read);
                    int actually_read = piece_stream.gcount();
                    
                    if (actually_read <= 0) break;
                    
                    final_file.write(buffer, actually_read);
                    bytes_copied += actually_read;
                    total_bytes_written += actually_read;
                }
                
                pieces_combined++;
            }
            
            piece_stream.close();
            unlink(piece_file.c_str());
        }
    }
    
    final_file.close();
   
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        auto it = active_downloads.find(file_info.filename);
        if (it != active_downloads.end()) {
            it->second.is_complete = true;
            it->second.total_size = total_bytes_written;
            it->second.downloaded_size = total_bytes_written;
        }
    }
   
    if (pieces_combined > 0) {
       print_success("✨ Download completed successfully!");
       print_success("📁 File saved to: " + final_path);
       print_success("📊 Total size: " + format_bytes_static(total_bytes_written));
       print_success("🧩 Combined " + std::to_string(pieces_combined) + " pieces");
       
       // Calculate final stats
       auto end_time = std::chrono::steady_clock::now();
       auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - download_state.start_time);
       if (total_duration.count() > 0) {
           long avg_speed = total_bytes_written / total_duration.count();
           print_info("⚡ Average speed: " + format_speed(avg_speed));
           print_info("⏱️  Total time: " + std::to_string(total_duration.count()) + " seconds");
       }
       
    } else {
       print_error("Failed to combine pieces into final file");
    }
}

bool P2PClient::download_piece_from_peer(const PeerInfo& peer, const std::string& filename, 
                                        int piece_index, const std::string& dest_path) {

    int peer_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_socket < 0) {
       return false;
    }
   
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 10;  
    timeout.tv_usec = 0;
    setsockopt(peer_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(peer_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
   
    struct sockaddr_in peer_addr;
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer.port);
   
    if (inet_pton(AF_INET, peer.ip.c_str(), &peer_addr.sin_addr) <= 0) {
        close(peer_socket);
        return false;
    }
   
    // Try to connect
    if (connect(peer_socket, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        close(peer_socket);
        return false;
    }
   
    std::string request = "GET_PIECE " + filename + " " + std::to_string(piece_index) + "\n";
    ssize_t sent = send(peer_socket, request.c_str(), request.length(), 0);
    if (sent <= 0) {
        close(peer_socket);
        return false;
    }
   
    // Receive the entire response (header + data might come together)
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_received = recv(peer_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(peer_socket);
        return false;
    }
   
    buffer[bytes_received] = '\0';
    std::string full_response(buffer, bytes_received);
    
    if (full_response.find("PIECE_NOT_FOUND") != std::string::npos) {
        close(peer_socket);
        return false;
    }
    
    if (full_response.find("PIECE_DATA") == std::string::npos) {
        close(peer_socket);
        return false;
    }
   
    // Parse the header to get piece size
    size_t header_end = full_response.find('\n');
    if (header_end == std::string::npos) {
        close(peer_socket);
        return false;
    }
   
    std::string header = full_response.substr(0, header_end);
    
    size_t space_pos = header.find(' ');
    if (space_pos == std::string::npos) {
        close(peer_socket);
        return false;
    }
   
    int expected_piece_size;
    try {
        expected_piece_size = std::stoi(header.substr(space_pos + 1));
    } catch (const std::exception& e) {
        close(peer_socket);
        return false;
    }
   
    // Extract data that came with the header
    std::string piece_data;
    if (header_end + 1 < full_response.length()) {
        piece_data = full_response.substr(header_end + 1);
    }
   
    // If we need more data, receive it
    while ((int)piece_data.length() < expected_piece_size) {
        int remaining = expected_piece_size - piece_data.length();
        int to_receive = std::min(remaining, (int)sizeof(buffer));
        
        bytes_received = recv(peer_socket, buffer, to_receive, 0);
        if (bytes_received <= 0) {
            close(peer_socket);
            return false;
        }
        
        piece_data.append(buffer, bytes_received);
    }
    
    close(peer_socket);
    
    // Verify we got the right amount of data
    if ((int)piece_data.length() != expected_piece_size) {
        return false;
    }
    
    // Save the piece data to file
    std::string piece_file = dest_path + "/" + filename + ".piece" + std::to_string(piece_index);
    std::ofstream piece_stream(piece_file, std::ios::binary);
    
    if (!piece_stream.is_open()) {
        return false;
    }
   
    piece_stream.write(piece_data.data(), piece_data.length());
    piece_stream.close();
    
    return true;
}

bool P2PClient::test_peer_connection(const PeerInfo& peer) {
    if (peer.ip.empty()) {
        return false;
    }
   
    int test_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (test_socket < 0) {
        return false;
    }
   
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(test_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(test_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
   
    struct sockaddr_in peer_addr;
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer.port);
    
    if (inet_pton(AF_INET, peer.ip.c_str(), &peer_addr.sin_addr) <= 0) {
        close(test_socket);
        return false;
    }
   
    bool connected = (connect(test_socket, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) == 0);
    close(test_socket);
    
    return connected;
}

bool P2PClient::show_downloads() {
    if (active_downloads.empty()) {
        print_info("No active downloads");
        return true;
    }
   
    print_info("Active Downloads:");
    print_separator();
    
    std::lock_guard<std::mutex> lock(client_mutex);
    for (const auto& download : active_downloads) {
        const DownloadInfo& info = download.second;
        std::string status = info.is_complete ? "[COMPLETE]" : "[DOWNLOADING]";
        int progress = info.total_size > 0 ? (info.downloaded_size * 100) / info.total_size : 0;
        
        std::cout << status << " " << info.filename << " - " << progress << "% complete" << std::endl;
    }
    
    print_separator();
    return true;
}

//=================================================================================================
// MAIN RUN FUNCTION WITH PROFESSIONAL UI
//=================================================================================================
void P2PClient::run() {
    ProfessionalUI::clear_screen();
    
    std::string input;
    int choice;
    
    while (true) {
        ProfessionalUI::print_header();
        
        std::string status = logged_in ? "Online" : "Offline";
        std::string current_user = logged_in ? user_id : "Guest";
        ProfessionalUI::print_status_bar(current_user, status);
        
        ProfessionalUI::print_menu();
        
        if (!logged_in) {
            std::cout << std::endl;
            NotificationSystem::info("Please login to access group and file sharing features");
        }
        
        NotificationSystem::prompt("Enter your choice");
        std::cin >> choice;
        std::cin.ignore(); 
        
        ProfessionalUI::clear_screen();
        
        switch (choice) {
            case 1: {
                // CREATE USER
                std::cout << BRIGHT_MAGENTA << BOLD << "  👤 USER REGISTRATION" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string username, password;
                NotificationSystem::prompt("Username");
                std::getline(std::cin, username);
                NotificationSystem::prompt("Password");
                std::getline(std::cin, password);
                
                // Loading animation
                for (int i = 0; i <= 100; i += 10) {
                    LoadingAnimation::show_progress("Creating user", i);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                
                if (create_user(username, password)) {
                    NotificationSystem::success("User '" + username + "' created successfully!");
                } else {
                    NotificationSystem::error("Failed to create user!");
                }
                break;
            }
            
            case 2: {
                // LOGIN
                std::cout << BRIGHT_BLUE << BOLD << "  🔐 USER LOGIN" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                if (logged_in) {
                    NotificationSystem::warning("Already logged in as " + user_id);
                    NotificationSystem::info("Please logout first to switch users");
                    break;
                }
                
                std::string username, password;
                NotificationSystem::prompt("Username");
                std::getline(std::cin, username);
                NotificationSystem::prompt("Password");
                std::getline(std::cin, password);
                
                // Loading animation
                for (int i = 0; i <= 100; i += 20) {
                    LoadingAnimation::show_progress("Authenticating", i);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                if (login(username, password)) {
                    NotificationSystem::success("Welcome back, " + username + "!");
                } else {
                    NotificationSystem::error("Login failed! Please check your credentials.");
                }
                break;
            }
            
            case 3: {
                // LOGOUT
                std::cout << BRIGHT_YELLOW << BOLD << "  🚪 LOGOUT" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                if (!logged_in) {
                    NotificationSystem::warning("You are not logged in!");
                    break;
                }
                
                std::string confirm_user = user_id;
                if (logout()) {
                    NotificationSystem::success("User '" + confirm_user + "' logged out successfully!");
                } else {
                    NotificationSystem::error("Logout failed!");
                }
                break;
            }
            
            case 4: {
                // CREATE GROUP
                if (!logged_in) {
                    NotificationSystem::error("Please login first to create a group!");
                    break;
                }
                
                std::cout << BRIGHT_GREEN << BOLD << "  👥 CREATE GROUP" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string group_id;
                NotificationSystem::prompt("Group ID");
                std::getline(std::cin, group_id);
                
                if (create_group(group_id)) {
                    NotificationSystem::success("Group '" + group_id + "' created successfully!");
                    NotificationSystem::info("You are now the owner of this group");
                } else {
                    NotificationSystem::error("Failed to create group!");
                }
                break;
            }
            
            case 5: {
                // JOIN GROUP
                if (!logged_in) {
                    NotificationSystem::error("Please login first to join a group!");
                    break;
                }
                
                std::cout << BRIGHT_CYAN << BOLD << "  📨 JOIN GROUP" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string group_id;
                NotificationSystem::prompt("Group ID to join");
                std::getline(std::cin, group_id);
                
                if (join_group(group_id)) {
                    NotificationSystem::success("Join request sent for group '" + group_id + "'!");
                    NotificationSystem::info("Waiting for owner approval...");
                } else {
                    NotificationSystem::error("Failed to send join request!");
                }
                break;
            }
            
            case 6: {
                // LEAVE GROUP
                if (!logged_in) {
                    NotificationSystem::error("Please login first!");
                    break;
                }
                
                std::cout << BRIGHT_YELLOW << BOLD << "  👋 LEAVE GROUP" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string group_id;
                NotificationSystem::prompt("Group ID to leave");
                std::getline(std::cin, group_id);
                
                if (leave_group(group_id)) {
                    NotificationSystem::success("Left group '" + group_id + "' successfully!");
                } else {
                    NotificationSystem::error("Failed to leave group!");
                }
                break;
            }
            
            case 7: {
                // LIST GROUPS
                std::cout << BRIGHT_CYAN << BOLD << "  🌐 ALL GROUPS" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                list_groups();
                break;
            }
            
            case 8: {
                // LIST REQUESTS
                if (!logged_in) {
                    NotificationSystem::error("Please login first!");
                    break;
                }
                
                std::cout << BRIGHT_WHITE << BOLD << "  📋 PENDING REQUESTS" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string group_id;
                NotificationSystem::prompt("Group ID");
                std::getline(std::cin, group_id);
                list_requests(group_id);
                break;
            }
            
            case 9: {
                // ACCEPT REQUEST
                if (!logged_in) {
                    NotificationSystem::error("Please login first!");
                    break;
                }
                
                std::cout << BRIGHT_GREEN << BOLD << "  ✅ ACCEPT REQUEST" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string group_id, username;
                NotificationSystem::prompt("Group ID");
                std::getline(std::cin, group_id);
                NotificationSystem::prompt("Username to accept");
                std::getline(std::cin, username);
                
                if (accept_request(group_id, username)) {
                    NotificationSystem::success("Request accepted for user '" + username + "'!");
                } else {
                    NotificationSystem::error("Failed to accept request!");
                }
                break;
            }
            
            case 10: {
                // LIST FILES
                if (!logged_in) {
                    NotificationSystem::error("Please login first!");
                    break;
                }
                
                std::cout << BRIGHT_BLUE << BOLD << "  📁 GROUP FILES" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string group_id;
                NotificationSystem::prompt("Group ID");
                std::getline(std::cin, group_id);
                list_files(group_id);
                break;
            }
            
            case 11: {
                // UPLOAD FILE
                if (!logged_in) {
                    NotificationSystem::error("Please login first to upload files!");
                    break;
                }
                
                std::cout << BRIGHT_RED << BOLD << "  📤 FILE UPLOAD" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string filepath, group_id;
                NotificationSystem::prompt("File path (e.g., ./test_file.txt)");
                std::getline(std::cin, filepath);
                NotificationSystem::prompt("Group ID");
                std::getline(std::cin, group_id);
                
                NotificationSystem::info("Starting file upload...");
                
                if (upload_file(filepath, group_id)) {
                    NotificationSystem::success("File uploaded successfully!");
                } else {
                    NotificationSystem::error("Upload failed!");
                }
                break;
            }
            
            case 12: {
                // DOWNLOAD FILE
                if (!logged_in) {
                    NotificationSystem::error("Please login first to download files!");
                    break;
                }
                
                std::cout << BRIGHT_MAGENTA << BOLD << "  📥 DOWNLOAD FILE" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string group_id, filename, dest_path;
                NotificationSystem::prompt("Group ID");
                std::getline(std::cin, group_id);
                NotificationSystem::prompt("Filename");
                std::getline(std::cin, filename);
                NotificationSystem::prompt("Destination path (e.g., .)");
                std::getline(std::cin, dest_path);
                
                NotificationSystem::info("Starting download...");
                if (download_file(group_id, filename, dest_path)) {
                    NotificationSystem::success("Download started!");
                } else {
                    NotificationSystem::error("Download failed!");
                }
                break;
            }
            
            case 13: {
                // STOP SHARING
                if (!logged_in) {
                    NotificationSystem::error("Please login first!");
                    break;
                }
                
                std::cout << BRIGHT_RED << BOLD << "  🛑 STOP SHARING" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                
                std::string group_id, filename;
                NotificationSystem::prompt("Group ID");
                std::getline(std::cin, group_id);
                NotificationSystem::prompt("Filename");
                std::getline(std::cin, filename);
                
                if (stop_share(group_id, filename)) {
                    NotificationSystem::success("Stopped sharing '" + filename + "'!");
                } else {
                    NotificationSystem::error("Failed to stop sharing!");
                }
                break;
            }
            
            case 14: {
                // SHOW DOWNLOADS
                if (!logged_in) {
                    NotificationSystem::error("Please login first!");
                    break;
                }
                
                std::cout << BRIGHT_YELLOW << BOLD << "  📊 ACTIVE DOWNLOADS" << RESET << std::endl;
                std::cout << "  " << std::string(50, '-') << std::endl;
                show_downloads();
                break;
            }
            
            case 0:
                // EXIT
                ProfessionalUI::clear_screen();
                std::cout << BRIGHT_MAGENTA << BOLD;
                std::cout << "  ╔════════════════════════════════════════╗" << std::endl;
                std::cout << "  ║                                        ║" << std::endl;  
                std::cout << "  ║    Thanks for using P2P File Sharing   ║" << std::endl;
                std::cout << "  ║            See you soon! 👋            ║" << std::endl;
                std::cout << "  ║                                        ║" << std::endl;
                std::cout << "  ╚════════════════════════════════════════╝" << RESET << std::endl;
                return;
                
            default:
                NotificationSystem::error("Invalid choice! Please enter a number from the menu (0-14).");
        }
        
        // Professional continue prompt
        std::cout << std::endl << BRIGHT_WHITE << "Press Enter to continue..." << RESET;
        std::cin.get();
    }
}

//=================================================================================================
// MAIN FUNCTION
//=================================================================================================
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <IP>:<PORT> <tracker_info.txt>" << std::endl;
        return 1;
    }
    
    std::string address(argv[1]);
    std::string tracker_file(argv[2]);
    
    size_t colon_pos = address.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid address format. Use IP:PORT" << std::endl;
        return 1;
    }
    
    std::string ip = address.substr(0, colon_pos);
    int port = std::stoi(address.substr(colon_pos + 1));
    
    P2PClient client(ip, port);
    
    if (!client.initialize(tracker_file)) {
        std::cerr << "Failed to initialize client" << std::endl;
        return 1;
    }
    
    client.run();
    return 0;
}