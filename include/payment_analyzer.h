#pragma once

#include <data_structures.h>
#include <vector>
#include <map>
#include <memory>

/**
 * Tracks a payment delay with its associated amount for weighted calculations
 */
struct PaymentDelayRecord {
    int days = 0;
    double amount = 0.0;
};

/**
 * Analyzes payment patterns for parties based on transaction history.
 * Uses FIFO matching algorithm identical to Django implementation.
 * Identifies:
 * - Average payment delay (weighted by amount)
 * - Pattern consistency
 * - Payment reliability scores
 */
class PaymentAnalyzer {
public:
    PaymentAnalyzer();
    
    /**
     * Analyze payment patterns from transactions using FIFO matching
     */
    AnalysisResult analyze_payment_patterns(
        const std::vector<Transaction>& transactions,
        int since_days = 30
    );
    
    /**
     * Predict future payment dates for a specific party
     */
    std::vector<CashflowPrediction> predict_future_payments(
        const std::string& party_name,
        int days_ahead = 30
    );
    
    /**
     * Calculate party balances from transactions
     */
    std::map<std::string, double> calculate_party_balances(
        const std::vector<Transaction>& transactions
    );
    
    /**
     * Get analyzed patterns (for external access)
     */
    const std::map<std::string, PaymentPattern>& get_patterns() const {
        return patterns;
    }
    
private:
    std::map<std::string, PaymentPattern> patterns;
    std::map<std::string, std::vector<PaymentDelayRecord>> payment_delays;
    
    int parse_date(const std::string& date_str);
    double calculate_std_deviation(const std::vector<PaymentDelayRecord>& delays);
    double calculate_pattern_consistency(const std::vector<PaymentDelayRecord>& delays);
};
