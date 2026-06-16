#include "payment_insight_engine.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <map>
#include <set>

// ============================================================================
// Statistical Helper Functions
// ============================================================================

PaymentInsightEngine::PaymentInsightEngine() {
    rng.seed(static_cast<unsigned>(time(nullptr)));
    
    // Initialize adaptive thresholds
    thresholds.type_a_max = 15.0;  // 0-15 days
    thresholds.type_b_max = 35.0;  // 16-35 days
    thresholds.type_c_max = 60.0;  // 36-60 days
    // D: >60 days
}

double PaymentInsightEngine::CalculateMean(const std::vector<double>& data) {
    if (data.empty()) return 0.0;
    return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

double PaymentInsightEngine::CalculateMedian(std::vector<double>& data) {
    if (data.empty()) return 0.0;
    std::sort(data.begin(), data.end());
    
    size_t n = data.size();
    if (n % 2 == 0) {
        return (data[n/2 - 1] + data[n/2]) / 2.0;
    } else {
        return data[n/2];
    }
}

double PaymentInsightEngine::CalculateStdDev(const std::vector<double>& data, double mean) {
    if (data.size() < 2) return 0.0;
    
    double variance = 0.0;
    for (double x : data) {
        variance += (x - mean) * (x - mean);
    }
    variance /= (data.size() - 1);  // Sample variance
    return std::sqrt(variance);
}

double PaymentInsightEngine::CalculateSkewness(const std::vector<double>& data, 
                                               double mean, double std_dev) {
    if (data.size() < 3 || std_dev == 0.0) return 0.0;
    
    double m3 = 0.0;
    for (double x : data) {
        m3 += std::pow(x - mean, 3);
    }
    m3 /= data.size();
    
    return m3 / std::pow(std_dev, 3);
}

double PaymentInsightEngine::CalculateKurtosis(const std::vector<double>& data, 
                                              double mean, double std_dev) {
    if (data.size() < 4 || std_dev == 0.0) return 0.0;
    
    double m4 = 0.0;
    for (double x : data) {
        m4 += std::pow(x - mean, 4);
    }
    m4 /= data.size();
    
    return (m4 / std::pow(std_dev, 4)) - 3.0;  // Excess kurtosis
}

void PaymentInsightEngine::CalculatePercentiles(const std::vector<double>& data, 
                                               double& q1, double& q3) {
    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());
    
    size_t n = sorted.size();
    
    // Calculate Q1 (25th percentile)
    double pos1 = 0.25 * (n + 1);
    if (pos1 < 1) {
        q1 = sorted[0];
    } else if (pos1 >= n) {
        q1 = sorted[n-1];
    } else {
        int lower = static_cast<int>(pos1) - 1;
        int upper = lower + 1;
        double weight = pos1 - (lower + 1);
        q1 = sorted[lower] * (1 - weight) + sorted[upper] * weight;
    }
    
    // Calculate Q3 (75th percentile)
    double pos3 = 0.75 * (n + 1);
    if (pos3 < 1) {
        q3 = sorted[0];
    } else if (pos3 >= n) {
        q3 = sorted[n-1];
    } else {
        int lower = static_cast<int>(pos3) - 1;
        int upper = lower + 1;
        double weight = pos3 - (lower + 1);
        q3 = sorted[lower] * (1 - weight) + sorted[upper] * weight;
    }
}

// ============================================================================
// Robust Statistics Calculation
// ============================================================================

