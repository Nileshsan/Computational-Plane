#pragma once

#include "data_structures.h"
#include <vector>
#include <map>

/**
 * Detects fixed/recurring expenses from transaction patterns.
 * Identifies:
 * - Recurring payment amounts
 * - Payment intervals and frequencies
 * - Expected next payment dates
 */
class ExpenseAnalyzer {
public:
    ExpenseAnalyzer();
    
    /**
     * Analyze fixed expenses from transactions
     */
    AnalysisResult analyze_fixed_expenses(
        const std::vector<Transaction>& transactions
    );
    
    /**
     * Get list of identified fixed expenses
     */
    std::vector<FixedExpense> get_fixed_expenses() const;
    
private:
    std::vector<FixedExpense> expenses;
    
    struct TransactionCluster {
        double amount = 0.0;
        std::vector<std::string> dates;
        std::vector<int> intervals;  // days between transactions
    };
    
    /**
     * Cluster transactions by amount to find recurring patterns
     */
    std::map<std::string, TransactionCluster> cluster_by_amount(
        const std::vector<Transaction>& transactions
    );
    
    /**
     * Determine if a cluster represents a fixed expense
     */
    bool is_fixed_expense_pattern(
        const TransactionCluster& cluster,
        double& consistency_score
    );
    
    /**
     * Determine frequency from interval days
     */
    std::string determine_frequency(int avg_interval_days);
    
    /**
     * Calculate next payment date
     */
    std::string calculate_next_date(
        const std::string& last_date,
        int interval_days
    );
};
