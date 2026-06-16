#pragma once

#include <vector>
#include <string>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Data structures for payment analysis

struct Voucher {
    int id;
    std::string voucher_type;
    std::string voucher_number;
    std::string date;
    double amount;
    std::string party_name;
    std::string register_type;
    double remaining_amount;
};

struct Balance {
    std::string ledger_name;
    std::string group;
    double opening_balance;
    double closing_balance;
    std::string balance_date;
};

struct PaymentMatch {
    const Voucher* sales_voucher;
    const Voucher* receipt_voucher;
    int days_to_pay;
    double amount;
};

struct PaymentPatternResult {
    std::string party_name;
    int avg_payment_days;
    double std_deviation;
    char category;
    double confidence_score;
    double pattern_consistency;
    int sample_size;
    double discount_recommendation;
    int discount_valid_until_days;
    std::string reason;
};

// Utility functions
int date_to_days(const std::string& date_str);
double calculate_mean(const std::vector<double>& values);
double calculate_stdev(const std::vector<double>& values, double mean);
const Voucher* find_matching_receipt(
    const Voucher& sales,
    const std::vector<const Voucher*>& receipts,
    const Voucher*& matched_receipt_out
);

// Main Payment Analysis Engine class
class PaymentAnalysisEngine {
public:
    PaymentAnalysisEngine();
    
    // Analyze payment patterns for all parties
    json AnalyzePaymentPatterns(const json& request);
    
    // Categorize clients by payment behavior (A, B, C)
    json CategorizeClientsByPaymentBehavior(const json& request);
    
    // Calculate discount recommendation based on payment days
    double CalculateDiscountRecommendation(int avg_payment_days, double amount);
    
private:
    // Helper methods
    PaymentPatternResult AnalyzePartyPayment(
        const std::string& party_name,
        const std::vector<const Voucher*>& party_vouchers,
        const std::vector<Voucher>& all_vouchers
    );
    
    // Categorize based on payment days
    char CategorizePaymentBehavior(int avg_payment_days);
};

// Main entry point for processing requests
json process_analysis_request(const json& request);