void PaymentInsightEngine::CalculateRobustStatistics(ClientProfile& client) {
    if (client.payment_days_history.empty()) {
        return;
    }
    
    // Convert to doubles for calculation
    std::vector<double> days(client.payment_days_history.begin(), 
                            client.payment_days_history.end());
    
    // Basic statistics
    client.avg_payment_days = CalculateMean(days);
    client.median_payment_days = CalculateMedian(days);
    client.std_deviation = CalculateStdDev(days, client.avg_payment_days);
    client.variance = client.std_deviation * client.std_deviation;
    
    // Coefficient of Variation (robust measure of variability)
    if (client.avg_payment_days > 0) {
        client.cv = client.std_deviation / client.avg_payment_days;
    }
    
    // Higher-order moments
    client.skewness = CalculateSkewness(days, client.avg_payment_days, client.std_deviation);
    client.kurtosis = CalculateKurtosis(days, client.avg_payment_days, client.std_deviation);
    
    // Quartiles and IQR
    CalculatePercentiles(days, client.q1, client.q3);
    client.iqr = client.q3 - client.q1;
    
    // Outlier bounds (using IQR method)
    client.lower_outlier_bound = client.q1 - IQR_MULTIPLIER * client.iqr;
    client.upper_outlier_bound = client.q3 + IQR_MULTIPLIER * client.iqr;
    
    if (client.lower_outlier_bound < 0) {
        client.lower_outlier_bound = 0;
    }
}

// ============================================================================
// Outlier Detection
// ============================================================================

void PaymentInsightEngine::DetectOutliers(ClientProfile& client) {
    if (client.payment_days_history.empty()) {
        client.is_outlier = false;
        return;
    }
    
    // Check if the majority of data points are outliers using Z-score
    std::vector<double> days(client.payment_days_history.begin(), 
                            client.payment_days_history.end());
    
    int outlier_count = 0;
    for (double day : days) {
        double z_score = std::abs((day - client.avg_payment_days) / 
                                 (client.std_deviation > 0 ? client.std_deviation : 1.0));
        if (z_score > Z_SCORE_95) {
            outlier_count++;
        }
    }
    
    // If <20% are outliers, mark client as not outlier (normal behavior)
    // If >50% are outliers, mark as outlier (erratic behavior)
    double outlier_ratio = static_cast<double>(outlier_count) / days.size();
    client.is_outlier = outlier_ratio > 0.5;
}

// ============================================================================
// Trend Analysis (Time Series)
// ============================================================================

