#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <signal.h>
#include <chrono>
#include <iomanip>
#include "sha1.h"
#include "ui.h"

#define MAX_BUFFER_SIZE 1024
#define PIECE_SIZE 524288  
#define MAX_CLIENTS 100
#define MAX_GROUPS 50

//=================================================================================================
// DATA STRUCTURES
//=================================================================================================

struct TrackerInfo {
    std::string ip;
    int port;
};

struct PeerInfo {
    std::string ip;
    int port;
    std::string user_id;
};

struct FileInfo {
    std::string filename;
    std::string file_hash;
    std::vector<std::string> piece_hashes;
    long file_size;
    int total_pieces;
    std::vector<PeerInfo> peers;
    
    // Default constructor
    FileInfo() : file_size(0), total_pieces(0) {}
};

struct DownloadInfo {
    std::string group_id;
    std::string filename;
    std::string dest_path;
    std::vector<bool> pieces_downloaded;
    long total_size;
    long downloaded_size;
    bool is_complete;
    
    // Default constructor
    DownloadInfo() : total_size(0), downloaded_size(0), is_complete(false) {}
    
    // Copy constructor
    DownloadInfo(const DownloadInfo& other) 
        : group_id(other.group_id), filename(other.filename), dest_path(other.dest_path),
          pieces_downloaded(other.pieces_downloaded), total_size(other.total_size),
          downloaded_size(other.downloaded_size), is_complete(other.is_complete) {}
    
    // Assignment operator
    DownloadInfo& operator=(const DownloadInfo& other) {
        if (this != &other) {
            group_id = other.group_id;
            filename = other.filename;
            dest_path = other.dest_path;
            pieces_downloaded = other.pieces_downloaded;
            total_size = other.total_size;
            downloaded_size = other.downloaded_size;
            is_complete = other.is_complete;
        }
        return *this;
    }
};

struct DownloadState {
    std::string filename;
    int total_pieces;
    int successful_pieces;
    int failed_pieces;
    long total_bytes;
    long downloaded_bytes;
    std::chrono::steady_clock::time_point start_time;
    bool show_detailed_logs;
    std::mutex progress_mutex;
    
    // Default constructor
    DownloadState() : total_pieces(0), successful_pieces(0), failed_pieces(0),
                     total_bytes(0), downloaded_bytes(0), show_detailed_logs(false) {}
};

struct ProgressStats {
    int percentage;
    long downloaded_bytes;
    long total_bytes;
    std::string speed;
    int successful_pieces;
    int failed_pieces;
    std::chrono::steady_clock::time_point last_update;
    
    // Default constructor
    ProgressStats() : percentage(0), downloaded_bytes(0), total_bytes(0),
                     speed("0 KB/s"), successful_pieces(0), failed_pieces(0) {}
};

//=================================================================================================
// CLIENT CLASS
//=================================================================================================

class P2PClient {
private:
    // Core member variables
    std::string my_ip;
    int my_port;
    std::string user_id;
    bool logged_in;
    int server_socket;
    std::vector<TrackerInfo> trackers;
    std::map<std::string, DownloadInfo> active_downloads;
    std::set<std::string> shared_files;
    std::mutex client_mutex;
    std::mutex download_mutex;
    std::thread server_thread;
    bool running;
    
    // Progress tracking
    std::map<std::string, ProgressStats> download_progress;
    std::mutex progress_mutex;
    
    //=============================================================================================
    // PRIVATE HELPER METHODS
    //=============================================================================================
    
    // UI and Display Functions
    void print_header();
    void print_menu();
    void clear_screen();
    void print_colored(const std::string& text, const std::string& color);
    void print_separator();
    void print_success(const std::string& message);
    void print_error(const std::string& message);
    void print_info(const std::string& message);
    
    // Network Communication
    bool connect_to_tracker(int& tracker_socket);
    bool send_to_tracker(int socket, const std::string& message);
    std::string receive_from_tracker(int socket);
    void start_server();
    void handle_peer_connection(int peer_socket);
    bool test_peer_connection(const PeerInfo& peer);
    
    // File Operations
    std::string calculate_file_hash(const std::string& filepath);
    std::vector<std::string> calculate_piece_hashes(const std::string& filepath);
    
    // Download Operations
    bool download_piece_from_peer(const PeerInfo& peer, const std::string& filename, 
                                  int piece_index, const std::string& dest_path);
    void piece_selection_algorithm(const FileInfo& file_info, const std::string& dest_path);
    
    // Utility Functions
    std::vector<std::string> split_string(const std::string& str, char delimiter);
    
    // Progress Tracking (Private)
    void update_download_progress(const std::string& filename, const ProgressStats& stats);
    void cleanup_download_progress(const std::string& filename);

public:
    //=============================================================================================
    // CONSTRUCTOR & DESTRUCTOR
    //=============================================================================================
    P2PClient(const std::string& ip, int port);
    ~P2PClient();
    
