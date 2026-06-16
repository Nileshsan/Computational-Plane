#pragma once

#include <nlohmann/json.hpp>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

using json = nlohmann::json;

// ============================================================================
// ML-Based Cashflow Prediction with 3-Stage Architecture
// ============================================================================

namespace MLCashflow {

// ============================================================================
// STAGE 1: GLOBAL MODEL - Company-Wide Patterns
// ============================================================================

struct GlobalPaymentMetrics {
    double global_mean_payment_days = 0.0;
    double global_std_dev = 0.0;
    double global_on_time_percentage = 0.0;
    double company_growth_factor = 1.0;
    int total_transactions = 0;
    int on_time_transactions = 0;
    
    json to_json() const;
};

// ============================================================================
// STAGE 2: STATISTICAL MODEL - Main Logic & Statistics
// ============================================================================

struct StatisticalModel {
    // Client-specific statistics
    double client_mean_days = 0.0;
    double client_std_dev = 0.0;
    double client_median_days = 0.0;
    double client_percentile_90 = 0.0;
    
    // Reliability metrics
    double on_time_percentage = 0.0;
    double payment_consistency = 0.0;  // 0-100
    double early_payment_percentage = 0.0;
    double late_payment_percentage = 0.0;
    
    // Statistical distributions
    double normal_distribution_fit = 0.0;  // How well payment days fit normal dist
    
    // Methods
    double calculate_normal_cdf(double x, double mean, double std_dev) const;
    double calculate_probability_pay_by_day(double payment_day) const;
    double calculate_probability_pay_amount_by_day(double amount, double payment_day) const;
    
    json to_json() const;
};

// ============================================================================
// STAGE 3: ML MODEL - Machine Learning Predictions
// ============================================================================

struct DecisionTreeNode {
    std::string feature_name;
    double threshold = 0.0;
    double probability = 0.0;
    int samples = 0;
    
    // For leaf nodes
    bool is_leaf = false;
    double leaf_probability = 0.0;
    
    // Child nodes (simplified binary tree)
    std::shared_ptr<DecisionTreeNode> left = nullptr;
    std::shared_ptr<DecisionTreeNode> right = nullptr;
};

struct MLPredictionModel {
    // Feature importance
    std::map<std::string, double> feature_importance;
    
    // Decision tree for payment probability
    std::shared_ptr<DecisionTreeNode> payment_tree = nullptr;
    
    // Ensemble weights (combining statistical + ML)
    double statistical_weight = 0.4;  // 40% statistical baseline
    double ml_weight = 0.6;            // 60% ML enhancements
    
    // Model parameters learned from data
    double client_deviation_factor = 0.0;  // How much client deviates from global
    double seasonality_factor = 1.0;
    double growth_momentum = 0.0;
    
    // Methods
    double predict_payment_probability(
        double payment_day,
        const StatisticalModel& stat_model,
        double client_reliability_score
    ) const;
    
    double predict_amount_payment_probability(
        double amount,
        double payment_day,
        double client_average_payment_amount,
        const StatisticalModel& stat_model
    ) const;
    
    json to_json() const;
};

// ============================================================================
// INTEGRATED 3-STAGE PREDICTOR
// ============================================================================

struct PaymentProbabilityPrediction {
    double amount = 0.0;
    double payment_day = 0.0;
    double probability_percentage = 0.0;
    
    // Stage breakdown
    double global_baseline = 0.0;      // Stage 1
    double statistical_component = 0.0; // Stage 2
    double ml_adjustment = 0.0;         // Stage 3
    
    // Confidence and reasoning
    double confidence_score = 0.0;
    std::string prediction_reasoning;
    
    json to_json() const;
};

class MLCashflowPredictor {
public:
    MLCashflowPredictor();
    
    // Initialize with company-wide data
    void initialize_global_model(const std::vector<json>& client_histories);
    
    // Train on client historical data
    void train_client_model(
        const std::string& client_name,
        const std::vector<double>& payment_days,
        const std::vector<double>& payment_amounts,
        double on_time_percentage
    );
    
    // Core prediction methods
    PaymentProbabilityPrediction predict_payment_probability(
        const std::string& client_name,
        double amount,
        double payment_day_threshold
    );
    
    std::vector<PaymentProbabilityPrediction> predict_multiple_scenarios(
        const std::string& client_name,
        double amount,
        const std::vector<double>& day_thresholds
    );
    
    // 3-stage breakdown analysis
    json get_prediction_with_stages(
        const std::string& client_name,
        double amount,
        double payment_day_threshold
    );
    
    // Forecasting multiple amounts and days
    json forecast_cashflow_ml(
        const std::string& client_name,
        double current_outstanding,
        const std::vector<double>& amounts_to_predict,
        const std::vector<int>& days_to_predict
    );
    
    // Get model information
    json get_model_summary() const;
    json get_client_model_details(const std::string& client_name) const;
    
private:
    // Stage 1: Global model
    GlobalPaymentMetrics global_model;
    
    // Stage 2 & 3: Client-specific models
    std::map<std::string, StatisticalModel> statistical_models;
    std::map<std::string, MLPredictionModel> ml_models;
    
    // Helper methods
    StatisticalModel build_statistical_model(
        const std::vector<double>& payment_days,
        const std::vector<double>& payment_amounts,
        double on_time_percentage
    );
    
    MLPredictionModel build_ml_model(
        const StatisticalModel& stat_model,
        const std::vector<double>& payment_days,
        double on_time_percentage
    );
    
    double calculate_payment_consistency(const std::vector<double>& payment_days);
    double calculate_normal_distribution_fit(
        const std::vector<double>& values,
        double mean,
        double std_dev
    );
    
    // ML-specific helpers
    double apply_ml_adjustments(
        const MLPredictionModel& ml_model,
        const StatisticalModel& stat_model,
        double base_probability,
        double payment_day
    );
    
    double calculate_amount_impact(
        double amount,
        double avg_payment_amount,
        double amount_std_dev
    );
};

}  // namespace MLCashflow