void PaymentInsightEngine::AnalyzeTrend(ClientProfile& client) {
    if (client.payment_days_history.size() < 3) {
        client.payment_trend = 0.0;
        client.trend_direction = 0;
        return;
    }
    
    // Simple linear regression on payment days
    int n = client.payment_days_history.size();
    double sum_x = n * (n + 1) / 2.0;  // 1+2+3+...+n
    double sum_y = 0.0;
    double sum_xy = 0.0;
    double sum_x2 = 0.0;
    
    for (int i = 0; i < n; ++i) {
        int x = i + 1;  // Day index (1-based)
        double y = client.payment_days_history[i];
        
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    // Normalize slope to -1 to +1 range
    // Assuming max change is 30 days per transaction
    client.payment_trend = std::max(-1.0, std::min(1.0, slope / 30.0));
    
    if (slope > 0.5) {
        client.trend_direction = 1;   // Worsening (paying slower)
    } else if (slope < -0.5) {
        client.trend_direction = -1;  // Improving (paying faster)
    } else {
        client.trend_direction = 0;   // Stable
    }
}

// ============================================================================
// Payment Behavior Prediction
// ============================================================================

void PaymentInsightEngine::PredictPaymentBehavior(ClientProfile& client) {
    if (client.payment_days_history.empty()) {
        client.predicted_payment_days = 0;
        return;
    }
    
    std::vector<double> days(client.payment_days_history.begin(), 
                            client.payment_days_history.end());
    
    // For small samples (<5), use median + trend adjustment
    // For larger samples, use mean + bootstrapped confidence intervals
    
    if (days.size() < 5) {
        // Simple approach: median + trend
        client.predicted_payment_days = client.median_payment_days + 
                                       (client.payment_trend * 2);  // Trend weight
    } else if (days.size() < 20) {
        // Medium: EWMA (Exponential Weighted Moving Average)
        double alpha = 0.3;  // Smoothing factor
        double ewma = days[0];
        for (size_t i = 1; i < days.size(); ++i) {
            ewma = alpha * days[i] + (1 - alpha) * ewma;
        }
        client.predicted_payment_days = ewma + (client.payment_trend * 1.5);
    } else {
        // Large: Use mean with trend adjustment
        client.predicted_payment_days = client.avg_payment_days + 
                                       (client.payment_trend * 1.0);
    }
    
    // Generate 95% confidence interval using bootstrap
    auto interval = BootstrapConfidenceInterval(days, 1000, 0.95);
    if (interval.size() == 2) {
        client.forecast_lower_bound = interval[0];
        client.forecast_upper_bound = interval[1];
    }
}

// ============================================================================
// Bootstrap Confidence Intervals (Robust for Small Samples)
// ============================================================================

std::vector<double> PaymentInsightEngine::BootstrapConfidenceInterval(
    const std::vector<double>& data, 
    int iterations, 
    double confidence) {
    
    if (data.size() < 2) {
        return {data[0], data[0]};
    }
    
    std::vector<double> bootstrap_means;
    std::uniform_int_distribution<> dis(0, data.size() - 1);
    
    for (int i = 0; i < iterations; ++i) {
        double sum = 0.0;
        for (size_t j = 0; j < data.size(); ++j) {
            sum += data[dis(rng)];
        }
        bootstrap_means.push_back(sum / data.size());
    }
    
    std::sort(bootstrap_means.begin(), bootstrap_means.end());
    
    double alpha = 1.0 - confidence;
    int lower_idx = static_cast<int>(bootstrap_means.size() * (alpha / 2));
    int upper_idx = static_cast<int>(bootstrap_means.size() * (1 - alpha / 2));
    
    return {bootstrap_means[lower_idx], bootstrap_means[upper_idx]};
}

// ============================================================================
// Client Classification (Adaptive Based on Data)
// ============================================================================

void PaymentInsightEngine::ClassifyClient(ClientProfile& client, 
                                         const PaymentThresholds& thresholds) {
    // If outlier, always classify as D
    if (client.is_outlier) {
        client.classification = 'D';
        client.confidence_score = 0.4;  // Low confidence for outliers
        return;
    }
    
    double pred_days = client.predicted_payment_days;
    
    // Classify based on predicted (future) payment, not just history
    if (pred_days <= thresholds.type_a_max) {
        client.classification = 'A';
    } else if (pred_days <= thresholds.type_b_max) {
        client.classification = 'B';
    } else if (pred_days <= thresholds.type_c_max) {
        client.classification = 'C';
    } else {
        client.classification = 'D';
    }
    
    // Confidence score based on data quality and consistency
    double base_confidence = 0.5;
    
    // More data = higher confidence
    double data_factor = std::min(1.0, static_cast<double>(client.payment_days_history.size()) / 10.0);
    
    // Lower CV (consistency) = higher confidence
    double consistency_factor = 1.0 / (1.0 + client.cv);
    
    // Bootstrap interval width = lower confidence
    double interval_width = client.forecast_upper_bound - client.forecast_lower_bound;
    double prediction_factor = 1.0 / (1.0 + interval_width / 30.0);
    
    client.confidence_score = base_confidence + 0.5 * 
        (data_factor * 0.3 + consistency_factor * 0.4 + prediction_factor * 0.3);
    
    client.confidence_score = std::min(1.0, std::max(0.0, client.confidence_score));
}

// ============================================================================
// Risk & Reliability Scoring
// ============================================================================

void PaymentInsightEngine::OptimizeDiscount(ClientProfile& client) {
    // Reliability Score (0-100)
    // Based on: % on-time, consistency, trend
    double on_time_pct = 0.0;
    for (int days : client.payment_days_history) {
        if (days <= 30) on_time_pct += 1.0;  // Assume SLA is 30 days
    }
    on_time_pct = (client.payment_days_history.empty()) ? 0 : 
                  on_time_pct * 100.0 / client.payment_days_history.size();
    
    double consistency = 100.0 / (1.0 + client.cv);  // 0-100
    double trend_factor = (client.trend_direction == -1) ? 10.0 : 0.0;  // Improving trend
    
    client.reliability_score = 0.4 * on_time_pct + 0.3 * consistency + 0.3 * trend_factor;
    client.payment_performance = on_time_pct;
    
    // Consistency Score (0-100)
    client.consistency_score = 100.0 / (1.0 + client.cv * 2.0);
    
    // Risk Score (0-100, higher = riskier)
    // Inverse of reliability + payment dynamics
    client.risk_score = 100.0 - client.reliability_score;
    
    if (client.is_outlier) {
        client.risk_score += 30.0;  // Add risk for outliers
    }
    
    client.risk_score = std::min(100.0, std::max(0.0, client.risk_score));
    
    // ===== DISCOUNT OPTIMIZATION =====
    
    // Simulate discounts and find optimal
    DiscountSimulation best_discount;
    best_discount.roi = -1000.0;
    
    for (double pct = 0.5; pct <= 3.0; pct += 0.5) {
        auto sim = SimulateDiscount(client, pct);
        
        if (sim.roi > best_discount.roi) {
            best_discount = sim;
        }
    }
    
    client.optimal_discount_percentage = best_discount.recommended ? 
                                        best_discount.discount_percentage : 0.0;
    client.discount_impact_on_cash_flow = best_discount.cash_flow_improvement;
    client.early_payment_probability = best_discount.early_payment_probability;
    client.roi_on_discount = best_discount.roi;
}

// ============================================================================
// Discount Simulation Engine
// ============================================================================

double PaymentInsightEngine::EstimateEarlyPaymentProbability(
    const ClientProfile& client, double discount_pct) {
    
    // Model: Higher reliability + lower payment days = more likely to take discount
    // Using logistic function
    
    double base_reliability = client.reliability_score / 100.0;
    double payment_speed = 1.0 / (1.0 + client.predicted_payment_days / 30.0);
    double discount_factor = std::min(1.0, discount_pct / 2.0);  // Diminishing returns
    
    // Logistic curve: probability ranges 0-1
    double z = base_reliability * 2.0 + payment_speed * 1.5 + discount_factor * 1.0 - 1.5;
    double probability = 1.0 / (1.0 + exp(-z));
    
    return probability;
}

DiscountSimulation PaymentInsightEngine::SimulateDiscount(
    const ClientProfile& client, double discount_pct) {
    
    DiscountSimulation sim;
    sim.discount_percentage = discount_pct;
    sim.early_payment_probability = EstimateEarlyPaymentProbability(client, discount_pct);
    
    // Calculate carrying cost benefit
    // cost of carrying = avg amount * (1 - early_pay_prob) * interest_rate * days / 365
    double avg_amount = client.amounts.empty() ? 50000.0 : 
                       std::accumulate(client.amounts.begin(), client.amounts.end(), 0.0) / 
                       client.amounts.size();
    
    constexpr double INTEREST_RATE = 0.10;  // 10% annual
    
    // Days saved if customer pays early
    double days_saved = client.predicted_payment_days - 7.0;  // Assume pays in 7 days with discount
    if (days_saved < 0) days_saved = 0;
    
    // Carrying cost avoided
    double carrying_cost_saved = avg_amount * sim.early_payment_probability * 
                                INTEREST_RATE * days_saved / 365.0;
    
    // Total cost of discount
    sim.total_cost = avg_amount * sim.early_payment_probability * (discount_pct / 100.0);
    
    // Net benefit
    if (days_saved > 0) {
        sim.cash_flow_improvement = carrying_cost_saved / days_saved;
    } else {
        sim.cash_flow_improvement = 0.0;
    }
    sim.net_benefit = carrying_cost_saved - sim.total_cost;
    
    // ROI calculation
    if (sim.total_cost > 0) {
        sim.roi = (sim.net_benefit / sim.total_cost) * 100.0;
    } else {
        sim.roi = 0;
    }
    
    // Recommendation: Offer discount only if positive ROI and high early payment prob
    sim.recommended = (sim.roi > 10.0 && sim.early_payment_probability > 0.6);
    
    return sim;
}

// ============================================================================
// K-Means Clustering (Client Segmentation)
// ============================================================================

ClusterResult PaymentInsightEngine::PerformKMeansClustering(
    const std::vector<ClientProfile>& clients, int k) {
    
    ClusterResult result;
    result.cluster_labels.resize(clients.size());
    result.optimal_k = k;
    
    if (clients.empty() || k <= 0) return result;
    
    // Convert clients to feature space: [avg_days, std_dev, cv]
    std::vector<std::vector<double>> data;
    for (const auto& client : clients) {
        data.push_back({client.avg_payment_days, client.std_deviation, client.cv});
    }
    
    // Initialize centroids randomly
    std::vector<std::vector<double>> centroids;
    std::uniform_int_distribution<> dis(0, clients.size() - 1);
    
    for (int i = 0; i < k; ++i) {
        centroids.push_back(data[dis(rng)]);
    }
    
    // K-means iterations
    const int MAX_ITERATIONS = 100;
    const double CONVERGENCE_THRESHOLD = 1e-4;
    
    for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
        // Assign points to nearest centroid
        std::vector<int> new_labels(clients.size());
        
        for (size_t i = 0; i < data.size(); ++i) {
            double min_dist = std::numeric_limits<double>::max();
            int best_centroid = 0;
            
            for (int c = 0; c < k; ++c) {
                double dist = 0.0;
                for (size_t d = 0; d < 3; ++d) {
                    dist += (data[i][d] - centroids[c][d]) * 
                           (data[i][d] - centroids[c][d]);
                }
                dist = std::sqrt(dist);
                
                if (dist < min_dist) {
                    min_dist = dist;
                    best_centroid = c;
                }
            }
            
            new_labels[i] = best_centroid;
        }
        
        // Update centroids
        std::vector<std::vector<double>> new_centroids(k, 
                                                       std::vector<double>(3, 0.0));
        std::vector<int> counts(k, 0);
        
        for (size_t i = 0; i < data.size(); ++i) {
            int label = new_labels[i];
            for (size_t d = 0; d < 3; ++d) {
                new_centroids[label][d] += data[i][d];
            }
            counts[label]++;
        }
        
        for (int c = 0; c < k; ++c) {
            if (counts[c] > 0) {
                for (size_t d = 0; d < 3; ++d) {
                    new_centroids[c][d] /= counts[c];
                }
            }
        }
        
        // Check convergence
        double centroid_diff = 0.0;
        for (int c = 0; c < k; ++c) {
            for (size_t d = 0; d < 3; ++d) {
                centroid_diff += std::abs(new_centroids[c][d] - centroids[c][d]);
            }
        }
        
        centroids = new_centroids;
        result.cluster_labels = new_labels;
        
        if (centroid_diff < CONVERGENCE_THRESHOLD) {
            break;
        }
    }
    
    result.centroids = centroids;
    result.silhouette_score = CalculateSilhouetteScore(clients, result);
    
    return result;
}

