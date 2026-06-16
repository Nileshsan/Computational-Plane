#pragma once

#include "data_structures.h"
#include <vector>
#include <map>

/**
 * Categorizes clients based on transaction history and payment behavior.
 * Categories:
 * - VIP: High-value, reliable, consistent payments
 * - Regular: Normal transaction patterns
 * - At-Risk: Declining activity or payment issues
 * - Inactive: No recent transactions
 * 
 * Also determines discount eligibility and suggests discount percentages.
 */
class ClientCategorizer {
public:
    ClientCategorizer();
    
    /**
     * Categorize all clients based on transactions
     */
    AnalysisResult categorize_clients(
        const std::vector<Transaction>& transactions
    );
    
    /**
     * Get categorized clients
     */
    std::vector<ClientCategory> get_client_categories() const;
    
    /**
     * Get discount recommendations for a specific client
     */
    ClientCategory get_discount_recommendation(
        const std::string& party_name
    ) const;
    
private:
    std::vector<ClientCategory> categories;
    
    struct ClientMetrics {
        std::string party_name;
        double total_transactions = 0.0;
        int transaction_count = 0;
        std::string last_transaction_date;
        int on_time_payments = 0;
        int late_payments = 0;
        double average_transaction = 0.0;
    };
    
    /**
     * Calculate client metrics from transactions
     */
    std::map<std::string, ClientMetrics> calculate_metrics(
        const std::vector<Transaction>& transactions
    );
    
    /**
     * Assign category based on metrics
     */
    std::string assign_category(
        const ClientMetrics& metrics,
        int days_since_last,
        double reliability
    );
    
    /**
     * Calculate discount eligibility and percentage
     */
    void calculate_discount(
        ClientCategory& category,
        const ClientMetrics& metrics
    );
    
    /**
     * Calculate payment reliability score
     */
    double calculate_reliability_score(
        int on_time,
        int late,
        double pattern_consistency
    );
};