    //=============================================================================================
    // CORE SYSTEM FUNCTIONS
    //=============================================================================================
    bool initialize(const std::string& tracker_file);
    void run();
    
    //=============================================================================================
    // PROGRESS & UI HELPER FUNCTIONS
    //=============================================================================================
    
    // Formatting Functions
    std::string format_speed(long bytes_per_sec);
    std::string format_bytes_static(long bytes);
    std::string format_time(long seconds);
    
    // Progress Display Functions
    void show_download_progress_inline(const std::string& filename, int percentage, 
                                     long downloaded_bytes, long total_bytes, 
                                     const std::string& speed = "0 KB/s");
    
    void show_download_progress_detailed(const std::string& filename, int percentage,
                                       long downloaded_bytes, long total_bytes,
                                       const std::string& speed, int successful_pieces,
                                       int failed_pieces, long elapsed_seconds);
    
    void show_completion_summary(const std::string& filename, long total_bytes,
                               int total_pieces, long elapsed_seconds);
    
    // Progress Bar Functions
    void draw_progress_bar(int percentage, int width = 50);
    void draw_compact_progress_bar(int percentage, int width = 25);
    
    //=============================================================================================
    // USER ACCOUNT MANAGEMENT
    //=============================================================================================
    bool create_user(const std::string& username, const std::string& password);
    bool login(const std::string& username, const std::string& password);
    bool logout();
    
    //=============================================================================================
    // GROUP MANAGEMENT
    //=============================================================================================
    bool create_group(const std::string& group_id);
    bool join_group(const std::string& group_id);
    bool leave_group(const std::string& group_id);
    bool list_groups();
    bool list_requests(const std::string& group_id);
    bool accept_request(const std::string& group_id, const std::string& username);
    
    //=============================================================================================
    // FILE SHARING OPERATIONS
    //=============================================================================================
    bool list_files(const std::string& group_id);
    bool upload_file(const std::string& filepath, const std::string& group_id);
    bool download_file(const std::string& group_id, const std::string& filename, 
                      const std::string& dest_path);
    bool stop_share(const std::string& group_id, const std::string& filename);
    
    //=============================================================================================
    // DOWNLOAD MONITORING
    //=============================================================================================
    bool show_downloads();
    bool show_download_details(const std::string& filename);
    void cancel_download(const std::string& filename);
    
    //=============================================================================================
    // UTILITY & DEBUG FUNCTIONS
    //=============================================================================================
    void enable_debug_mode(bool enable = true);
    void show_system_stats();
    void show_peer_connections();
    std::vector<std::string> get_active_downloads();
    
    //=============================================================================================
    // ADVANCED FEATURES
    //=============================================================================================
    
    // Multi-threaded Download Support
    void set_max_concurrent_downloads(int max_downloads);
    void set_download_timeout(int seconds);
    
    // Bandwidth Management
    void set_download_speed_limit(long bytes_per_sec);
    void set_upload_speed_limit(long bytes_per_sec);
    
    // File Verification
    bool verify_file_integrity(const std::string& filepath, const std::string& expected_hash);
    bool verify_downloaded_file(const std::string& filename);
    
    // Statistics
    struct NetworkStats {
        long total_bytes_downloaded;
        long total_bytes_uploaded;
        int successful_downloads;
        int failed_downloads;
        std::chrono::steady_clock::time_point session_start;
        
        NetworkStats() : total_bytes_downloaded(0), total_bytes_uploaded(0),
                        successful_downloads(0), failed_downloads(0) {}
    };
    
    NetworkStats get_network_stats();
    void reset_network_stats();
    
private:
    // Advanced feature member variables
    bool debug_mode;
    int max_concurrent_downloads;
    int download_timeout_seconds;
    long download_speed_limit;
    long upload_speed_limit;
    NetworkStats network_stats;
};

//=================================================================================================
// INLINE UTILITY FUNCTIONS (HEADER-ONLY)
//=================================================================================================

// Color formatting helpers
inline std::string colorize_text(const std::string& text, const std::string& color) {
    return color + text + "\033[0m";
}

inline std::string make_bold(const std::string& text) {
    return "\033[1m" + text + "\033[0m";
}

// Progress calculation helpers
inline int calculate_percentage(long current, long total) {
    if (total <= 0) return 0;
    return static_cast<int>((current * 100) / total);
}

inline double calculate_speed(long bytes, long seconds) {
    if (seconds <= 0) return 0.0;
    return static_cast<double>(bytes) / seconds;
}

// File size validation
inline bool is_valid_file_size(long size) {
    return size > 0 && size <= (50LL * 1024 * 1024 * 1024); // Max 50GB
}

#endif // CLIENT_H