double PaymentInsightEngine::CalculateSilhouetteScore(
    const std::vector<ClientProfile>& clients,
    const ClusterResult& result) {
    
    if (clients.empty()) return 0.0;
    
    double total_silhouette = 0.0;
    
    for (size_t i = 0; i < clients.size(); ++i) {
        std::vector<double> point = {clients[i].avg_payment_days, 
                                     clients[i].std_deviation, 
                                     clients[i].cv};
        int cluster = result.cluster_labels[i];
        
        // Within-cluster distance (a)
        double a = 0.0;
        int cluster_count = 0;
        for (size_t j = 0; j < clients.size(); ++j) {
            if (result.cluster_labels[j] == cluster && i != j) {
                std::vector<double> other = {clients[j].avg_payment_days,
                                            clients[j].std_deviation,
                                            clients[j].cv};
                double dist = 0.0;
                for (size_t d = 0; d < 3; ++d) {
                    dist += (point[d] - other[d]) * (point[d] - other[d]);
                }
                a += std::sqrt(dist);
                cluster_count++;
            }
        }
        if (cluster_count > 0) a /= cluster_count;
        
        // Between-cluster distance (b)
        double b = std::numeric_limits<double>::max();
        for (size_t c = 0; c < result.centroids.size(); ++c) {
            if (static_cast<int>(c) != cluster) {
                double dist = 0.0;
                for (size_t d = 0; d < 3; ++d) {
                    dist += (point[d] - result.centroids[c][d]) * 
                           (point[d] - result.centroids[c][d]);
                }
                dist = std::sqrt(dist);
                b = std::min(b, dist);
            }
        }
        
        // Silhouette coefficient
        double max_dist = std::max(a, b);
        double silhouette = (max_dist > 0) ? (b - a) / max_dist : 0;
        total_silhouette += silhouette;
    }
    
    return total_silhouette / clients.size();
}

