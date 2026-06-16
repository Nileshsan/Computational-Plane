#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cmath>
#include <numeric>
#define NOMINMAX
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

// Helper to avoid macro collisions
inline double RoundTo2(double val) { return floor(val * 100 + 0.5) / 100; }

// ============================================================================
// Configuration & Metrics
// ============================================================================

struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 9001;
    int max_worker_threads = 8;
    int queue_size = 1000;
    int request_timeout_seconds = 300;
    bool enable_logging = true;
};

struct PerformanceMetrics {
    std::atomic<int> total_requests{0};
    std::atomic<int> successful_requests{0};
    std::atomic<int> failed_requests{0};
    std::atomic<int> queued_requests{0};
    std::atomic<long> total_processing_time{0};  // in milliseconds
    std::chrono::high_resolution_clock::time_point start_time;
};

// Global objects
ServerConfig g_config;
PerformanceMetrics g_metrics;

// ============================================================================
// Request Queue & Worker Pool
// ============================================================================

struct AnalysisRequest {
    std::string request_id;
    json payload;
    std::chrono::high_resolution_clock::time_point timestamp;
    int company_id;
    std::string client_name;
};

class RequestQueue {
private:
    std::queue<AnalysisRequest> queue;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> shutdown{false};

public:
    void enqueue(const AnalysisRequest& req) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.size() < g_config.queue_size) {
            queue.push(req);
            g_metrics.queued_requests++;
            cv.notify_one();
        }
    }

    bool dequeue(AnalysisRequest& req) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(1), [this] { return !queue.empty() || shutdown; });
        
        if (queue.empty()) return false;
        
        req = queue.front();
        queue.pop();
        g_metrics.queued_requests--;
        return true;
    }

    void stop() { shutdown = true; cv.notify_all(); }
    int size() const { std::lock_guard<std::mutex> lock(mutex); return queue.size(); }
};

// ============================================================================
// Payment Analysis Engine
// ============================================================================

