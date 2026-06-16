#include "client_categorizer.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>

ClientCategorizer::ClientCategorizer() {}

AnalysisResult ClientCategorizer::categorize_clients(
    const std::vector<Transaction>& transactions
) {
    AnalysisResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        categories.clear();
        
        // Calculate metrics for each client
        auto metrics = calculate_metrics(transactions);
        
        // Get current date for calculating inactivity
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        
        json categories_array = json::array();
        
        for (auto& [party, metric] : metrics) {
            ClientCategory cat;
            cat.party_name = party;
            cat.average_transaction_value = metric.average_transaction;
            cat.transaction_count = metric.transaction_count;
            
            // Calculate days since last transaction
            int days_since = 0;
            if (!metric.last_transaction_date.empty()) {
                std::tm tm = {};
                std::istringstream ss(metric.last_transaction_date);
                ss >> std::get_time(&tm, "%Y-%m-%d");
                
                auto last_time = std::mktime(&tm);
                days_since = (time_t_now - last_time) / 86400;
            }
            cat.days_since_last_transaction = days_since;
            
            // Calculate payment reliability
            cat.payment_reliability_score = calculate_reliability_score(
                metric.on_time_payments,
                metric.late_payments,
                0.7  // Placeholder pattern consistency
            );
            
            // Assign category
            cat.category = assign_category(
                metric,
                days_since,
                cat.payment_reliability_score
            );
            
            // Calculate discount
            calculate_discount(cat, metric);
            
            categories.push_back(cat);
            categories_array.push_back(cat.to_json());
        }
        
        result.success = true;
        result.data = {
            {"categories", categories_array},
            {"total_clients", categories.size()},
            {"vip_count", 0},  // Would count in real scenario
            {"at_risk_count", 0}
        };
        result.confidence_score = 0.75;
        result.sample_size = transactions.size();
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Client categorization failed: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    return result;
}

std::vector<ClientCategory> ClientCategorizer::get_client_categories() const {
    return categories;
}

ClientCategory ClientCategorizer::get_discount_recommendation(
    const std::string& party_name
) const {
    for (const auto& cat : categories) {
        if (cat.party_name == party_name) {
            return cat;
        }
    }
    return ClientCategory();
}

std::map<std::string, ClientCategorizer::ClientMetrics> ClientCategorizer::calculate_metrics(
    const std::vector<Transaction>& transactions
) {
    std::map<std::string, ClientMetrics> metrics;
    
    for (const auto& txn : transactions) {
        if (metrics.find(txn.party_name) == metrics.end()) {
            metrics[txn.party_name] = ClientMetrics{txn.party_name};
        }
        
        auto& metric = metrics[txn.party_name];
        metric.last_transaction_date = txn.date;
        metric.transaction_count++;
        
        if (txn.register_type == "sales") {
            metric.total_transactions += txn.amount;
        }
        
        // Simplified: assume all payments are on-time unless flagged
        if (txn.register_type == "receipt") {
            metric.on_time_payments++;
        }
    }
    
    // Calculate average transaction
    for (auto& [party, metric] : metrics) {
        if (metric.transaction_count > 0) {
            metric.average_transaction = metric.total_transactions / metric.transaction_count;
        }
    }
    
    return metrics;
}

std::string ClientCategorizer::assign_category(
    const ClientMetrics& metrics,
    int days_since_last,
    double reliability
) {
    // No transactions in 180+ days = Inactive
    if (days_since_last > 180) {
        return "Inactive";
    }
    
    // Recent, highly reliable, high-value = VIP
    if (days_since_last < 30 && reliability > 0.9 && metrics.average_transaction > 5000) {
        return "VIP";
    }
    
    // Low activity or low reliability = At-Risk
    if (days_since_last > 90 || reliability < 0.6) {
        return "At-Risk";
    }
    
    // Default = Regular
    return "Regular";
}

void ClientCategorizer::calculate_discount(
    ClientCategory& category,
    const ClientMetrics& metrics
) {
    category.discount_eligible = false;
    category.suggested_discount_percent = 0.0;
    
    // VIP gets 5-10% discount
    if (category.category == "VIP") {
        category.discount_eligible = true;
        category.suggested_discount_percent = 5.0 + (category.payment_reliability_score * 5.0);
    }
    
    // Regular with high reliability gets 2-3% discount
    else if (category.category == "Regular" && category.payment_reliability_score > 0.85) {
        category.discount_eligible = true;
        category.suggested_discount_percent = 2.0 + (category.payment_reliability_score - 0.85) * 10.0;
    }
    
    // At-Risk gets 1% early payment discount if they pay early
    else if (category.category == "At-Risk" && category.payment_reliability_score > 0.5) {
        category.discount_eligible = true;
        category.suggested_discount_percent = 1.0;
    }
    
    // Inactive = no discount
}

double ClientCategorizer::calculate_reliability_score(
    int on_time,
    int late,
    double pattern_consistency
) {
    int total = on_time + late;
    if (total == 0) return 0.5;
    
    double on_time_ratio = static_cast<double>(on_time) / total;
    
    // Combine on-time ratio with pattern consistency
    return (on_time_ratio * 0.7) + (pattern_consistency * 0.3);
}