int PaymentInsightEngine::FindOptimalClusters(const std::vector<ClientProfile>& clients, 
                                             int max_k) {
    
    if (clients.size() < 3) return 1;
    
    int max_k_possible = std::min(max_k, static_cast<int>(clients.size()));
    double best_score = -2.0;
    int best_k = 1;
    
    for (int k = 1; k <= max_k_possible; ++k) {
        auto result = PerformKMeansClustering(clients, k);
        
        if (result.silhouette_score > best_score) {
            best_score = result.silhouette_score;
            best_k = k;
        }
    }
    
    return best_k;
}

// ============================================================================
// Cashflow Forecasting
// ============================================================================

CashflowForecast PaymentInsightEngine::GenerateCashflowForecast(
    const std::vector<ClientProfile>& clients) {
    
    CashflowForecast forecast;
    
    for (const auto& client : clients) {
        // Expected cash inflow by client
        double expected_amount = client.amounts.empty() ? 0.0 : 
                                std::accumulate(client.amounts.begin(), 
                                              client.amounts.end(), 0.0);
        
        // Timeline: when will this payment come in?
        int expected_days = static_cast<int>(client.predicted_payment_days);
        
        // Add to corresponding forecast bucket with confidence weighting
        double confidence_weight = client.confidence_score;
        
        forecast.client_forecast_30d[client.client_name] = (expected_days <= 30) ? 
                                                           expected_amount * confidence_weight : 0.0;
        forecast.client_forecast_60d[client.client_name] = (expected_days <= 60) ? 
                                                           expected_amount * confidence_weight : 0.0;
        forecast.client_forecast_90d[client.client_name] = (expected_days <= 90) ? 
                                                           expected_amount * confidence_weight : 0.0;
        
        // Aggregate
        forecast.forecast_30days.push_back(forecast.client_forecast_30d[client.client_name]);
        forecast.forecast_60days.push_back(forecast.client_forecast_60d[client.client_name]);
        forecast.forecast_90days.push_back(forecast.client_forecast_90d[client.client_name]);
    }
    
    // Calculate confidence intervals
    if (!forecast.forecast_30days.empty()) {
        auto interval_30 = BootstrapConfidenceInterval(forecast.forecast_30days);
        forecast.confidence_90_lower_30d = interval_30[0];
        forecast.confidence_90_upper_30d = interval_30[1];
    }
    
    if (!forecast.forecast_60days.empty()) {
        auto interval_60 = BootstrapConfidenceInterval(forecast.forecast_60days);
        forecast.confidence_90_lower_60d = interval_60[0];
        forecast.confidence_90_upper_60d = interval_60[1];
    }
    
    if (!forecast.forecast_90days.empty()) {
        auto interval_90 = BootstrapConfidenceInterval(forecast.forecast_90days);
        forecast.confidence_90_lower_90d = interval_90[0];
        forecast.confidence_90_upper_90d = interval_90[1];
    }
    
    return forecast;
}

