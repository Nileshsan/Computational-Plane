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
#include <nlohmann/json.hpp>

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
    #define SOCKET int
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define INVALID_SOCKET_CHECK(s) ((s) < 0)
    #define CLOSE_SOCKET(s) close(s)
#endif

using json = nlohmann::json;

// Helper function to safely calculate min
static inline int SafeMin(int a, int b) { return (a < b) ? a : b; }
static inline int SafeMax(int a, int b) { return (a > b) ? a : b; }

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
        for (const auto& kv : headers) {
            oss << kv.first << ": " << kv.second << "\r\n";
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
// Analysis Helper Functions
// ============================================================================

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
    return sqrt(variance / (values.size() - 1));
}

static int CalculatePercentile(std::vector<int> values, int percentile) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    int index = (percentile * (int)values.size()) / 100;
    return values[SafeMin(index, (int)values.size() - 1)];
}

static std::string AnalyzePaymentTrend(const std::vector<int>& payment_days) {
    if (payment_days.size() < 3) return "insufficient_data";
    
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
    score += (int)(on_time_percentage * 0.4);
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
    double health_score = (on_time_percentage * 0.7) + (SafeMax(0.0, 25.0 - stdev) * 1.0);
    
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

static std::string DetermineConfidenceLevel(int sample_size) {
    if (sample_size >= 30) return "high";
    if (sample_size >= 15) return "medium";
    if (sample_size >= 5) return "low";
    return "very_low";
}

static json GenerateAdvancedCashflowForecast(const json& clients, const std::vector<int>& all_payment_days) {
    json forecast = json::object();
    
    if (all_payment_days.empty()) {
        forecast["status"] = "insufficient_data";
        return forecast;
    }

    double avg_collection_days = CalculateAverage(all_payment_days);
    double stdev_collection_days = CalculateStdDev(all_payment_days);

    double total_outstanding = 0.0;
    if (clients.is_array()) {
        for (const auto& client : clients) {
            total_outstanding += client.value("outstanding_amount", 0.0);
        }
    }

    double collection_30_rate_optimistic = SafeMin(1.0, 30.0 / avg_collection_days);
    double collection_30_rate_conservative = SafeMax(0.0, (30.0 - stdev_collection_days) / avg_collection_days);
    double collection_30_rate_likely = SafeMin(1.0, 30.0 / (avg_collection_days + stdev_collection_days * 0.5));

    double collection_60_rate_optimistic = SafeMin(1.0, 60.0 / avg_collection_days);
    double collection_60_rate_conservative = SafeMax(0.0, (60.0 - stdev_collection_days * 2) / avg_collection_days);
    double collection_60_rate_likely = SafeMin(1.0, 60.0 / (avg_collection_days + stdev_collection_days * 0.3));

    double collection_90_rate_optimistic = SafeMin(1.0, 90.0 / avg_collection_days);
    double collection_90_rate_conservative = SafeMax(0.0, (90.0 - stdev_collection_days * 3) / avg_collection_days);
    double collection_90_rate_likely = SafeMin(1.0, 90.0 / (avg_collection_days + stdev_collection_days * 0.1));

    forecast = {
        {"total_outstanding", (int)(total_outstanding * 100) / 100.0},
        {"average_collection_days", (int)(avg_collection_days * 100) / 100.0},
        {"collection_variability", (int)(stdev_collection_days * 100) / 100.0},
        
        {"forecasts", {
            {"30_days", {
                {"optimistic", (int)(total_outstanding * collection_30_rate_optimistic * 100) / 100.0},
                {"likely", (int)(total_outstanding * collection_30_rate_likely * 100) / 100.0},
                {"conservative", (int)(total_outstanding * collection_30_rate_conservative * 100) / 100.0}
            }},
            {"60_days", {
                {"optimistic", (int)(total_outstanding * collection_60_rate_optimistic * 100) / 100.0},
                {"likely", (int)(total_outstanding * collection_60_rate_likely * 100) / 100.0},
                {"conservative", (int)(total_outstanding * collection_60_rate_conservative * 100) / 100.0}
            }},
            {"90_days", {
                {"optimistic", (int)(total_outstanding * collection_90_rate_optimistic * 100) / 100.0},
                {"likely", (int)(total_outstanding * collection_90_rate_likely * 100) / 100.0},
                {"conservative", (int)(total_outstanding * collection_90_rate_conservative * 100) / 100.0}
            }}
        }},
        
        {"confidence_level", DetermineConfidenceLevel((int)all_payment_days.size())}
    };

    return forecast;
}

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
    PaymentAnalysisServer(int port = 8888) : listen_socket(INVALID_SOCKET), port(port) {}

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

        int reuse = 1;
        if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, 
                      (const char*)&reuse, sizeof(reuse)) < 0) {
            std::cerr << "[WARNING] setsockopt failed" << std::endl;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
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
        std::cout << "[SERVER] Listening on http://127.0.0.1:" << port << std::endl;
        std::cout << "[SERVER] Waiting for requests from Django..." << std::endl;

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

            {
                std::lock_guard<std::mutex> lock(thread_mutex);
                client_threads.emplace_back(&PaymentAnalysisServer::HandleClient, 
                                          this, client_socket, client_ip);
            }
        }
    }

    void HandleClient(SOCKET client_socket, const std::string& client_ip) {
        try {
            const int BUFFER_SIZE = 65536;
            char buffer[BUFFER_SIZE] = {0};

            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) {
                std::cerr << "[CLIENT] Receive failed" << std::endl;
                CLOSE_SOCKET(client_socket);
                return;
            }

            buffer[bytes_received] = '\0';
            std::string raw_request(buffer);

            std::cout << "[CLIENT] Received " << bytes_received << " bytes" << std::endl;

            HTTPRequest http_req = HTTPParser::Parse(raw_request);

            std::cout << "[CLIENT] Method: " << http_req.method << std::endl;
            std::cout << "[CLIENT] Path: " << http_req.path << std::endl;

            HTTPResponse response = RouteRequest(http_req);

            std::string response_str = response.BuildResponse();
            int bytes_sent = send(client_socket, response_str.c_str(), (int)response_str.length(), 0);

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

        // Health check endpoint - supports both GET and POST
        if ((req.path == "/api/health" || req.path == "/health") && 
            (req.method == "GET" || req.method == "POST")) {
            HTTPResponse response(200);
            json health = {
                {"status", "healthy"},
                {"version", "2.1"},
                {"server", "PaymentAnalysisEngine"},
                {"timestamp", (long long)std::chrono::system_clock::now().time_since_epoch().count()}
            };
            response.body = health.dump();
            return response;
        }

        // Analysis endpoints - POST only
        if (req.method == "POST") {
            // Route 1: Cashflow prediction (primary endpoint)
            if (req.path == "/api/cashflow-prediction") {
                return HandleAnalysisRequest(req);
            }
            
            // Route 2: Client categorization (primary endpoint)
            if (req.path == "/api/categorize-clients") {
                return HandleAnalysisRequest(req);
            }
            
            // Route 3: Legacy endpoint (backward compatibility)
            if (req.path == "/api/analyze-payment-behavior") {
                return HandleAnalysisRequest(req);
            }
        }

        // Method not allowed for analysis endpoints
        if (req.method != "GET" && req.method != "POST") {
            HTTPResponse response(405);
            response.body = R"({"error": "Method not allowed - use GET for /health or POST for /api/cashflow-prediction, /api/categorize-clients"})";
            return response;
        }

        // Not found
        HTTPResponse response(404);
        response.body = R"({"error": "Endpoint not found"})";
        return response;
    }

    HTTPResponse HandleAnalysisRequest(const HTTPRequest& req) {
        try {
            request_count++;
            int req_id = request_count;

            std::cout << "[ANALYSIS_" << req_id << "] Parsing JSON payload..." << std::endl;

            json payload = json::parse(req.body);

            int company_id = payload.value("company_id", 0);
            std::string company_name = payload.value("company_name", "Unknown");

            std::cout << "[ANALYSIS_" << req_id << "] Company: " << company_name 
                     << " (ID: " << company_id << ")" << std::endl;

            json clients = payload.value("clients", json::array());
            std::cout << "[ANALYSIS_" << req_id << "] Analyzing " << clients.size() 
                     << " clients..." << std::endl;

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

                    double avg_days = CalculateAverage(payment_days);
                    double median_days = CalculateMedian(payment_days);
                    double stdev_days = CalculateStdDev(payment_days);
                    double on_time_percentage = (transaction_count > 0) ? 
                        (on_time_count * 100.0 / transaction_count) : 0.0;
                    
                    int percentile_90 = CalculatePercentile(payment_days, 90);
                    int min_days = 0, max_days = 0;
                    if (!payment_days.empty()) {
                        min_days = *std::min_element(payment_days.begin(), payment_days.end());
                        max_days = *std::max_element(payment_days.begin(), payment_days.end());
                    }

                    std::string classification = ClassifyClient(avg_days, stdev_days);
                    std::string risk = CalculateRiskLevel(avg_days, on_time_percentage);
                    int confidence_score = CalculateConfidenceScore((int)payment_days.size(), stdev_days);

                    std::string trend = AnalyzePaymentTrend(payment_days);
                    std::string predictability = (stdev_days < 5) ? "high" : (stdev_days < 15) ? "medium" : "low";

                    client_analysis[client_name] = {
                        {"classification", classification},
                        {"risk_level", risk},
                        {"confidence_score", confidence_score},
                        
                        {"payment_metrics", {
                            {"avg_payment_days", (int)(avg_days * 100) / 100.0},
                            {"median_payment_days", (int)(median_days * 100) / 100.0},
                            {"stdev_payment_days", (int)(stdev_days * 100) / 100.0},
                            {"min_payment_days", min_days},
                            {"max_payment_days", max_days},
                            {"percentile_90", percentile_90},
                            {"predictability", predictability}
                        }},
                        
                        {"performance", {
                            {"on_time_count", on_time_count},
                            {"total_transactions", transaction_count},
                            {"on_time_percentage", (int)(on_time_percentage * 100) / 100.0},
                            {"late_transactions", transaction_count - on_time_count},
                            {"reliability_score", ComputeReliabilityScore(on_time_percentage, stdev_days)}
                        }},
                        
                        {"financial", {
                            {"outstanding_amount", (int)(outstanding * 100) / 100.0},
                            {"days_to_collect", (int)ceil(avg_days)},
                            {"collection_risk", ComputeCollectionRisk(avg_days, on_time_percentage)}
                        }},
                        
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

            if (client_count > 0) {
                total_on_time_percentage /= client_count;
                double company_stdev = CalculateStdDev(all_payment_days);
                double company_avg = CalculateAverage(all_payment_days);

                company_metrics = {
                    {"total_clients", client_count},
                    {"total_outstanding", (int)(total_outstanding * 100) / 100.0},
                    {"avg_on_time_percentage", (int)(total_on_time_percentage * 100) / 100.0},
                    
                    {"payment_profile", {
                        {"avg_days", (int)(company_avg * 100) / 100.0},
                        {"stdev_days", (int)(company_stdev * 100) / 100.0},
                        {"overall_reliability", ComputeReliabilityScore(total_on_time_percentage, company_stdev)}
                    }},
                    
                    {"classification_distribution", classification_count},
                    
                    {"payment_quality", DeterminePaymentQuality(total_on_time_percentage)},
                    
                    {"summary", {
                        {"total_transactions", total_transactions},
                        {"on_time_transactions", (int)(total_transactions * total_on_time_percentage / 100)},
                        {"payment_health", DeterminePaymentHealth(total_on_time_percentage, company_stdev)}
                    }}
                };
            }

            json cashflow = GenerateAdvancedCashflowForecast(clients, all_payment_days);

            json response_json = {
                {"request_id", "REQ-" + std::to_string(req_id)},
                {"company_id", company_id},
                {"company_name", company_name},
                {"status", "success"},
                
                {"company_metrics", company_metrics},
                {"client_analysis", client_analysis},
                {"classifications", classifications},
                {"cashflow_forecast", cashflow},
                
                {"analysis_summary", {
                    {"total_clients_analyzed", client_count},
                    {"high_risk_clients", classification_count["D"]},
                    {"medium_risk_clients", classification_count["C"]},
                    {"low_risk_clients", classification_count["A"] + classification_count["B"]},
                    {"overall_payment_health", DeterminePaymentHealth(total_on_time_percentage, CalculateStdDev(all_payment_days))}
                }},
                
                {"metadata", {
                    {"timestamp", (long long)std::chrono::system_clock::now().time_since_epoch().count()},
                    {"processed_by", "cpp_server"},
                    {"version", "2.1"},
                    {"analysis_quality", "comprehensive"}
                }}
            };

            std::cout << "[ANALYSIS_" << req_id << "] Analysis complete - comprehensive output generated" << std::endl;

            HTTPResponse response(200);
            response.body = response_json.dump(2);
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
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "PAYMENT ANALYSIS ENGINE - C++ SERVER v2.1" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Robustly handles multiple concurrent clients" << std::endl;
        std::cout << "Enhanced analysis with comprehensive output" << std::endl;
        std::cout << std::string(70, '=') << "\n" << std::endl;

        // Default to port 8888 for Django integration; allow override with --port or -p
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
