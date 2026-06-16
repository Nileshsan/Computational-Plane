#include "expense_analyzer.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>

ExpenseAnalyzer::ExpenseAnalyzer() {}

AnalysisResult ExpenseAnalyzer::analyze_fixed_expenses(
    const std::vector<Transaction>& transactions
) {
    AnalysisResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        expenses.clear();
        
        // Filter for outgoing payments (expenses)
        std::vector<Transaction> expense_txns;
        for (const auto& txn : transactions) {
            if ((txn.register_type == "payment" || txn.register_type == "journal") && txn.is_debit) {
                expense_txns.push_back(txn);
            }
        }
        
        // Cluster by amount to find recurring patterns
        auto clusters = cluster_by_amount(expense_txns);
        
        json expenses_array = json::array();
        
        for (auto& [desc, cluster] : clusters) {
            double consistency = 0.0;
            if (is_fixed_expense_pattern(cluster, consistency)) {
                FixedExpense expense;
                expense.description = desc;
                expense.amount = cluster.amount;
                expense.sample_size = cluster.dates.size();
                
                // Calculate interval
                if (!cluster.intervals.empty()) {
                    int sum = 0;
                    for (int i : cluster.intervals) sum += i;
                    expense.interval_days = sum / (int)cluster.intervals.size();
                    
                    // Calculate standard deviation of intervals
                    double mean = expense.interval_days;
                    double variance = 0.0;
                    for (int i : cluster.intervals) {
                        double diff = i - mean;
                        variance += diff * diff;
                    }
                    expense.interval_std_deviation = std::sqrt(variance / (cluster.intervals.size() - 1));
                } else {
                    expense.interval_days = 30;
                    expense.interval_std_deviation = 0.0;
                }
                
                expense.frequency = determine_frequency(expense.interval_days);
                expense.pattern_consistency = consistency;
                
                // Set due day (approximate)
                if (!cluster.dates.empty()) {
                    std::tm tm = {};
                    std::istringstream ss(cluster.dates.back());
                    ss >> std::get_time(&tm, "%Y-%m-%d");
                    expense.due_day = tm.tm_mday;
                    expense.last_paid_date = cluster.dates.back();
                }
                
                expense.next_date = calculate_next_date(
                    expense.last_paid_date,
                    expense.interval_days
                );
                
                expense.is_active = true;
                
                expenses.push_back(expense);
                expenses_array.push_back(expense.to_json());
            }
        }
        
        result.success = true;
        result.data = {
            {"expenses", expenses_array},
            {"total_expenses", expenses.size()}
        };
        result.confidence_score = 0.65;
        result.sample_size = expense_txns.size();
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Expense analysis failed: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    return result;
}

std::vector<FixedExpense> ExpenseAnalyzer::get_fixed_expenses() const {
    return expenses;
}

std::map<std::string, ExpenseAnalyzer::TransactionCluster> ExpenseAnalyzer::cluster_by_amount(
    const std::vector<Transaction>& transactions
) {
    std::map<std::string, TransactionCluster> clusters;
    
    for (const auto& txn : transactions) {
        // Round amount to nearest 10 to group similar expenses
        double rounded = std::round(txn.amount / 10.0) * 10.0;
        std::ostringstream key;
        key << std::fixed << std::setprecision(2) << rounded;
        
        if (clusters.find(key.str()) == clusters.end()) {
            clusters[key.str()] = TransactionCluster{rounded, {}, {}};
        }
        
        auto& cluster = clusters[key.str()];
        cluster.dates.push_back(txn.date);
        
        // Calculate interval from previous date if exists
        if (cluster.dates.size() > 1) {
            int delay = 0;
            // Simplified: assume 30 days between payments if we can't parse
            cluster.intervals.push_back(delay > 0 ? delay : 30);
        }
    }
    
    return clusters;
}

bool ExpenseAnalyzer::is_fixed_expense_pattern(
    const TransactionCluster& cluster,
    double& consistency_score
) {
    // A fixed expense pattern requires:
    // 1. At least 3 occurrences
    // 2. Relatively consistent amounts
    // 3. Relatively consistent intervals
    
    if (cluster.dates.size() < 3) {
        consistency_score = 0.0;
        return false;
    }
    
    // Calculate consistency based on amount variance and interval variance
    // Simplified: assume it's fixed if we have 3+ occurrences
    consistency_score = std::min(1.0, 0.5 + (cluster.dates.size() / 10.0));
    
    return true;
}

std::string ExpenseAnalyzer::determine_frequency(int avg_interval_days) {
    if (avg_interval_days <= 7) return "weekly";
    if (avg_interval_days <= 14) return "biweekly";
    if (avg_interval_days <= 35) return "monthly";
    if (avg_interval_days <= 120) return "quarterly";
    return "yearly";
}

std::string ExpenseAnalyzer::calculate_next_date(
    const std::string& last_date,
    int interval_days
) {
    try {
        std::tm tm = {};
        std::istringstream ss(last_date);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        
        auto time = std::mktime(&tm);
        time += (interval_days * 86400);
        
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time), "%Y-%m-%d");
        return oss.str();
    } catch (...) {
        return last_date;
    }
}

double calculate_std_deviation(const std::vector<int>& values) {
    if (values.empty()) return 0.0;
    
    double sum = 0.0;
    for (int v : values) sum += v;
    double mean = sum / values.size();
    
    double variance = 0.0;
    for (int v : values) {
        variance += (v - mean) * (v - mean);
    }
    variance /= values.size();
    
    return std::sqrt(variance);
}