// ============================================================================
// Main Analysis Pipelines
// ============================================================================

json PaymentInsightEngine::AnalyzeAllClients(const json& request) {
    json response = json::object();
    
    try {
        auto clients_json = request.value("clients", json::array());
        
        std::vector<ClientProfile> clients;
        
        // Parse input data
        for (const auto& client_data : clients_json) {
            ClientProfile profile;
            profile.client_name = client_data.value("name", "Unknown");
            
            auto payment_days = client_data.value("payment_days_history", json::array());
            for (auto day : payment_days) {
                profile.payment_days_history.push_back(day.get<int>());
            }
            
            auto amounts_arr = client_data.value("amounts", json::array());
            for (auto amt : amounts_arr) {
                profile.amounts.push_back(amt.get<double>());
            }
            
            // Run analysis pipeline
            CalculateRobustStatistics(profile);
            DetectOutliers(profile);
            AnalyzeTrend(profile);
            PredictPaymentBehavior(profile);
            ClassifyClient(profile, thresholds);
            OptimizeDiscount(profile);
            
            clients.push_back(profile);
        }
        
        // Perform clustering for segmentation
        int optimal_k = FindOptimalClusters(clients, 5);
        auto clustering = PerformKMeansClustering(clients, optimal_k);
        
        // Generate cashflow forecast
        auto cashflow = GenerateCashflowForecast(clients);
        
        // Build response
        response["success"] = true;
        response["analysis_type"] = "advanced_payment_insight";
        response["timestamp"] = json(time(nullptr));
        
        // Client analyses
        json clients_array = json::array();
        
        int type_a_count = 0, type_b_count = 0, type_c_count = 0, type_d_count = 0;
        
        for (const auto& client : clients) {
            json client_json = json::object();
            client_json["name"] = client.client_name;
            client_json["classification"] = std::string(1, client.classification);
            client_json["confidence_score"] = std::round(client.confidence_score * 100) / 100.0;
            client_json["is_outlier"] = client.is_outlier;
            
            // Statistics
            json stats = json::object();
            stats["avg_payment_days"] = std::round(client.avg_payment_days * 100) / 100.0;
            stats["median_payment_days"] = std::round(client.median_payment_days * 100) / 100.0;
            stats["std_deviation"] = std::round(client.std_deviation * 100) / 100.0;
            stats["coefficient_of_variation"] = std::round(client.cv * 100) / 100.0;
            stats["skewness"] = std::round(client.skewness * 100) / 100.0;
            stats["kurtosis"] = std::round(client.kurtosis * 100) / 100.0;
            
            client_json["statistics"] = stats;
            
            // Risk and scores
            json risk = json::object();
            risk["reliability_score"] = std::round(client.reliability_score * 100) / 100.0;
            risk["consistency_score"] = std::round(client.consistency_score * 100) / 100.0;
            risk["risk_score"] = std::round(client.risk_score * 100) / 100.0;
            risk["payment_performance"] = std::round(client.payment_performance * 100) / 100.0;
            
            client_json["risk_assessment"] = risk;
            
            // Prediction
            json prediction = json::object();
            prediction["predicted_days"] = std::round(client.predicted_payment_days * 100) / 100.0;
            prediction["forecast_lower"] = std::round(client.forecast_lower_bound * 100) / 100.0;
            prediction["forecast_upper"] = std::round(client.forecast_upper_bound * 100) / 100.0;
            prediction["trend"] = client.trend_direction;
            prediction["trend_value"] = std::round(client.payment_trend * 100) / 100.0;
            
            client_json["prediction"] = prediction;
            
            // Discount optimization
            json discount = json::object();
            discount["optimal_discount_percentage"] = std::round(client.optimal_discount_percentage * 100) / 100.0;
            discount["early_payment_probability"] = std::round(client.early_payment_probability * 100) / 100.0;
            discount["cash_flow_improvement"] = std::round(client.discount_impact_on_cash_flow * 100) / 100.0;
            discount["roi_on_discount"] = std::round(client.roi_on_discount * 100) / 100.0;
            
            client_json["discount_optimization"] = discount;
            
            clients_array.push_back(client_json);
            
            // Count classifications
            if (client.classification == 'A') type_a_count++;
            else if (client.classification == 'B') type_b_count++;
            else if (client.classification == 'C') type_c_count++;
            else if (client.classification == 'D') type_d_count++;
        }
        
        response["clients"] = clients_array;
        
        // Summary statistics
        json summary = json::object();
        summary["total_clients_analyzed"] = clients.size();
        summary["type_a_count"] = type_a_count;
        summary["type_b_count"] = type_b_count;
        summary["type_c_count"] = type_c_count;
        summary["type_d_count"] = type_d_count;
        summary["outlier_count"] = std::count_if(clients.begin(), clients.end(),
                                                 [](const ClientProfile& c) { return c.is_outlier; });
        summary["optimal_clusters"] = optimal_k;
        summary["clustering_quality_silhouette"] = std::round(clustering.silhouette_score * 100) / 100.0;
        
        response["summary"] = summary;
        
        // Cashflow forecast
        json forecast_json = json::object();
        forecast_json["30_day_forecast"] = std::round(
            std::accumulate(cashflow.forecast_30days.begin(), 
                          cashflow.forecast_30days.end(), 0.0) * 100) / 100.0;
        forecast_json["30_day_lower"] = std::round(cashflow.confidence_90_lower_30d * 100) / 100.0;
        forecast_json["30_day_upper"] = std::round(cashflow.confidence_90_upper_30d * 100) / 100.0;
        
        forecast_json["60_day_forecast"] = std::round(
            std::accumulate(cashflow.forecast_60days.begin(), 
                          cashflow.forecast_60days.end(), 0.0) * 100) / 100.0;
        forecast_json["60_day_lower"] = std::round(cashflow.confidence_90_lower_60d * 100) / 100.0;
        forecast_json["60_day_upper"] = std::round(cashflow.confidence_90_upper_60d * 100) / 100.0;
        
        forecast_json["90_day_forecast"] = std::round(
            std::accumulate(cashflow.forecast_90days.begin(), 
                          cashflow.forecast_90days.end(), 0.0) * 100) / 100.0;
        forecast_json["90_day_lower"] = std::round(cashflow.confidence_90_lower_90d * 100) / 100.0;
        forecast_json["90_day_upper"] = std::round(cashflow.confidence_90_upper_90d * 100) / 100.0;
        
        response["cashflow_forecast"] = forecast_json;
        
        return response;
        
    } catch (const std::exception& e) {
        response["success"] = false;
        response["error"] = std::string("Analysis failed: ") + e.what();
        return response;
    }
}