class PaymentAnalysisEngine {
public:
    /**
     * Analyze payment behavior for a client
     * @param clients: Array of client payment records
     * @return JSON with analysis results
     */
    static json AnalyzePaymentBehavior(const json& clients) {
        json results = json::object();
        
        if (!clients.is_array() || clients.empty()) {
            return results;
        }

        try {
            for (const auto& client : clients) {
                std::string client_name = client.value("client_name", "Unknown");
                
                std::vector<int> payment_days;
                int on_time_count = 0;
                double total_outstanding = 0.0;
                int transaction_count = 0;

                // Extract payment metrics
                if (client.contains("payment_days") && client["payment_days"].is_array()) {
                    for (const auto& days : client["payment_days"]) {
                        payment_days.push_back(days.get<int>());
                    }
                }

                if (client.contains("on_time_count")) {
                    on_time_count = client["on_time_count"].get<int>();
                }

                if (client.contains("outstanding_amount")) {
                    total_outstanding = client["outstanding_amount"].get<double>();
                }

                if (client.contains("transaction_count")) {
                    transaction_count = client["transaction_count"].get<int>();
                }

                // Calculate statistics
                double avg_payment_days = CalculateAverage(payment_days);
                double median_payment_days = CalculateMedian(payment_days);
                double stdev_payment_days = CalculateStdDev(payment_days);
                double on_time_percentage = transaction_count > 0 
                    ? (on_time_count * 100.0 / transaction_count) 
                    : 0.0;

                // Classify client
                std::string classification = ClassifyClient(avg_payment_days);
                std::string risk_level = CalculateRiskLevel(avg_payment_days, stdev_payment_days);
                int confidence_score = CalculateConfidenceScore(payment_days.size(), stdev_payment_days);

                // Store client analysis
                json client_analysis = {
                    {"client_name", client_name},
                    {"classification", classification},
                    {"avg_payment_days", std::round(avg_payment_days * 100) / 100},
                    {"median_payment_days", std::round(median_payment_days * 100) / 100},
                    {"stdev_payment_days", std::round(stdev_payment_days * 100) / 100},
                    {"on_time_percentage", std::round(on_time_percentage * 100) / 100},
                    {"risk_level", risk_level},
                    {"confidence_score", confidence_score},
                    {"outstanding_amount", std::round(total_outstanding * 100) / 100},
                    {"transaction_count", transaction_count}
                };

                results[client_name] = client_analysis;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[ANALYSIS_ERROR] " << e.what() << std::endl;
        }

        return results;
    }

    /**
     * Generate cashflow forecast
     * @param clients: Client data with expected payment dates
     * @return JSON with 30/60/90 day forecasts
     */
    static json GenerateCashflowForecast(const json& clients) {
        json forecast = {
            {"cash_inflow_30_days", 0.0},
            {"cash_inflow_60_days", 0.0},
            {"cash_inflow_90_days", 0.0},
            {"client_breakdown", json::object()}
        };

        if (!clients.is_array()) return forecast;

        try {
            double inflow_30 = 0.0, inflow_60 = 0.0, inflow_90 = 0.0;

            for (const auto& client : clients) {
                if (!client.contains("outstanding_amount") || 
                    !client.contains("avg_payment_days")) {
                    continue;
                }

                double outstanding = client["outstanding_amount"].get<double>();
                int avg_days = client["avg_payment_days"].get<int>();

                if (avg_days <= 30) inflow_30 += outstanding;
                if (avg_days <= 60) inflow_60 += outstanding;
                if (avg_days <= 90) inflow_90 += outstanding;
            }

            forecast["cash_inflow_30_days"] = std::round(inflow_30 * 100) / 100;
            forecast["cash_inflow_60_days"] = std::round(inflow_60 * 100) / 100;
            forecast["cash_inflow_90_days"] = std::round(inflow_90 * 100) / 100;
        }
        catch (const std::exception& e) {
            std::cerr << "[FORECAST_ERROR] " << e.what() << std::endl;
        }

        return forecast;
    }

private:
    static double CalculateAverage(const std::vector<int>& values) {
        if (values.empty()) return 0.0;
        return static_cast<double>(std::accumulate(values.begin(), values.end(), 0)) / values.size();
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

    static std::string ClassifyClient(double avg_days) {
        if (avg_days <= 10) return "A";
        if (avg_days <= 25) return "B";
        if (avg_days <= 35) return "C";
        return "D";
    }

    static std::string CalculateRiskLevel(double avg_days, double stdev) {
        if (avg_days <= 10) return "low";
        if (avg_days <= 25) return "medium";
        if (avg_days <= 35) return "high";
        return "critical";
    }

    static int CalculateConfidenceScore(int sample_size, double stdev) {
        int score = 50;
        if (sample_size > 10) score += 20;
        if (sample_size > 30) score += 15;
        if (stdev < 5) score += 15;
        return (score > 100) ? 100 : score;
    }
};

// ============================================================================
// Worker Thread Pool
// ============================================================================

class WorkerPool {
private:
    std::vector<std::thread> workers;
    RequestQueue& request_queue;

public:
    WorkerPool(RequestQueue& queue, int num_workers) : request_queue(queue) {
        for (int i = 0; i < num_workers; ++i) {
            workers.emplace_back(&WorkerPool::WorkerThread, this, i);
        }
    }

    ~WorkerPool() {
        request_queue.stop();
        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }

private:
    void WorkerThread(int worker_id) {
        std::cout << "[WORKER_" << worker_id << "] Started" << std::endl;

        AnalysisRequest req;
        while (request_queue.dequeue(req)) {
            auto start_time = std::chrono::high_resolution_clock::now();

            try {
                std::cout << "[WORKER_" << worker_id << "] Processing request: " << req.request_id << std::endl;

                // Extract clients from payload
                json clients = req.payload.value("clients", json::array());
                
                // Run analysis
                json client_analysis = PaymentAnalysisEngine::AnalyzePaymentBehavior(clients);
                json cashflow_forecast = PaymentAnalysisEngine::GenerateCashflowForecast(clients);

                // Build response
                json response = {
                    {"request_id", req.request_id},
                    {"company_id", req.company_id},
                    {"status", "success"},
                    {"client_analysis", client_analysis},
                    {"cashflow_forecast", cashflow_forecast},
                    {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
                    {"processed_by", "cpp_engine"}
                };

                // Log results
                std::cout << "[WORKER_" << worker_id << "] COMPLETED: " << req.request_id << std::endl;
                std::cout << "[WORKER_" << worker_id << "] Response: " << response.dump(2) << std::endl;

                g_metrics.successful_requests++;
            }
            catch (const std::exception& e) {
                std::cerr << "[WORKER_" << worker_id << "] ERROR: " << e.what() << std::endl;
                g_metrics.failed_requests++;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            g_metrics.total_processing_time += duration;
            g_metrics.total_requests++;
        }

        std::cout << "[WORKER_" << worker_id << "] Stopped" << std::endl;
    }
};

// ============================================================================
// HTTP Server
// ============================================================================

class HTTPServer {
private:
    RequestQueue request_queue;
    std::unique_ptr<WorkerPool> worker_pool;
    std::atomic<bool> running{false};

public:
    HTTPServer() : request_queue() {
        g_metrics.start_time = std::chrono::high_resolution_clock::now();
    }

    bool Start() {
        std::cout << "[SERVER] Starting Payment Analysis Engine v2.0" << std::endl;
        std::cout << "[SERVER] Port: " << g_config.port << std::endl;
        std::cout << "[SERVER] Workers: " << g_config.max_worker_threads << std::endl;

        // Initialize worker pool
        worker_pool = std::make_unique<WorkerPool>(request_queue, g_config.max_worker_threads);
        running = true;

        std::cout << "[SERVER] Ready to receive requests" << std::endl;
        return true;
    }

    void Stop() {
        running = false;
        if (worker_pool) {
            worker_pool.reset();
        }
    }

    void ProcessRequest(const std::string& request_id, const json& payload, int company_id) {
        AnalysisRequest req;
        req.request_id = request_id;
        req.payload = payload;
        req.company_id = company_id;
        req.timestamp = std::chrono::high_resolution_clock::now();
        req.client_name = payload.value("company_name", "Unknown");

        std::cout << "[SERVER] Received request: " << request_id << " for company " << company_id << std::endl;
        request_queue.enqueue(req);
    }

    void PrintStats() {
        auto now = std::chrono::high_resolution_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - g_metrics.start_time).count();

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "PERFORMANCE METRICS" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "Uptime: " << uptime << "s" << std::endl;
        std::cout << "Total Requests: " << g_metrics.total_requests << std::endl;
        std::cout << "Successful: " << g_metrics.successful_requests << std::endl;
        std::cout << "Failed: " << g_metrics.failed_requests << std::endl;
        std::cout << "Queued: " << g_metrics.queued_requests << std::endl;
        
        if (g_metrics.total_requests > 0) {
            double avg_time = g_metrics.total_processing_time / g_metrics.total_requests;
            std::cout << "Avg Processing Time: " << avg_time << "ms" << std::endl;
        }
        std::cout << std::string(60, '=') << "\n" << std::endl;
    }
};

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        HTTPServer server;
        server.Start();

        // Simulate receiving requests from Django
        std::cout << "\n[MAIN] Waiting for requests... Press Ctrl+C to stop\n" << std::endl;

        // Test request 1
        {
            json test_payload = {
                {"company_id", 1},
                {"company_name", "Test Company 1"},
                {"clients", json::array({
                    {
                        {"client_name", "Client A"},
                        {"payment_days", json::array({8, 9, 7, 10, 8})},
                        {"on_time_count", 5},
                        {"outstanding_amount", 500000},
                        {"transaction_count", 5}
                    },
                    {
                        {"client_name", "Client B"},
                        {"payment_days", json::array({20, 22, 19, 23, 21})},
                        {"on_time_count", 4},
                        {"outstanding_amount", 300000},
                        {"transaction_count", 5}
                    },
                    {
                        {"client_name", "Client C"},
                        {"payment_days", json::array({30, 32, 28, 35, 31})},
                        {"on_time_count", 2},
                        {"outstanding_amount", 200000},
                        {"transaction_count", 5}
                    }
                })}
            };

            server.ProcessRequest("REQ-001", test_payload, 1);
        }

        // Test request 2 (simulating multiple concurrent requests)
        {
            json test_payload = {
                {"company_id", 2},
                {"company_name", "Test Company 2"},
                {"clients", json::array({
                    {
                        {"client_name", "Vendor X"},
                        {"payment_days", json::array({5, 6, 5, 7, 6})},
                        {"on_time_count", 5},
                        {"outstanding_amount", 1000000},
                        {"transaction_count", 5}
                    },
                    {
                        {"client_name", "Vendor Y"},
                        {"payment_days", json::array({45, 48, 42, 50, 46})},
                        {"on_time_count", 0},
                        {"outstanding_amount", 500000},
                        {"transaction_count", 5}
                    }
                })}
            };

            server.ProcessRequest("REQ-002", test_payload, 2);
        }

        // Keep server running
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        server.PrintStats();
        server.Stop();

        std::cout << "[MAIN] Server stopped gracefully" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
}
