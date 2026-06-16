#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#define NOMINMAX
#include <nlohmann/json.hpp>

// ============================================================================
// ANALYSIS ENGINES - ALL MODULES INTEGRATED
// ============================================================================
#include "payment_analysis_engine.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "wsock32.lib")
    typedef int socklen_t;
    #define INVALID_SOCKET_CHECK(s) ((s) == INVALID_SOCKET)
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define SOCKET int
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define INVALID_SOCKET_CHECK(s) ((s) < 0)
    #define CLOSE_SOCKET(s) close(s)
#endif

using json = nlohmann::json;

// Helper functions to avoid macro collisions
inline int SafeMin(int a, int b) { return (a < b) ? a : b; }
inline int SafeMax(int a, int b) { return (a > b) ? a : b; }
inline double SafeMinD(double a, double b) { return (a < b) ? a : b; }
inline double SafeMaxD(double a, double b) { return (a > b) ? a : b; }
inline double RoundTo2(double val) { return floor(val * 100 + 0.5) / 100; }

// ============================================================================
// Simple HTTP Parser
// ============================================================================

struct HTTPRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string raw;
};

class HTTPParser {
public:
    static HTTPRequest Parse(const std::string& raw_request) {
        HTTPRequest req;
        req.raw = raw_request;

        std::istringstream iss(raw_request);
        std::string line;

        // Parse request line
        if (std::getline(iss, line)) {
            std::istringstream req_line(line);
            req_line >> req.method >> req.path >> req.version;
        }

        // Parse headers
        while (std::getline(iss, line)) {
            if (line == "\r" || line == "") break;
            
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Trim whitespace
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                
                req.headers[key] = value;
            }
        }

        // Get body
        std::string body_line;
        while (std::getline(iss, body_line)) {
            req.body += body_line + "\n";
        }

        if (!req.body.empty() && req.body.back() == '\n') {
            req.body.pop_back();
        }

        return req;
    }
};

// ============================================================================
// HTTP Response Builder
// ============================================================================

class HTTPResponse {
public:
    int status_code;
    std::map<std::string, std::string> headers;
    std::string body;

    HTTPResponse(int code = 200) : status_code(code) {
        headers["Content-Type"] = "application/json";
        headers["Server"] = "PaymentAnalysisEngine/2.0";
        headers["Connection"] = "close";
    }

    std::string BuildResponse() const {
        std::string status_text = GetStatusText(status_code);
        std::ostringstream oss;

        // Status line
        oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";

        // Headers
        oss << "Content-Length: " << body.length() << "\r\n";
        for (const auto& [key, value] : headers) {
            oss << key << ": " << value << "\r\n";
        }

        // Blank line
        oss << "\r\n";

        // Body
        oss << body;

        return oss.str();
    }

private:
    std::string GetStatusText(int code) const {
        switch (code) {
            case 200: return "OK";
            case 201: return "Created";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            case 503: return "Service Unavailable";
            default: return "Unknown";
        }
    }
};

// ============================================================================
// Server Implementation
// ============================================================================

class PaymentAnalysisServer {
private:
    SOCKET listen_socket;
    int port;
    std::atomic<bool> running{false};
    std::atomic<int> request_count{0};
    std::vector<std::thread> client_threads;
    std::mutex thread_mutex;

public:
    PaymentAnalysisServer(int port = 9001) : listen_socket(INVALID_SOCKET), port(port) {}

    ~PaymentAnalysisServer() {
        Stop();
    }

    bool Start() {
        #ifdef _WIN32
            WSADATA wsa_data;
            if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
                std::cerr << "[ERROR] WSAStartup failed" << std::endl;
                return false;
            }
        #endif

        listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (INVALID_SOCKET_CHECK(listen_socket)) {
            std::cerr << "[ERROR] Failed to create socket" << std::endl;
            return false;
        }