json PaymentInsightEngine::AnalyzeSingleClient(const json& request) {
    json response = json::object();
    
    try {
        ClientProfile client;
        client.client_name = request.value("name", "Unknown");
        
        auto payment_days = request.value("payment_days_history", json::array());
        for (auto day : payment_days) {
            client.payment_days_history.push_back(day.get<int>());
        }
        
        auto amounts_arr = request.value("amounts", json::array());
        for (auto amt : amounts_arr) {
            client.amounts.push_back(amt.get<double>());
        }
        
        // Run analysis pipeline
        CalculateRobustStatistics(client);
        DetectOutliers(client);
        AnalyzeTrend(client);
        PredictPaymentBehavior(client);
        ClassifyClient(client, thresholds);
        OptimizeDiscount(client);
        
        response["success"] = true;
        response["client_name"] = client.client_name;
        response["classification"] = std::string(1, client.classification);
        response["confidence"] = client.confidence_score;
        response["avg_payment_days"] = client.avg_payment_days;
        response["predicted_payment_days"] = client.predicted_payment_days;
        response["optimal_discount_percentage"] = client.optimal_discount_percentage;
        response["risk_score"] = client.risk_score;
        response["is_outlier"] = client.is_outlier;
        
        return response;
        
    } catch (const std::exception& e) {
        response["success"] = false;
        response["error"] = std::string("Analysis failed: ") + e.what();
        return response;
    }
}
