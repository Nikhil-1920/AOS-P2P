#ifndef UI_H
#define UI_H

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

// Modern Color Palette
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define ITALIC      "\033[3m"
#define UNDERLINE   "\033[4m"

// Text Colors
#define BLACK       "\033[30m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"

// Bright Colors
#define BRIGHT_BLACK   "\033[90m"
#define BRIGHT_RED     "\033[91m"
#define BRIGHT_GREEN   "\033[92m"
#define BRIGHT_YELLOW  "\033[93m"
#define BRIGHT_BLUE    "\033[94m"
#define BRIGHT_MAGENTA "\033[95m"
#define BRIGHT_CYAN    "\033[96m"
#define BRIGHT_WHITE   "\033[97m"

// #define CHECK    "[OK]"
#define ARROW_RIGHT ">"
#define BULLET      "*"
#define CHECK       "✓"
#define CROSS       "[X]"
#define WARNING     "[!]"
#define INFO        "[i]"
#define BOX_H       "-"

class ProfessionalUI {
public:
    static void clear_screen() {
        std::cout << "\033[2J\033[H";
    }
    
    static std::string center_text(const std::string& text, int width) {
        int padding = (width - text.length()) / 2;
        return std::string(padding > 0 ? padding : 0, ' ') + text;
    }
    
    static void print_header() {
        clear_screen();
        std::cout << BOLD << CYAN;
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║                                                          ║\n";
        std::cout << "║              🚀 P2P FILE SHARING SYSTEM 🚀               ║\n";
        std::cout << "║                                                          ║\n";
        std::cout << "║               Advanced Operating Systems                 ║\n";
        std::cout << "║                       Assignment 3                       ║\n";
        std::cout << "║                                                          ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n";
        std::cout << RESET << "\n";
    }
    
    static void print_status_bar(const std::string& user, const std::string& status) {
        std::cout << BRIGHT_WHITE << "  Status: " << BRIGHT_GREEN << user << RESET;
        std::cout << " | " << (status == "Online" ? BRIGHT_GREEN : BRIGHT_YELLOW) << status << RESET << std::endl;
        std::cout << std::endl;
    }
    
    static void print_menu() {
        std::cout << BRIGHT_WHITE << BOLD << "  " << INFO << " MAIN MENU" << RESET << std::endl;
        std::cout << BRIGHT_BLACK;
        for (int i = 0; i < 60; i++) std::cout << BOX_H;
        std::cout << RESET << std::endl;
        
        // Account Management
        std::cout << BRIGHT_YELLOW << "  Account Management:" << RESET << std::endl;
        print_menu_item("1", "👤 Create User", "Register a new account");
        print_menu_item("2", "🔐 Login", "Sign in to your account");
        print_menu_item("3", "🚪 Logout", "Sign out of your account");
        
        // Group Management
        std::cout << std::endl << BRIGHT_CYAN << "  Group Management:" << RESET << std::endl;
        print_menu_item("4", "👥 Create Group", "Start a new file sharing group");
        print_menu_item("5", "📨 Join Group", "Request to join a group");
        print_menu_item("6", "👋 Leave Group", "Exit from a group");
        print_menu_item("7", "🌐 List Groups", "View all available groups");
        print_menu_item("8", "📋 List Requests", "View pending join requests");
        print_menu_item("9", "✅ Accept Request", "Approve group join requests");
        
        // File Sharing
        std::cout << std::endl << BRIGHT_GREEN << "  File Sharing:" << RESET << std::endl;
        print_menu_item("10", "📁 List Files", "Browse files in a group");
        print_menu_item("11", "📤 Upload File", "Share a file with group");
        print_menu_item("12", "📥 Download File", "Download files from peers");
        print_menu_item("13", "🛑 Stop Sharing", "Stop sharing a file");
        print_menu_item("14", "📊 Show Downloads", "Monitor active transfers");
        
        std::cout << std::endl;
        print_menu_item("0", "❌ Exit", "Close the application");
        
        std::cout << BRIGHT_BLACK;
        for (int i = 0; i < 60; i++) std::cout << BOX_H;
        std::cout << RESET << std::endl;
    }
    
private:
    static void print_menu_item(const std::string& num, const std::string& title, const std::string& desc) {
        std::cout << "  " << BRIGHT_CYAN << "[" << num << "]" << RESET << " " 
                  << BRIGHT_WHITE << title << RESET << std::endl;
        std::cout << "      " << DIM << desc << RESET << std::endl;
    }
};

class NotificationSystem {
public:
    static void success(const std::string& message) {
        std::cout << BRIGHT_GREEN << "  " << CHECK << " " << message << RESET << std::endl;
    }
    
    static void error(const std::string& message) {
        std::cout << BRIGHT_RED << "  " << CROSS << " " << message << RESET << std::endl;
    }
    
    static void warning(const std::string& message) {
        std::cout << BRIGHT_YELLOW << "  " << WARNING << " " << message << RESET << std::endl;
    }
    
    static void info(const std::string& message) {
        std::cout << BRIGHT_CYAN << "  " << INFO << " " << message << RESET << std::endl;
    }
    
    static void prompt(const std::string& message) {
        std::cout << BRIGHT_WHITE << "  " << ARROW_RIGHT << " " << message << ": " << RESET;
    }
};

class LoadingAnimation {
public:
    static void show_progress(const std::string& task, int percentage) {
        std::cout << "\r" << BRIGHT_BLUE << "  " << INFO << " " << task << ": ";
        
        std::cout << "[";
        int filled = percentage / 2;
        for (int i = 0; i < 50; i++) {
            if (i < filled) {
                std::cout << BRIGHT_GREEN << "=" << BRIGHT_BLUE;
            } else {
                std::cout << BRIGHT_BLACK << "-" << BRIGHT_BLUE;
            }
        }
        std::cout << "] " << BRIGHT_WHITE << percentage << "%" << RESET;
        std::cout.flush();
        
        if (percentage >= 100) {
            std::cout << " " << BRIGHT_GREEN << CHECK << " Complete!" << RESET << std::endl;
        }
    }
};

class ProgressBar {
public:
    static void show_download_progress(const std::string& filename, int percentage, 
                                     long downloaded_bytes, long total_bytes, 
                                     const std::string& speed = "0 KB/s") {
        std::cout << "\r" << BRIGHT_CYAN << "📥 " << filename << RESET << std::endl;
        
        // Progress bar with colors
        std::cout << "  [";
        int filled = percentage / 2;    // 50 chars total
        for (int i = 0; i < 50; i++) {
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
        
        // Size information
        std::cout << std::endl;
        std::cout << "  " << BRIGHT_BLUE << format_bytes(downloaded_bytes) << "/" 
                  << format_bytes(total_bytes) << RESET;
        std::cout << " | Speed: " << BRIGHT_MAGENTA << speed << RESET;
        
        if (percentage >= 100) {
            std::cout << " " << BRIGHT_GREEN << CHECK << " Complete!" << RESET;
        }
        
        std::cout << std::endl;
        std::cout.flush();
    }
    
    static void show_piece_progress(int current_piece, int total_pieces, int successful, int failed) {
        std::cout << "\r" << DIM << "  Pieces: " << BRIGHT_WHITE << current_piece << "/" << total_pieces
                  << RESET << DIM << " | Success: " << BRIGHT_GREEN << successful 
                  << RESET << DIM << " | Failed: " << BRIGHT_RED << failed << RESET;
        std::cout.flush();
    }
    
private:
    static std::string format_bytes(long bytes) {
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
};

#endif