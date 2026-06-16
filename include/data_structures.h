#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Data structures for transaction and analysis data
 */

struct Transaction {
    std::string voucher_number;
    std::string voucher_type;
    std::string date;
    double amount = 0.0;
    double remaining_amount = 0.0;
    std::string party_name;
    std::string register_type;
    bool is_debit = false;
    bool is_credit = false;
    
    json to_json() const {
        return json{
            {"voucher_number", voucher_number},
            {"voucher_type", voucher_type},
            {"date", date},
            {"amount", amount},
            {"remaining_amount", remaining_amount},
            {"party_name", party_name},
            {"register_type", register_type},
            {"is_debit", is_debit},
            {"is_credit", is_credit}
        };
    }
    
    static Transaction from_json(const json& j) {
        Transaction t;
        if (j.contains("voucher_number")) t.voucher_number = j["voucher_number"];
        if (j.contains("voucher_type")) t.voucher_type = j["voucher_type"];
        if (j.contains("date")) t.date = j["date"];
        if (j.contains("amount")) t.amount = j["amount"];
        if (j.contains("remaining_amount")) t.remaining_amount = j["remaining_amount"];
        if (j.contains("party_name")) t.party_name = j["party_name"];
        if (j.contains("register_type")) t.register_type = j["register_type"];
        if (j.contains("is_debit")) t.is_debit = j["is_debit"];
        if (j.contains("is_credit")) t.is_credit = j["is_credit"];
        return t;
    }
};

struct PaymentPattern {
    std::string party_name;
    int avg_payment_days = 0;
    double confidence_score = 0.5;
    double delay_std_deviation = 0.0;
    double pattern_consistency = 1.0;
    int sample_size = 0;
    std::string expected_payment_date;
    
    json to_json() const {
        return json{
            {"party_name", party_name},
            {"avg_payment_days", avg_payment_days},
            {"confidence_score", confidence_score},
            {"delay_std_deviation", delay_std_deviation},
            {"pattern_consistency", pattern_consistency},
            {"sample_size", sample_size},
            {"expected_payment_date", expected_payment_date}
        };
    }
};

struct FixedExpense {
    std::string description;
    double amount = 0.0;
    double amount_std_deviation = 0.0;
    std::string frequency;  // monthly, quarterly, yearly
    int interval_days = 30;
    double interval_std_deviation = 0.0;
    double pattern_consistency = 1.0;
    int due_day = 1;
    std::string next_date;
    std::string last_paid_date;
    int sample_size = 0;
    bool is_active = true;
    
    json to_json() const {
        return json{
            {"description", description},
            {"amount", amount},
            {"amount_std_deviation", amount_std_deviation},
            {"frequency", frequency},
            {"interval_days", interval_days},
            {"interval_std_deviation", interval_std_deviation},
            {"pattern_consistency", pattern_consistency},
            {"due_day", due_day},
            {"next_date", next_date},
            {"last_paid_date", last_paid_date},
            {"sample_size", sample_size},
            {"is_active", is_active}
        };
    }
};

struct CashflowPrediction {
    std::string date;
    double predicted_balance = 0.0;
    double min_balance = 0.0;
    double max_balance = 0.0;
    double receipts = 0.0;
    double expenses = 0.0;
    double confidence = 0.5;
    
    json to_json() const {
        return json{
            {"date", date},
            {"predicted_balance", predicted_balance},
            {"min_balance", min_balance},
            {"max_balance", max_balance},
            {"receipts", receipts},
            {"expenses", expenses},
            {"confidence", confidence}
        };
    }
};

struct ClientCategory {
    std::string party_name;
    std::string category;  // VIP, Regular, At-Risk, Inactive
    double average_transaction_value = 0.0;
    int transaction_count = 0;
    int days_since_last_transaction = 0;
    double payment_reliability_score = 0.5;  // 0-1
    double discount_eligible = false;
    double suggested_discount_percent = 0.0;
    
    json to_json() const {
        return json{
            {"party_name", party_name},
            {"category", category},
            {"average_transaction_value", average_transaction_value},
            {"transaction_count", transaction_count},
            {"days_since_last_transaction", days_since_last_transaction},
            {"payment_reliability_score", payment_reliability_score},
            {"discount_eligible", discount_eligible},
            {"suggested_discount_percent", suggested_discount_percent}
        };
    }
};

/**
 * Analysis result wrapper
 */
struct AnalysisResult {
    bool success = true;
    std::string error_message;
    json data;
    int64_t execution_time_ms = 0;
    std::vector<std::string> logs;
    double confidence_score = 0.5;
    int sample_size = 0;
    
    json to_json() const {
        return json{
            {"success", success},
            {"error_message", error_message},
            {"data", data},
            {"execution_time_ms", execution_time_ms},
            {"logs", logs},
            {"confidence_score", confidence_score},
            {"sample_size", sample_size}
        };
    }
};
