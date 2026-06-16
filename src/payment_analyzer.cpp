#include "payment_analyzer.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <numeric>
#include <ctime>

PaymentAnalyzer::PaymentAnalyzer() {}

int PaymentAnalyzer::parse_date(const std::string& date_str) {
    // Parse YYYY-MM-DD format, return days since epoch
    try {
        if (date_str.length() < 10) return 0;
        
        int year = std::stoi(date_str.substr(0, 4));
        int month = std::stoi(date_str.substr(5, 2));
        int day = std::stoi(date_str.substr(8, 2));
        
        // Simplified day of year calculation
        int days_in_months[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        int day_of_year = days_in_months[month - 1] + day;
        
        // Check for leap year
        if (month > 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
            day_of_year++;
        }
        
        // Convert to days since epoch (1970-01-01)
        int days = (year - 1970) * 365 + ((year - 1969) / 4) - ((year - 1901) / 100) + ((year - 1601) / 400) + day_of_year;
        return days;
    } catch (...) {
        return 0;
    }
}

AnalysisResult PaymentAnalyzer::analyze_payment_patterns(
    const std::vector<Transaction>& transactions,
    int since_days
) {
    AnalysisResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        patterns.clear();
        payment_delays.clear();
        
        if (transactions.empty()) {
            result.success = true;
            result.data = {
                {"patterns", json::array()},
                {"total_parties", 0}
            };
            result.confidence_score = 0.0;
            result.sample_size = 0;
            return result;
        }
        
        // Group transactions by party and type
        std::map<std::string, std::vector<Transaction>> sales_by_party;
        std::map<std::string, std::vector<Transaction>> receipts_by_party;
        
        for (const auto& txn : transactions) {
            std::string reg_type = txn.register_type;
            std::transform(reg_type.begin(), reg_type.end(), reg_type.begin(), ::tolower);
            
            std::string party = txn.party_name;
            if (party.empty()) continue;
            
            if (reg_type == "sales" || reg_type == "sale") {
                sales_by_party[party].push_back(txn);
            } else if (reg_type == "receipt" || reg_type == "rcpt" || reg_type == "receipts") {
                receipts_by_party[party].push_back(txn);
            }
        }
        
        json patterns_array = json::array();
        double total_confidence = 0.0;
        int patterns_count = 0;
        
        // FIFO Matching Pass - Process each party
        for (auto& [party, sales] : sales_by_party) {
            auto& receipts = receipts_by_party[party];
            if (sales.empty() || receipts.empty()) continue;
            
            // Sort by date
            std::sort(sales.begin(), sales.end(), [this](const Transaction& a, const Transaction& b) {
                return parse_date(a.date) < parse_date(b.date);
            });
            std::sort(receipts.begin(), receipts.end(), [this](const Transaction& a, const Transaction& b) {
                return parse_date(a.date) < parse_date(b.date);
            });
            
            std::vector<PaymentDelayRecord> delays;
            size_t receipt_idx = 0;
            
            // FIFO matching algorithm (identical to Django)
            for (auto& sale : sales) {
                double remaining_sale = sale.amount;
                if (remaining_sale <= 0) continue;
                
                while (remaining_sale > 0.01 && receipt_idx < receipts.size()) {
                    auto& receipt = receipts[receipt_idx];
                    double remaining_receipt = receipt.remaining_amount > 0 ? receipt.remaining_amount : receipt.amount;
                    
                    // Skip receipts before sale date
                    if (parse_date(receipt.date) < parse_date(sale.date)) {
                        receipt_idx++;
                        continue;
                    }
                    
                    if (remaining_receipt <= 0.01) {
                        receipt_idx++;
                        continue;
                    }
                    
                    // Allocate amount
                    double allocate = std::min(remaining_sale, remaining_receipt);
                    if (allocate > 0.01) {
                        int delay = parse_date(receipt.date) - parse_date(sale.date);
                        delays.push_back({delay, allocate});
                        
                        remaining_sale -= allocate;
                        receipt.remaining_amount = remaining_receipt - allocate;
                    }
                    
                    if (receipt.remaining_amount <= 0.01) {
                        receipt_idx++;
                    }
                }
            }
            
            // Calculate weighted statistics (same as Django)
            if (!delays.empty()) {
                double total_matched = 0.0;
                double weighted_sum = 0.0;
                
                for (const auto& delay : delays) {
                    total_matched += delay.amount;
                    weighted_sum += delay.days * delay.amount;
                }
                
                if (total_matched > 0.01) {
                    double avg_delay = weighted_sum / total_matched;
                    
                    // Calculate variance and standard deviation
                    double variance = 0.0;
                    for (const auto& delay : delays) {
                        double diff = delay.days - avg_delay;
                        variance += (diff * diff) * delay.amount;
                    }
                    variance /= total_matched;
                    double std_dev = std::sqrt(variance);
                    
                    // Confidence based on sample size (same as Django: min(sample_size / 10.0, 1.0))
                    double confidence = std::min(1.0, delays.size() / 10.0);
                    
                    PaymentPattern pattern;
                    pattern.party_name = party;
                    pattern.avg_payment_days = static_cast<int>(std::round(avg_delay));
                    pattern.delay_std_deviation = std_dev;
                    pattern.confidence_score = confidence;
                    pattern.pattern_consistency = confidence;
                    pattern.sample_size = static_cast<int>(delays.size());
                    
                    patterns[party] = pattern;
                    payment_delays[party] = delays;
                    
                    patterns_array.push_back(pattern.to_json());
                    total_confidence += confidence;
                    patterns_count++;
                }
            }
        }
        
        double avg_confidence = patterns_count > 0 ? total_confidence / patterns_count : 0.0;
        
        result.success = true;
        result.data = {
            {"patterns", patterns_array},
            {"total_parties", patterns.size()},
            {"avg_confidence", avg_confidence}
        };
        result.confidence_score = avg_confidence;
        result.sample_size = static_cast<int>(transactions.size());
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Payment pattern analysis failed: ") + e.what();
        result.confidence_score = 0.0;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    return result;
}

std::vector<CashflowPrediction> PaymentAnalyzer::predict_future_payments(
    const std::string& party_name,
    int days_ahead
) {
    std::vector<CashflowPrediction> predictions;
    
    auto it = patterns.find(party_name);
    if (it == patterns.end()) {
        return predictions;
    }
    
    const auto& pattern = it->second;
    
    // Get current date
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    // Generate predictions for next N days
    for (int i = 1; i <= days_ahead; ++i) {
        tm.tm_mday += 1;
        std::mktime(&tm);
        
        CashflowPrediction pred;
        
        // Format date as YYYY-MM-DD
        char date_buffer[11];
        std::strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", &tm);
        pred.date = std::string(date_buffer);
        
        // Predict payment based on pattern
        pred.receipts = 1000.0;  // Placeholder: would use historical average in real scenario
        pred.confidence = pattern.confidence_score;
        pred.predicted_balance = 0.0;
        pred.min_balance = 0.0;
        pred.max_balance = 0.0;
        
        predictions.push_back(pred);
    }
    
    return predictions;
}

std::map<std::string, double> PaymentAnalyzer::calculate_party_balances(
    const std::vector<Transaction>& transactions
) {
    std::map<std::string, double> balances;
    
    for (const auto& txn : transactions) {
        if (balances.find(txn.party_name) == balances.end()) {
            balances[txn.party_name] = 0.0;
        }
        
        std::string reg_type = txn.register_type;
        std::transform(reg_type.begin(), reg_type.end(), reg_type.begin(), ::tolower);
        
        if (reg_type == "sales" || reg_type == "sale") {
            balances[txn.party_name] += txn.amount;
        } else if (reg_type == "receipt" || reg_type == "rcpt" || reg_type == "receipts") {
            balances[txn.party_name] -= txn.amount;
        }
    }
    
    return balances;
}

double PaymentAnalyzer::calculate_std_deviation(const std::vector<PaymentDelayRecord>& delays) {
    if (delays.empty()) return 0.0;
    
    double sum = 0.0;
    double total_weight = 0.0;
    
    for (const auto& d : delays) {
        sum += d.days * d.amount;
        total_weight += d.amount;
    }
    
    if (total_weight < 0.01) return 0.0;
    
    double mean = sum / total_weight;
    
    double variance = 0.0;
    for (const auto& d : delays) {
        double diff = d.days - mean;
        variance += (diff * diff) * d.amount;
    }
    variance /= total_weight;
    
    return std::sqrt(variance);
}

double PaymentAnalyzer::calculate_pattern_consistency(const std::vector<PaymentDelayRecord>& delays) {
    if (delays.size() < 2) return 1.0;
    
    double std_dev = calculate_std_deviation(delays);
    
    double sum = 0.0;
    double total_weight = 0.0;
    for (const auto& d : delays) {
        sum += d.days * d.amount;
        total_weight += d.amount;
    }
    
    if (total_weight < 0.01) return 0.0;
    
    double mean = sum / total_weight;
    if (mean <= 0) return 0.0;
    
    // Coefficient of variation
    double cv = std_dev / mean;
    
    // Convert to consistency score (0-1)
    // Lower variation = higher consistency
    double consistency = 1.0 / (1.0 + cv);
    
    return std::min(1.0, consistency);
}
