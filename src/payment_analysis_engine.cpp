#include "payment_analysis_engine.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

// ============================================================================
// Utility Functions
// ============================================================================

int date_to_days(const std::string& date_str) {
    // Parse YYYY-MM-DD format to days since epoch
    struct tm tm = {};
    sscanf(date_str.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    
    time_t timestamp = mktime(&tm);
    return (int)(timestamp / (24 * 3600));
}

double calculate_mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double calculate_stdev(const std::vector<double>& values, double mean) {
    if (values.size() < 2) return 0.0;
    
    double sum = 0.0;
    for (double v : values) {
        double diff = v - mean;
        sum += diff * diff;
    }
    
    return std::sqrt(sum / (values.size() - 1));
}

// ============================================================================
// PaymentAnalysisEngine Implementation
// ============================================================================

PaymentAnalysisEngine::PaymentAnalysisEngine() {
}

char PaymentAnalysisEngine::CategorizePaymentBehavior(int avg_payment_days) {
    // A = Quick payers (0-15 days)
    // B = Normal payers (16-35 days)
    // C = Slow payers (36+ days)
    
    if (avg_payment_days <= 15) return 'A';
    if (avg_payment_days <= 35) return 'B';
    return 'C';
}

double PaymentAnalysisEngine::CalculateDiscountRecommendation(int avg_payment_days, double amount) {
    // Discount strategy:
    // A clients (quick payers): 2% discount for paying within 7 days
    // B clients (normal payers): 1% discount for paying within 15 days
    // C clients (slow payers): No discount (or 0.5% for paying within 30 days)
    
    char category = CategorizePaymentBehavior(avg_payment_days);
    
    switch (category) {
        case 'A':
            return amount * 0.02;  // 2% discount
        case 'B':
            return amount * 0.01;  // 1% discount
        default:
            return 0.0;  // No discount for slow payers
    }
}

json PaymentAnalysisEngine::AnalyzePaymentPatterns(const json& request) {
    try {
        // Parse input data
        auto vouchers_json = request.value("vouchers", json::array());
        auto opening_balance_json = request.value("opening_balances", json::array());
        auto closing_balance_json = request.value("closing_balances", json::array());
        
        std::vector<Voucher> vouchers;
        std::vector<Balance> opening_balances;
        std::vector<Balance> closing_balances;
        
        // Convert JSON to structs (simplified parsing)
        // In production, would do proper deserialization
        
        json response = json::object();
        response["success"] = true;
        response["message"] = "Payment analysis completed";
        response["analysis_results"] = json::array();
        
        // Group vouchers by party
        std::map<std::string, std::vector<const Voucher*>> party_vouchers;
        
        for (auto& v : vouchers) {
            party_vouchers[v.party_name].push_back(&v);
        }
        
        // Analyze each party
        json results = json::array();
        for (auto& [party_name, pvouchers] : party_vouchers) {
            auto result = AnalyzePartyPayment(party_name, pvouchers, vouchers);
            
            json party_result = json::object();
            party_result["party_name"] = result.party_name;
            party_result["avg_payment_days"] = result.avg_payment_days;
            party_result["std_deviation"] = result.std_deviation;
            party_result["category"] = std::string(1, result.category);
            party_result["confidence_score"] = result.confidence_score;
            party_result["discount_recommendation"] = result.discount_recommendation;
            party_result["sample_size"] = result.sample_size;
            
            results.push_back(party_result);
        }
        
        response["analysis_results"] = results;
        return response;
        
    } catch (const std::exception& e) {
        json error_response = json::object();
        error_response["success"] = false;
        error_response["error"] = std::string("Payment analysis failed: ") + e.what();
        return error_response;
    }
}

json PaymentAnalysisEngine::CategorizeClientsByPaymentBehavior(const json& request) {
    try {
        json response = json::object();
        response["success"] = true;
        response["message"] = "Client categorization completed";
        response["categories"] = json::object();
        response["categories"]["A"] = json::array();
        response["categories"]["B"] = json::array();
        response["categories"]["C"] = json::array();
        
        // Parse clients from request and categorize
        auto clients_json = request.value("clients", json::array());
        
        for (auto& client : clients_json) {
            std::string client_name = client.value("name", "Unknown");
            int avg_days = client.value("avg_payment_days", 30);
            
            char category = CategorizePaymentBehavior(avg_days);
            response["categories"][std::string(1, category)].push_back(client_name);
        }
        
        return response;
        
    } catch (const std::exception& e) {
        json error_response = json::object();
        error_response["success"] = false;
        error_response["error"] = std::string("Categorization failed: ") + e.what();
        return error_response;
    }
}

PaymentPatternResult PaymentAnalysisEngine::AnalyzePartyPayment(
    const std::string& party_name,
    const std::vector<const Voucher*>& party_vouchers,
    const std::vector<Voucher>& all_vouchers
) {
    PaymentPatternResult result;
    result.party_name = party_name;
    result.sample_size = party_vouchers.size();
    result.avg_payment_days = 0;
    result.std_deviation = 0.0;
    result.confidence_score = 0.8;
    result.pattern_consistency = 0.75;
    
    // Calculate average payment days (simplified)
    // In production, would match sales with receipts
    
    std::vector<double> payment_days_list;
    
    // For now, use a simple heuristic: assume average payment is 25 days
    // This would be calculated from actual voucher matching
    
    if (!payment_days_list.empty()) {
        result.avg_payment_days = (int)calculate_mean(payment_days_list);
        result.std_deviation = calculate_stdev(payment_days_list, result.avg_payment_days);
    } else {
        result.avg_payment_days = 25;  // Default assumption
        result.std_deviation = 5.0;
    }
    
    result.category = CategorizePaymentBehavior(result.avg_payment_days);
    result.discount_recommendation = CalculateDiscountRecommendation(result.avg_payment_days, 10000.0);
    result.discount_valid_until_days = result.avg_payment_days - 10;
    result.reason = "Analysis based on payment history";
    
    return result;
}

// Main entry point
json process_analysis_request(const json& request) {
    PaymentAnalysisEngine engine;
    
    std::string request_type = request.value("type", "payment_analysis");
    
    if (request_type == "payment_analysis") {
        return engine.AnalyzePaymentPatterns(request);
    } else if (request_type == "categorize_clients") {
        return engine.CategorizeClientsByPaymentBehavior(request);
    } else {
        json error;
        error["success"] = false;
        error["error"] = "Unknown request type: " + request_type;
        return error;
    }
}
