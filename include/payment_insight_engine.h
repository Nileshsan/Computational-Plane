#ifndef PAYMENT_INSIGHT_ENGINE_H
#define PAYMENT_INSIGHT_ENGINE_H

#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <ctime>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// ============================================================================
// Advanced Statistical Analysis & ML Engine for Payment Behavior
// ============================================================================

// Statistical constants
constexpr double Z_SCORE_95 = 1.96;      // 95% confidence interval
constexpr double Z_SCORE_99 = 2.576;     // 99% confidence interval
constexpr double IQR_MULTIPLIER = 1.5;   // Standard IQR multiplier for outliers

// Payment classification thresholds (robust to data size)
struct PaymentThresholds {
    double type_a_max;      // Quick payers (0-15 days)
    double type_b_max;      // Normal payers (16-35 days)
    double type_c_max;      // Slow payers (36-60 days)
    // Type D: >60 days (outliers/risky)
};

// Client payment profile
struct ClientProfile {
    std::string client_name;
    std::vector<int> payment_days_history;     // Days to pay for each transaction
    std::vector<double> amounts;               // Invoice amounts
    std::vector<std::string> dates;            // Transaction dates
    
    // Calculated metrics
    double avg_payment_days = 0.0;
    double median_payment_days = 0.0;
    double std_deviation = 0.0;
    double variance = 0.0;
    double skewness = 0.0;
    double kurtosis = 0.0;
    double cv = 0.0;                           // Coefficient of variation
    
    // Robustness metrics
    double iqr = 0.0;                          // Interquartile range
    double q1 = 0.0;                           // 25th percentile
    double q3 = 0.0;                           // 75th percentile
    double lower_outlier_bound = 0.0;
    double upper_outlier_bound = 0.0;
    
    // Risk assessment
    double reliability_score = 0.0;            // 0-100
    double consistency_score = 0.0;            // 0-100
    double payment_performance = 0.0;          // % on-time payments
    double risk_score = 0.0;                   // 0-100 (higher = riskier)
    
    // Classification
    char classification = 'D';                 // A/B/C/D
    bool is_outlier = false;
    double confidence_score = 0.0;
    
    // Trend analysis
    double payment_trend = 0.0;                // -1 (worsening) to +1 (improving)
    int trend_direction = 0;                   // -1/0/+1
    
    // Forecast
    double predicted_payment_days = 0.0;
    double forecast_upper_bound = 0.0;         // 95% confidence upper
    double forecast_lower_bound = 0.0;         // 95% confidence lower
    
    // Discount optimization
    double optimal_discount_percentage = 0.0;
    double discount_impact_on_cash_flow = 0.0;
    double early_payment_probability = 0.0;
    double roi_on_discount = 0.0;
};

// Clustering result for segmentation
struct ClusterResult {
    std::vector<int> cluster_labels;           // Cluster ID per client
    std::vector<std::vector<double>> centroids; // K-means centroids
    double silhouette_score = 0.0;             // Clustering quality 0-1
    int optimal_k = 3;                         // Optimal number of clusters
};

// Discount simulation result
struct DiscountSimulation {
    double discount_percentage = 0.0;
    double early_payment_probability = 0.0;   // Probability of payment within 7 days
    double cash_flow_improvement = 0.0;        // Daily average
    double total_cost = 0.0;                   // Lost revenue
    double net_benefit = 0.0;                  // Improvement - cost
    double roi = 0.0;                          // Return on discount investment
    bool recommended = false;
};

// Cashflow forecast with confidence bands
struct CashflowForecast {
    std::vector<double> forecast_30days;
    std::vector<double> forecast_60days;
    std::vector<double> forecast_90days;
    
    double confidence_90_upper_30d = 0.0;
    double confidence_90_lower_30d = 0.0;
    
    double confidence_90_upper_60d = 0.0;
    double confidence_90_lower_60d = 0.0;
    
    double confidence_90_upper_90d = 0.0;
    double confidence_90_lower_90d = 0.0;
    
    std::map<std::string, double> client_forecast_30d;
    std::map<std::string, double> client_forecast_60d;
    std::map<std::string, double> client_forecast_90d;
};

// Main engine class
class PaymentInsightEngine {
public:
    PaymentInsightEngine();
    
    // Main analysis pipeline
    json AnalyzeAllClients(const json& request);
    json AnalyzeSingleClient(const json& request);
    
    // Advanced analytical functions
    void CalculateRobustStatistics(ClientProfile& client);
    void DetectOutliers(ClientProfile& client);
    void AnalyzeTrend(ClientProfile& client);
    void PredictPaymentBehavior(ClientProfile& client);
    void OptimizeDiscount(ClientProfile& client);
    void ClassifyClient(ClientProfile& client, const PaymentThresholds& thresholds);
    
    // ML-based functions
    ClusterResult PerformKMeansClustering(const std::vector<ClientProfile>& clients, int k = 3);
    double CalculateSilhouetteScore(const std::vector<ClientProfile>& clients, 
                                    const ClusterResult& result);
    int FindOptimalClusters(const std::vector<ClientProfile>& clients, 
                           int max_k = 5);
    
    // Discount optimization
    DiscountSimulation SimulateDiscount(const ClientProfile& client, double discount_pct);
    double EstimateEarlyPaymentProbability(const ClientProfile& client, double discount_pct);
    
    // Cashflow forecasting
    CashflowForecast GenerateCashflowForecast(const std::vector<ClientProfile>& clients);
    std::vector<double> ForecastWithConfidenceIntervals(const ClientProfile& client, 
                                                        int days_ahead);
    
    // Utility functions
    double CalculateMean(const std::vector<double>& data);
    double CalculateMedian(std::vector<double>& data);
    double CalculateStdDev(const std::vector<double>& data, double mean);
    double CalculateSkewness(const std::vector<double>& data, double mean, double std_dev);
    double CalculateKurtosis(const std::vector<double>& data, double mean, double std_dev);
    void CalculatePercentiles(const std::vector<double>& data, 
                             double& q1, double& q3);
    
    // Statistical tests
    double PerformADFTest(const std::vector<double>& time_series);  // Stationarity
    double PerformShapiroWilkTest(std::vector<double>& data);       // Normality
    
    // Bootstrap methods for small samples
    std::vector<double> BootstrapConfidenceInterval(const std::vector<double>& data, 
                                                    int iterations = 1000, 
                                                    double confidence = 0.95);
    
private:
    PaymentThresholds thresholds;
    std::mt19937 rng;  // Random number generator for bootstrap
};

#endif // PAYMENT_INSIGHT_ENGINE_H
