#include "payment_insight_engine.h"
#include <iostream>
#include <fstream>
#include <sstream>

// Simple HTTP server wrapper for the PaymentInsightEngine
// This allows Django to call via HTTP POST

// Mock HTTP request handler (simplified - use microhttpd or similar in production)
json HandleAnalysisRequest(const std::string& request_body) {
    try {
        json request = json::parse(request_body);
        PaymentInsightEngine engine;
        
        std::string analysis_type = request.value("type", "analyze_all_clients");
        
        if (analysis_type == "analyze_all_clients") {
            return engine.AnalyzeAllClients(request);
        } else if (analysis_type == "analyze_single_client") {
            return engine.AnalyzeSingleClient(request);
        } else {
            json error;
            error["success"] = false;
            error["error"] = "Unknown analysis type: " + analysis_type;
            return error;
        }
    } catch (const std::exception& e) {
        json error;
        error["success"] = false;
        error["error"] = std::string("Request processing failed: ") + e.what();
        return error;
    }
}

// Example: Process test data
int main(int argc, char* argv[]) {
    
    // Test case: Using your debtor example
    json test_request = json::object();
    test_request["type"] = "analyze_all_clients";
    
    json clients_array = json::array();
    
    // Debtor A: Type A (fast payer)
    json debtor_a = json::object();
    debtor_a["name"] = "Debtor A";
    debtor_a["payment_days_history"] = json::array({10, 12, 15, 11, 14});
    debtor_a["amounts"] = json::array({50000.0});
    clients_array.push_back(debtor_a);
    
    // Debtor B: Type B (normal payer)
    json debtor_b = json::object();
    debtor_b["name"] = "Debtor B";
    debtor_b["payment_days_history"] = json::array({28, 30, 31, 27, 32});
    debtor_b["amounts"] = json::array({75000.0});
    clients_array.push_back(debtor_b);
    
    // Debtor C: Type C (slow payer)
    json debtor_c = json::object();
    debtor_c["name"] = "Debtor C";
    debtor_c["payment_days_history"] = json::array({45, 55, 50, 60, 48});
    debtor_c["amounts"] = json::array({100000.0});
    clients_array.push_back(debtor_c);
    
    test_request["clients"] = clients_array;
    
    // Run analysis
    std::cout << "==============================================================================" << std::endl;
    std::cout << "Advanced ML Payment Insight Engine - Test" << std::endl;
    std::cout << "==============================================================================" << std::endl;
    
    auto result = HandleAnalysisRequest(test_request.dump());
    
    std::cout << "\n[RESPONSE]" << std::endl;
    std::cout << result.dump(2) << std::endl;
    
    std::cout << "\n==============================================================================" << std::endl;
    std::cout << "Analysis Complete!" << std::endl;
    std::cout << "==============================================================================" << std::endl;
    
    return 0;
}