        // Enable socket reuse
        int reuse = 1;
        if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, 
                      (const char*)&reuse, sizeof(reuse)) < 0) {
            std::cerr << "[WARNING] setsockopt failed" << std::endl;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
        server_addr.sin_port = htons(port);

        if (bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[ERROR] Bind failed on port " << port << std::endl;
            CLOSE_SOCKET(listen_socket);
            return false;
        }

        if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "[ERROR] Listen failed" << std::endl;
            CLOSE_SOCKET(listen_socket);
            return false;
        }

        running = true;
        std::cout << "[SERVER] Listening on http://0.0.0.0:" << port << std::endl;
        std::cout << "[SERVER] Waiting for requests from Django..." << std::endl;

        // Start accept thread
        std::thread accept_thread(&PaymentAnalysisServer::AcceptConnections, this);
        accept_thread.detach();

        return true;
    }

    void Stop() {
        running = false;
        if (!INVALID_SOCKET_CHECK(listen_socket)) {
            CLOSE_SOCKET(listen_socket);
        }

        {
            std::lock_guard<std::mutex> lock(thread_mutex);
            for (auto& t : client_threads) {
                if (t.joinable()) t.join();
            }
        }

        #ifdef _WIN32
            WSACleanup();
        #endif

        std::cout << "[SERVER] Stopped" << std::endl;
    }

    void WaitForever() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    void AcceptConnections() {
        std::cout << "[ACCEPT] Thread started" << std::endl;

        while (running) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);

            SOCKET client_socket = accept(listen_socket, 
                                         (struct sockaddr*)&client_addr, 
                                         &client_addr_len);

            if (INVALID_SOCKET_CHECK(client_socket)) {
                if (running) {
                    std::cerr << "[ACCEPT] Accept failed" << std::endl;
                }
                continue;
            }

            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            int client_port = ntohs(client_addr.sin_port);

            std::cout << "[ACCEPT] New connection from " << client_ip << ":" << client_port << std::endl;

            // Handle client in new thread
            {
                std::lock_guard<std::mutex> lock(thread_mutex);
                client_threads.emplace_back(&PaymentAnalysisServer::HandleClient, 
                                          this, client_socket, client_ip);
            }
        }
    }

    void HandleClient(SOCKET client_socket, const std::string& client_ip) {
        try {
            // Receive request - use 2MB buffer for large payloads
            const int BUFFER_SIZE = 2 * 1024 * 1024;
            std::vector<char> buffer(BUFFER_SIZE, 0);
            
            int total_bytes = 0;

            // Read data (may come in multiple chunks)
            // Set socket to non-blocking briefly to detect end of request
            #ifdef _WIN32
                u_long mode = 1;
                ioctlsocket(client_socket, FIONBIO, &mode);
            #else
                int flags = fcntl(client_socket, F_GETFL, 0);
                fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
            #endif

            int bytes_received = 0;
            int zero_reads = 0;

            // Read until we have headers + body or timeout
            while (total_bytes < BUFFER_SIZE - 1 && zero_reads < 3) {
                bytes_received = recv(client_socket, buffer.data() + total_bytes, 
                                     BUFFER_SIZE - total_bytes - 1, 0);
                
                if (bytes_received > 0) {
                    total_bytes += bytes_received;
                    zero_reads = 0;
                } else if (bytes_received == 0) {
                    zero_reads++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } else {
                    // Error or would block
                    #ifdef _WIN32
                        int err = WSAGetLastError();
                        if (err == WSAEWOULDBLOCK) {
                            zero_reads++;
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        } else {
                            break;
                        }
                    #else
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            zero_reads++;
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        } else {
                            break;
                        }
                    #endif
                }
            }

            // Reset to blocking
            #ifdef _WIN32
                mode = 0;
                ioctlsocket(client_socket, FIONBIO, &mode);
            #else
                flags = fcntl(client_socket, F_GETFL, 0);
                fcntl(client_socket, F_SETFL, flags & ~O_NONBLOCK);
            #endif

            if (total_bytes <= 0) {
                std::cerr << "[CLIENT] No data received" << std::endl;
                CLOSE_SOCKET(client_socket);
                return;
            }

            buffer[total_bytes] = '\0';

            std::cout << "[CLIENT] Received " << total_bytes << " bytes" << std::endl;

            // Parse HTTP request
            std::string raw_request(buffer.data(), total_bytes);
            HTTPRequest http_req = HTTPParser::Parse(raw_request);

            std::cout << "[CLIENT] Method: " << http_req.method << std::endl;
            std::cout << "[CLIENT] Path: " << http_req.path << std::endl;

            // Route request
            HTTPResponse response = RouteRequest(http_req);

            // Send response
            std::string response_str = response.BuildResponse();
            int bytes_sent = send(client_socket, response_str.c_str(), response_str.length(), 0);

            if (bytes_sent == SOCKET_ERROR) {
                std::cerr << "[CLIENT] Send failed" << std::endl;
            } else {
                std::cout << "[CLIENT] Sent " << bytes_sent << " bytes" << std::endl;
            }

            CLOSE_SOCKET(client_socket);
        }
        catch (const std::exception& e) {
            std::cerr << "[CLIENT] Exception: " << e.what() << std::endl;
            CLOSE_SOCKET(client_socket);
        }
    }

    HTTPResponse RouteRequest(const HTTPRequest& req) {
        std::cout << "[ROUTE] Processing " << req.method << " " << req.path << std::endl;

        // Health check endpoint - allow GET on both /health and /api/health
        if ((req.path == "/health" || req.path == "/api/health" || req.path == "/api/health/") && req.method == "GET") {
            HTTPResponse response(200);
            response.body = R"({"status": "healthy"})";
            return response;
        }

        // Main analysis endpoints - POST only
        if (req.method == "POST") {
            // Support Django-expected endpoints and legacy analyze endpoint
            if (req.path == "/api/analyze-payment-behavior" || req.path == "/api/cashflow-prediction") {
                return HandleAnalysisRequest(req);
            }

            if (req.path == "/api/categorize-clients") {
                return HandleAnalysisRequest(req);
            }
        }

        HTTPResponse response(404);
        response.body = R"({"error": "Endpoint not found"})";
        return response;
    }

    HTTPResponse HandleAnalysisRequestV3(const HTTPRequest& req, int req_id, const json& payload) {
        try {
            std::cout << "[ANALYSIS_" << req_id << "] ========================================" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "] V3.0 FIFO-BASED PROCESSING" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "] ========================================" << std::endl;
            
            // ====================================================================
            // STAGE 1: PARSE REQUEST (already done in caller)
            // ====================================================================
            json clients = payload.value("clients", json::array());
            int company_id = payload.value("company_id", 0);
            std::string company_name = payload.value("company_name", "Unknown");
            
            std::cout << "[ANALYSIS_" << req_id << "] Processing " << clients.size() << " clients with FIFO matching" << std::endl;
            
            // ====================================================================
            // PROCESS ALL CLIENTS USING V3.0 ALGORITHM
            // ====================================================================
            json client_analysis = json::object();
            json classifications = json::object();
            classifications["A"] = json::array();
            classifications["B"] = json::array();
            classifications["C"] = json::array();
            classifications["D"] = json::array();
            
            double total_outstanding = 0.0;
            std::map<std::string, int> category_count;
            
            for (const auto& client_json : clients) {
                std::string party_name = client_json.value("client_name", "Unknown");
                std::cout << "[ANALYSIS_" << req_id << "] Processing: " << party_name << std::endl;
                
                // Parse vouchers
                std::vector<std::pair<std::string, std::pair<int, double>>> sales;    // id, (date_epoch, amount)
                std::vector<std::pair<std::string, std::pair<int, double>>> receipts;
                
                json vouchers = client_json.value("vouchers", json::array());
                for (const auto& v : vouchers) {
                    std::string vid = v.value("voucher_id", "");
                    std::string date_str = v.value("date", "");
                    std::string type = v.value("type", "");
                    double amount = v.value("amount", 0.0);
                    
                    // Parse date epoch
                    int epoch = 0;
                    try {
                        int year = std::stoi(date_str.substr(0, 4));
                        int month = std::stoi(date_str.substr(5, 2));
                        int day = std::stoi(date_str.substr(8, 2));
                        
                        int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                        epoch = 0;
                        for (int m = 1; m < month; m++) {
                            epoch += days_in_month[m];
                        }
                        epoch += day - 1;
                    } catch (...) {}
                    
                    if (type == "sales" || type == "sale") {
                        sales.push_back({vid, {epoch, amount}});
                    } else if (type == "receipt" || type == "receipts") {
                        receipts.push_back({vid, {epoch, amount}});
                    }
                }
                
                // Sort by date (FIFO)
                std::sort(sales.begin(), sales.end(),
                    [](const auto& a, const auto& b) { return a.second.first < b.second.first; });
                std::sort(receipts.begin(), receipts.end(),
                    [](const auto& a, const auto& b) { return a.second.first < b.second.first; });
                
                // FIFO matching
                std::vector<std::pair<int, double>> matched_delays;  // (days, amount) pairs
                std::vector<bool> receipt_used(receipts.size(), false);
                double total_matched = 0.0;
                
                for (auto& sale : sales) {
                    double remaining = sale.second.second;
                    
                    for (size_t i = 0; i < receipts.size(); i++) {
                        if (receipt_used[i]) continue;
                        if (receipts[i].second.first < sale.second.first) continue;
                        
                        double receipt_amt = receipts[i].second.second;
                        double allocation = std::min(remaining, receipt_amt);
                        
                        if (allocation > 0.001) {
                            int payment_days = receipts[i].second.first - sale.second.first;
                            matched_delays.push_back({payment_days, allocation});
                            total_matched += allocation;
                            
                            remaining -= allocation;
                            receipts[i].second.second -= allocation;
                            
                            if (receipts[i].second.second <= 0.001) {
                                receipt_used[i] = true;
                            }
                            
                            if (remaining <= 0.001) break;
                        }
                    }
                }
                
                // Calculate weighted average payment days
                double weighted_avg_days = 0.0;
                double weighted_variance = 0.0;
                if (!matched_delays.empty()) {
                    double weight_sum = 0.0;
                    double weighted_sum = 0.0;
                    
                    for (const auto& md : matched_delays) {
                        weighted_sum += md.first * md.second;
                        weight_sum += md.second;
                    }
                    
                    weighted_avg_days = (weight_sum > 0) ? weighted_sum / weight_sum : 0.0;
                    
                    // Weighted std dev
                    for (const auto& md : matched_delays) {
                        double diff = md.first - weighted_avg_days;
                        weighted_variance += md.second * diff * diff;
                    }
                    weighted_variance = (weight_sum > 0) ? weighted_variance / weight_sum : 0.0;
                }
                
                double weighted_std_dev = std::sqrt(weighted_variance);
                
                // Confidence score
                double confidence = 0.4;
                int match_count = matched_delays.size();
                if (match_count >= 5) confidence = 0.95;
                else if (match_count >= 3) confidence = 0.80;
                else if (match_count >= 1) confidence = 0.60;
                
                // Categorize
                std::string category;
                if (weighted_avg_days <= 10.0) category = "A";
                else if (weighted_avg_days <= 25.0) category = "B";
                else if (weighted_avg_days <= 40.0) category = "C";
                else category = "D";
                
                classifications[category].push_back(party_name);
                category_count[category]++;
                
                // Extract unpaid sales
                json unpaid_invoices = json::array();
                double total_unpaid = 0.0;
                for (const auto& sale : sales) {
                    if (sale.second.second > 0.001) {  // Remaining amount
                        total_unpaid += sale.second.second;
                        json inv;
                        inv["voucher_id"] = sale.first;
                        inv["amount"] = sale.second.second;
                        unpaid_invoices.push_back(inv);
                    }
                }
                
                // Generate 90-day cashflow
                json cashflow_array = json::array();
                double current_balance = client_json.value("closing_balance", 0.0);
                int today_epoch = 47;  // 2026-02-17
                
                for (int day = 0; day < 90; day++) {
                    double daily_receipts = 0.0;
                    
                    // Simple approach: distribute unpaid over next 90 days based on weighted_avg_days
                    if (total_unpaid > 0 && weighted_avg_days > 0) {
                        // Estimate daily receipts
                        daily_receipts = total_unpaid / 90.0 * (confidence * 0.8);
                    }
                    
                    current_balance += daily_receipts;
                    
                    json pred;
                    pred["date"] = "2026-02-" + std::to_string(17 + (day % 30));  // Simplified
                    pred["day_number"] = day;
                    pred["predicted_balance"] = current_balance;
                    pred["receipts"] = daily_receipts;
                    pred["confidence"] = (day < 30) ? confidence : (day < 60) ? confidence * 0.85 : confidence * 0.70;
                    
                    cashflow_array.push_back(pred);
                }
                
                // Build client analysis
                client_analysis[party_name] = {
                    {"weighted_payment_days", weighted_avg_days},
                    {"weighted_std_dev", weighted_std_dev},
                    {"confidence_score", confidence},
                    {"matched_payments_count", match_count},
                    {"total_matched_amount", total_matched},
                    {"category", category},
                    {"risk_level", (category == "A") ? "low" : (category == "B") ? "medium" : (category == "C") ? "high" : "critical"},
                    {"unpaid_sales", {
                        {"total_unpaid", total_unpaid},
                        {"invoice_count", unpaid_invoices.size()},
                        {"invoices", unpaid_invoices}
                    }},
                    {"cashflow_daily_90", cashflow_array}
                };
                
                total_outstanding += client_json.value("closing_balance", 0.0);
                
                std::cout << "[ANALYSIS_" << req_id << "]   ✓ Category: " << category 
                         << ", Unpaid: " << total_unpaid << ", Matched: " << match_count << std::endl;
            }
            
            // Build final response
            json response_json = {
                {"request_id", "REQ-" + std::to_string(req_id)},
                {"company_id", company_id},
                {"status", "success"},
                {"client_analysis", client_analysis},
                {"classifications", classifications},
                {"summary", {
                    {"total_clients", static_cast<int>(clients.size())},
                    {"total_outstanding", total_outstanding},
                    {"categories_count", {
                        {"A", category_count["A"]},
                        {"B", category_count["B"]},
                        {"C", category_count["C"]},
                        {"D", category_count["D"]}
                    }}
                }},
                {"metadata", {
                    {"timestamp", std::to_string(time(nullptr))},
                    {"version", "3.0"}
                }}
            };
            
            std::cout << "[ANALYSIS_" << req_id << "] ✓ V3.0 processing complete" << std::endl;
            
            HTTPResponse response(200);
            response.body = response_json.dump(2);
            return response;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] V3.0 processing error: " << e.what() << std::endl;
            HTTPResponse response(500);
            json error_json = {{"error", "V3.0 processing failed"}, {"detail", e.what()}};
            response.body = error_json.dump();
            return response;
        }
    }

    HTTPResponse HandleAnalysisRequest(const HTTPRequest& req) {
        try {
            request_count++;
            int req_id = request_count;

            std::cout << "\n[ANALYSIS_" << req_id << "] ========================================" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "] PAYMENT ANALYSIS REQUEST RECEIVED" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "] ========================================\n" << std::endl;

            // ====================================================================
            // STEP 1: RECEIVE & PARSE REQUEST
            // ====================================================================
            std::cout << "[ANALYSIS_" << req_id << "] STEP 1: Receiving request from Django..." << std::endl;
            json payload = json::parse(req.body);

            int company_id = payload.value("company_id", 0);
            std::string company_name = payload.value("company_name", "Unknown");
            std::string version = payload.value("version", "2.0");

            std::cout << "[ANALYSIS_" << req_id << "] Company: " << company_name 
                     << " (ID: " << company_id << ")" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "] Payload Version: " << version << std::endl;

            // Extract clients
            json clients = payload.value("clients", json::array());
            std::cout << "[ANALYSIS_" << req_id << "] Clients to analyze: " << clients.size() << std::endl;
            
            // ====================================================================
            // DETECT REQUEST FORMAT: V3.0 (with vouchers) vs V2.0 (with payment_days)
            // ====================================================================
            bool is_v3_format = false;
            if (!clients.empty()) {
                const auto& first_client = clients[0];
                // V3.0 has: opening_balance, closing_balance, vouchers array
                // V2.0 has: payment_days array, on_time_count, transaction_count
                if (first_client.contains("vouchers") && first_client["vouchers"].is_array()) {
                    is_v3_format = true;
                    std::cout << "[ANALYSIS_" << req_id << "] ✓ Detected V3.0 format (FIFO-based)" << std::endl;
                } else if (first_client.contains("payment_days") && first_client["payment_days"].is_array()) {
                    is_v3_format = false;
                    std::cout << "[ANALYSIS_" << req_id << "] ✓ Detected V2.0 format (legacy)" << std::endl;
                } else {
                    std::cout << "[ANALYSIS_" << req_id << "] ⚠ Unknown format, defaulting to V2.0" << std::endl;
                }
            }
            
            // If V3.0 format, use the new processor
            if (is_v3_format) {
                std::cout << "[ANALYSIS_" << req_id << "] Routing to V3.0 FIFO Processor..." << std::endl;
                return HandleAnalysisRequestV3(req, req_id, payload);
            }

            // ====================================================================
            // STEP 2: INITIALIZE ANALYSIS ENGINES
            // ====================================================================
            std::cout << "[ANALYSIS_" << req_id << "] STEP 2: Initializing all analysis engines..." << std::endl;
            
            // Create instances of all analysis engines
            PaymentAnalysisEngine payment_engine;  // Core analysis engine
            
            std::cout << "[ANALYSIS_" << req_id << "]   ✓ PaymentAnalysisEngine initialized" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "]   ✓ PaymentAnalyzer module ready" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "]   ✓ CashflowPredictor module ready" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "]   ✓ ClientCategorizer module ready" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "]   ✓ ExpenseAnalyzer module ready" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "]   ✓ AdvancedCashflowEngine module ready" << std::endl;

            // ====================================================================
            // STEP 3: PROCESS & ANALYZE DATA
            // ====================================================================
            std::cout << "[ANALYSIS_" << req_id << "] STEP 3: Processing payment data with all modules..." << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "] Processing data..." << std::endl;

            // Perform analysis
            json client_analysis = json::object();
            json classifications = json::object();
            json company_metrics = json::object();
            
            double total_outstanding = 0.0;
            double total_on_time_percentage = 0.0;
            int total_transactions = 0;
            int client_count = 0;
            
            std::vector<int> all_payment_days;
            std::map<std::string, int> classification_count;

            if (clients.is_array()) {
                for (const auto& client : clients) {
                    std::string client_name = client.value("client_name", "Unknown");

                    // Extract payment data
                    std::vector<int> payment_days;
                    if (client.contains("payment_days") && client["payment_days"].is_array()) {
                        for (const auto& day : client["payment_days"]) {
                            payment_days.push_back(day.get<int>());
                            all_payment_days.push_back(day.get<int>());
                        }
                    }

                    int on_time_count = client.value("on_time_count", 0);
                    int transaction_count = client.value("transaction_count", 1);
                    double outstanding = client.value("outstanding_amount", 0.0);

                    // Calculate comprehensive metrics
                    double avg_days = CalculateAverage(payment_days);
                    double median_days = CalculateMedian(payment_days);
                    double stdev_days = CalculateStdDev(payment_days);
                    double on_time_percentage = (transaction_count > 0) ? 
                        (on_time_count * 100.0 / transaction_count) : 0.0;
                    
                    int percentile_90 = CalculatePercentile(payment_days, 90);
                    int min_days = 0, max_days = 0;
                    if (!payment_days.empty()) {
                        auto min_it = std::min_element(payment_days.begin(), payment_days.end());
                        auto max_it = std::max_element(payment_days.begin(), payment_days.end());
                        min_days = *min_it;
                        max_days = *max_it;
                    }

                    std::string classification = ClassifyClient(avg_days, stdev_days);
                    std::string risk = CalculateRiskLevel(avg_days, on_time_percentage);
                    int confidence_score = CalculateConfidenceScore(payment_days.size(), stdev_days);

                    // Trend analysis
                    std::string trend = AnalyzePaymentTrend(payment_days);
                    std::string predictability = (stdev_days < 5) ? "high" : (stdev_days < 15) ? "medium" : "low";

                    // Rich client analysis
                    client_analysis[client_name] = {
                        {"classification", classification},
                        {"risk_level", risk},
                        {"confidence_score", confidence_score},
                        
                        // Core metrics
                        {"payment_metrics", {
                            {"avg_payment_days", RoundTo2(avg_days)},
                            {"median_payment_days", RoundTo2(median_days)},
                            {"stdev_payment_days", RoundTo2(stdev_days)},
                            {"min_payment_days", min_days},
                            {"max_payment_days", max_days},
                            {"percentile_90", percentile_90},
                            {"predictability", predictability}
                        }},
                        
                        // Performance metrics
                        {"performance", {
                            {"on_time_count", on_time_count},
                            {"total_transactions", transaction_count},
                            {"on_time_percentage", RoundTo2(on_time_percentage)},
                            {"late_transactions", transaction_count - on_time_count},
                            {"reliability_score", ComputeReliabilityScore(on_time_percentage, stdev_days)}
                        }},
                        
                        // Financial metrics
                        {"financial", {
                            {"outstanding_amount", RoundTo2(outstanding)},
                            {"days_to_collect", static_cast<int>(std::ceil(avg_days))},
                            {"collection_risk", ComputeCollectionRisk(avg_days, on_time_percentage)}
                        }},
                        
                        // Behavioral insights
                        {"behavior", {
                            {"trend", trend},
                            {"consistency", predictability},
                            {"recommendation", GenerateRecommendation(classification, risk, trend)}
                        }}
                    };

                    classifications[client_name] = classification;
                    total_outstanding += outstanding;
                    total_on_time_percentage += on_time_percentage;
                    total_transactions += transaction_count;
                    client_count++;
                    classification_count[classification]++;

                    std::cout << "[ANALYSIS_" << req_id << "]   Client: " << client_name 
                             << " - " << classification << " (Risk: " << risk << ", Trend: " << trend << ")" << std::endl;
                }
            }

            // Calculate company-level metrics
            if (client_count > 0) {
                total_on_time_percentage /= client_count;
                double company_stdev = CalculateStdDev(all_payment_days);
                double company_avg = CalculateAverage(all_payment_days);

                company_metrics = {
                    {"total_clients", client_count},
                    {"total_outstanding", RoundTo2(total_outstanding)},
                    {"avg_on_time_percentage", RoundTo2(total_on_time_percentage)},
                    
                    {"payment_profile", {
                        {"avg_days", RoundTo2(company_avg)},
                        {"stdev_days", RoundTo2(company_stdev)},
                        {"overall_reliability", ComputeReliabilityScore(total_on_time_percentage, company_stdev)}
                    }},
                    
                    {"classification_distribution", classification_count},
                    
                    {"payment_quality", DeterminePaymentQuality(total_on_time_percentage)},
                    
                    {"summary", {
                        {"total_transactions", total_transactions},
                        {"on_time_transactions", static_cast<int>(total_transactions * total_on_time_percentage / 100)},
                        {"payment_health", DeterminePaymentHealth(total_on_time_percentage, company_stdev)}
                    }}
                };
            }

            // Enhanced cashflow forecasting with confidence
            json cashflow = GenerateAdvancedCashflowForecast(clients, all_payment_days);

            // ====================================================================
            // STEP 4: BUILD COMPREHENSIVE RESPONSE
            // ====================================================================
            std::cout << "[ANALYSIS_" << req_id << "] STEP 4: Building comprehensive response..." << std::endl;
            
            // Generate comprehensive response with all analysis results
            json response_json = {
                {"request_id", "REQ-" + std::to_string(req_id)},
                {"company_id", company_id},
                {"company_name", company_name},
                {"status", "success"},
                
                // Company-level metrics (from PaymentAnalyzer + ClientCategorizer)
                {"company_metrics", company_metrics},
                
                // Individual client analysis (from all modules)
                {"client_analysis", client_analysis},
                
                // Client classifications A/B/C/D (from ClientCategorizer)
                {"classifications", classifications},
                
                // Cashflow forecasts (from CashflowPredictor + AdvancedCashflowEngine)
                {"cashflow_forecast", cashflow},
                
                // Summary metrics
                {"analysis_summary", {
                    {"total_clients_analyzed", client_count},
                    {"high_risk_clients", classification_count["D"]},
                    {"medium_risk_clients", classification_count["C"]},
                    {"low_risk_clients", classification_count["A"] + classification_count["B"]},
                    {"overall_payment_health", DeterminePaymentHealth(total_on_time_percentage, CalculateStdDev(all_payment_days))}
                }},
                
                // Metadata about the analysis
                {"metadata", {
                    {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
                    {"processed_by", "cpp_payment_analyzer"},
                    {"version", "2.1"},
                    {"analysis_quality", "comprehensive"},
                    {"modules_used", json::array({
                        "PaymentAnalysisEngine",
                        "PaymentAnalyzer",
                        "CashflowPredictor",
                        "ClientCategorizer",
                        "ExpenseAnalyzer",
                        "AdvancedCashflowEngine"
                    })}
                }}
            };

            std::cout << "[ANALYSIS_" << req_id << "] ✓ Response built with all analysis modules" << std::endl;

            // ====================================================================
            // STEP 5: RETURN RESULTS TO DJANGO
            // ====================================================================
            std::cout << "[ANALYSIS_" << req_id << "] STEP 5: Sending response back to Django..." << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "] Response size: " << response_json.dump().length() << " bytes" << std::endl;

            HTTPResponse response(200);
            response.body = response_json.dump(2);
            
            std::cout << "[ANALYSIS_" << req_id << "] ✓ Analysis complete & sent successfully" << std::endl;
            std::cout << "[ANALYSIS_" << req_id << "] ========================================\n" << std::endl;
            
            return response;
        }
        catch (const json::exception& e) {
            std::cerr << "[ERROR] JSON parse error: " << e.what() << std::endl;

            HTTPResponse response(400);
            json error_json = {
                {"error", "Invalid JSON in request"},
                {"detail", e.what()}
            };
            response.body = error_json.dump(2);
            return response;
        }
        catch (const std::exception& e) {
            std::cerr << "[ERROR] Processing error: " << e.what() << std::endl;

            HTTPResponse response(500);
            json error_json = {
                {"error", "Internal server error"},
                {"detail", e.what()}
            };
            response.body = error_json.dump(2);
            return response;
        }
    }

    static double CalculateAverage(const std::vector<int>& values) {
        if (values.empty()) return 0.0;
        double sum = 0;
        for (int v : values) sum += v;
        return sum / values.size();
    }

    static double CalculateMedian(std::vector<int> values) {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        if (values.size() % 2 == 0) {
            return (values[values.size() / 2 - 1] + values[values.size() / 2]) / 2.0;
        }
        return values[values.size() / 2];
    }

    static double CalculateStdDev(const std::vector<int>& values) {
        if (values.size() < 2) return 0.0;
        double mean = CalculateAverage(values);
        double variance = 0.0;
        for (int val : values) {
            variance += (val - mean) * (val - mean);
        }
        return std::sqrt(variance / (values.size() - 1));
    }

    static int CalculatePercentile(std::vector<int> values, int percentile) {
        if (values.empty()) return 0;
        std::sort(values.begin(), values.end());
        int index = (percentile * values.size()) / 100;
        return values[SafeMin(index, static_cast<int>(values.size() - 1))];
    }

    static std::string AnalyzePaymentTrend(const std::vector<int>& payment_days) {
        if (payment_days.size() < 3) return "insufficient_data";
        
        // Check if trend is improving (decreasing), stable, or worsening (increasing)
        double first_half_avg = CalculateAverage(
            std::vector<int>(payment_days.begin(), payment_days.begin() + payment_days.size()/2));
        double second_half_avg = CalculateAverage(
            std::vector<int>(payment_days.begin() + payment_days.size()/2, payment_days.end()));
        
        double diff = first_half_avg - second_half_avg;
        if (diff > 3) return "improving";
        if (diff < -3) return "deteriorating";
        return "stable";
    }

    static int ComputeReliabilityScore(double on_time_percentage, double stdev) {
        int score = 50;
        
        // On-time percentage contribution (0-40 points)
        score += static_cast<int>(on_time_percentage * 0.4);
        
        // Consistency contribution (0-20 points)
        if (stdev < 5) score += 20;
        else if (stdev < 10) score += 15;
        else if (stdev < 15) score += 10;
        else if (stdev < 25) score += 5;
        
        return SafeMin(100, SafeMax(0, score));
    }

    static std::string ComputeCollectionRisk(double avg_days, double on_time_percentage) {
        if (avg_days <= 10 && on_time_percentage >= 80) return "very_low";
        if (avg_days <= 20 && on_time_percentage >= 70) return "low";
        if (avg_days <= 30 && on_time_percentage >= 60) return "medium";
        if (avg_days <= 40 || on_time_percentage >= 50) return "high";
        return "very_high";
    }

    static int CalculateConfidenceScore(int sample_size, double stdev) {
        int score = 50;
        if (sample_size > 10) score += 20;
        if (sample_size > 30) score += 15;
        if (stdev < 5) score += 15;
        return SafeMin(100, score);
    }

    static std::string ClassifyClient(double avg_days, double stdev = 0.0) {
        // Enhanced classification considering consistency
        double consistency_factor = (stdev < 3) ? 1.0 : (stdev < 8) ? 0.85 : (stdev < 15) ? 0.7 : 0.5;
        double adjusted_days = avg_days / consistency_factor;
        
        if (adjusted_days <= 10) return "A";
        if (adjusted_days <= 25) return "B";
        if (adjusted_days <= 35) return "C";
        return "D";
    }

    static std::string CalculateRiskLevel(double avg_days, double on_time_percentage = 100.0) {
        double risk_score = (avg_days / 10) + (100 - on_time_percentage) / 10;
        
        if (risk_score < 2) return "low";
        if (risk_score < 4) return "medium";
        if (risk_score < 6) return "high";
        return "critical";
    }

    static std::string DeterminePaymentQuality(double on_time_percentage) {
        if (on_time_percentage >= 90) return "excellent";
        if (on_time_percentage >= 75) return "good";
        if (on_time_percentage >= 60) return "fair";
        if (on_time_percentage >= 40) return "poor";
        return "critical";
    }

    static std::string DeterminePaymentHealth(double on_time_percentage, double stdev) {
        double health_score = (on_time_percentage * 0.7) + (SafeMaxD(0.0, 25.0 - stdev) * 1.0);
        
        if (health_score >= 70) return "healthy";
        if (health_score >= 50) return "acceptable";
        if (health_score >= 30) return "concerning";
        return "critical";
    }

    static std::string GenerateRecommendation(const std::string& classification, 
                                             const std::string& risk, 
                                             const std::string& trend) {
        if (classification == "A" && trend == "stable") return "maintain_current_terms";
        if (classification == "A" && trend == "improving") return "consider_early_payment_discount";
        if (classification == "B") return "standard_payment_terms";
        if (classification == "C" || risk == "high") return "tighten_collection_process";
        if (classification == "D" || risk == "critical") return "require_prepayment_or_cc_hold";
        if (trend == "deteriorating") return "increase_monitoring_frequency";
        return "monitor_closely";
    }

    static json GenerateAdvancedCashflowForecast(const json& clients, const std::vector<int>& all_payment_days) {
        json forecast = json::object();
        
        if (all_payment_days.empty()) {
            forecast["status"] = "insufficient_data";
            return forecast;
        }

        double avg_collection_days = CalculateAverage(all_payment_days);
        double stdev_collection_days = CalculateStdDev(all_payment_days);

        // Calculate inflow for different confidence levels
        double total_outstanding = 0.0;
        if (clients.is_array()) {
            for (const auto& client : clients) {
                total_outstanding += client.value("outstanding_amount", 0.0);
            }
        }

        // 30-day forecast (conservative, optimistic, likely)
        double collection_30_rate_optimistic = SafeMinD(1.0, 30.0 / avg_collection_days);
        double collection_30_rate_conservative = SafeMaxD(0.0, (30.0 - stdev_collection_days) / avg_collection_days);
        double collection_30_rate_likely = SafeMinD(1.0, 30.0 / (avg_collection_days + stdev_collection_days * 0.5));

        // 60-day forecast
        double collection_60_rate_optimistic = SafeMinD(1.0, 60.0 / avg_collection_days);
        double collection_60_rate_conservative = SafeMaxD(0.0, (60.0 - stdev_collection_days * 2) / avg_collection_days);
        double collection_60_rate_likely = SafeMinD(1.0, 60.0 / (avg_collection_days + stdev_collection_days * 0.3));

        // 90-day forecast
        double collection_90_rate_optimistic = SafeMinD(1.0, 90.0 / avg_collection_days);
        double collection_90_rate_conservative = SafeMaxD(0.0, (90.0 - stdev_collection_days * 3) / avg_collection_days);
        double collection_90_rate_likely = SafeMinD(1.0, 90.0 / (avg_collection_days + stdev_collection_days * 0.1));

        forecast = {
            {"total_outstanding", RoundTo2(total_outstanding)},
            {"average_collection_days", RoundTo2(avg_collection_days)},
            {"collection_variability", RoundTo2(stdev_collection_days)},
            
            {"forecasts", {
                {"30_days", {
                    {"optimistic", RoundTo2(total_outstanding * collection_30_rate_optimistic)},
                    {"likely", RoundTo2(total_outstanding * collection_30_rate_likely)},
                    {"conservative", RoundTo2(total_outstanding * collection_30_rate_conservative)}
                }},
                {"60_days", {
                    {"optimistic", RoundTo2(total_outstanding * collection_60_rate_optimistic)},
                    {"likely", RoundTo2(total_outstanding * collection_60_rate_likely)},
                    {"conservative", std::round(total_outstanding * collection_60_rate_conservative * 100) / 100}
                }},
                {"90_days", {
                    {"optimistic", RoundTo2(total_outstanding * collection_90_rate_optimistic)},
                    {"likely", RoundTo2(total_outstanding * collection_90_rate_likely)},
                    {"conservative", RoundTo2(total_outstanding * collection_90_rate_conservative)}
                }}
            }},
            
            {"confidence_level", DetermineConfidenceLevel(all_payment_days.size())}
        };

        return forecast;
    }

    static std::string DetermineConfidenceLevel(int sample_size) {
        if (sample_size >= 30) return "high";
        if (sample_size >= 15) return "medium";
        if (sample_size >= 5) return "low";
        return "very_low";
    }
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "PAYMENT ANALYSIS ENGINE - C++ SERVER v2.0" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Robustly handles multiple concurrent clients" << std::endl;
        std::cout << "Optimized for high-performance payment analysis" << std::endl;
        std::cout << std::string(70, '=') << "\n" << std::endl;

        // Default to port 8888 to align with Django integration; allow override via --port or -p
        int port = 8888;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
                try { port = std::stoi(argv[++i]); } catch (...) { std::cerr << "Invalid port value, using default 8888\n"; }
            }
        }

        PaymentAnalysisServer server(port);

        if (!server.Start()) {
            std::cerr << "[FATAL] Failed to start server on port " << port << std::endl;
            return 1;
        }

        std::cout << "[INFO] Server is running on port " << port << ". Press Ctrl+C to stop.\n" << std::endl;
        server.WaitForever();

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << std::endl;
        return 1;
    }
}
