#include "ml_cashflow_predictor.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace MLCashflow {

// ============================================================================
// Global Payment Metrics Implementation
// ============================================================================

json GlobalPaymentMetrics::to_json() const {
    return json{
        {"global_mean_payment_days", global_mean_payment_days},
        {"global_std_dev", global_std_dev},
        {"global_on_time_percentage", global_on_time_percentage},
        {"company_growth_factor", company_growth_factor},
        {"total_transactions", total_transactions},
        {"on_time_transactions", on_time_transactions}
    };
}

// ============================================================================
// Statistical Model Implementation
// ============================================================================

double StatisticalModel::calculate_normal_cdf(double x, double mean, double std_dev) const {
    if (std_dev <= 0) return 0.5;
    
    // Standard normal CDF approximation using error function
    double z = (x - mean) / std_dev;
    return 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
}

double StatisticalModel::calculate_probability_pay_by_day(double payment_day) const {
    if (client_std_dev <= 0) return 0.5;
    
    // Probability that payment happens by this day using normal CDF
    return calculate_normal_cdf(payment_day, client_mean_days, client_std_dev);
}

double StatisticalModel::calculate_probability_pay_amount_by_day(
    double amount,
    double payment_day
) const {
    // Base probability from timing
    double timing_prob = calculate_probability_pay_by_day(payment_day);
    
    // Adjust for on-time percentage
    double adjusted_prob = timing_prob * (on_time_percentage / 100.0);
    
    // Small boost for amounts matching historical patterns
    adjusted_prob = std::min(adjusted_prob * 1.05, 1.0);
    
    return adjusted_prob;
}

json StatisticalModel::to_json() const {
    return json{
        {"client_mean_days", client_mean_days},
        {"client_std_dev", client_std_dev},
        {"client_median_days", client_median_days},
        {"client_percentile_90", client_percentile_90},
        {"on_time_percentage", on_time_percentage},
        {"payment_consistency", payment_consistency},
        {"early_payment_percentage", early_payment_percentage},
        {"late_payment_percentage", late_payment_percentage},
        {"normal_distribution_fit", normal_distribution_fit}
    };
}

// ============================================================================
// ML Prediction Model Implementation
// ============================================================================

double MLPredictionModel::predict_payment_probability(
    double payment_day,
    const StatisticalModel& stat_model,
    double client_reliability_score
) const {
    // Statistical component (40%)
    double stat_prob = stat_model.calculate_probability_pay_by_day(payment_day);
    
    // ML adjustments (60%)
    double ml_prob = stat_prob;
    
    // Apply client deviation factor
    if (client_deviation_factor > 0) {
        // Positive deviation = better performer
        ml_prob += (client_reliability_score / 100.0) * client_deviation_factor;
    } else {
        // Negative deviation = worse performer
        ml_prob -= (-client_deviation_factor) * (1.0 - client_reliability_score / 100.0);
    }
    
    // Apply seasonality
    ml_prob *= seasonality_factor;
    
    // Apply growth momentum
    ml_prob += growth_momentum * 0.1;
    
    // Weighted ensemble
    double ensemble_prob = (stat_prob * statistical_weight) + (ml_prob * ml_weight);
    
    return std::min(std::max(ensemble_prob, 0.0), 1.0);
}

double MLPredictionModel::predict_amount_payment_probability(
    double amount,
    double payment_day,
    double client_average_payment_amount,
    const StatisticalModel& stat_model
) const {
    // Base probability from timing
    double base_prob = predict_payment_probability(payment_day, stat_model, 85.0);
    
    // Amount-based adjustment
    double amount_ratio = client_average_payment_amount > 0 
        ? amount / client_average_payment_amount 
        : 1.0;
    
    // Smaller amounts have higher probability of payment
    double amount_factor = 1.0 / (1.0 + std::exp(2.0 * (amount_ratio - 1.0)));
    
    return base_prob * amount_factor;
}

json MLPredictionModel::to_json() const {
    json fi_json = json::object();
    for (const auto& [key, value] : feature_importance) {
        fi_json[key] = value;
    }
    
    return json{
        {"feature_importance", fi_json},
        {"statistical_weight", statistical_weight},
        {"ml_weight", ml_weight},
        {"client_deviation_factor", client_deviation_factor},
        {"seasonality_factor", seasonality_factor},
        {"growth_momentum", growth_momentum}
    };
}

// ============================================================================
// Payment Probability Prediction Implementation
// ============================================================================

json PaymentProbabilityPrediction::to_json() const {
    return json{
        {"amount", amount},
        {"payment_day", payment_day},
        {"probability_percentage", probability_percentage},
        {"global_baseline", global_baseline},
        {"statistical_component", statistical_component},
        {"ml_adjustment", ml_adjustment},
        {"confidence_score", confidence_score},
        {"prediction_reasoning", prediction_reasoning}
    };
}

// ============================================================================
// ML Cashflow Predictor Implementation
// ============================================================================

MLCashflowPredictor::MLCashflowPredictor() {
    global_model.company_growth_factor = 1.0;
}

void MLCashflowPredictor::initialize_global_model(const std::vector<json>& client_histories) {
    if (client_histories.empty()) return;
    
    std::vector<double> all_payment_days;
    int total_on_time = 0;
    int total_count = 0;
    
    // Aggregate all payment data
    for (const auto& client_data : client_histories) {
        if (client_data.contains("payment_days") && client_data["payment_days"].is_array()) {
            for (const auto& day : client_data["payment_days"]) {
                all_payment_days.push_back(day.get<double>());
                total_count++;
            }
        }
        
        if (client_data.contains("on_time_count")) {
            total_on_time += client_data["on_time_count"].get<int>();
        }
    }
    
    // Calculate global statistics
    if (!all_payment_days.empty()) {
        global_model.global_mean_payment_days = 
            std::accumulate(all_payment_days.begin(), all_payment_days.end(), 0.0) / all_payment_days.size();
        
        double sum_sq_diff = 0.0;
        for (double day : all_payment_days) {
            double diff = day - global_model.global_mean_payment_days;
            sum_sq_diff += diff * diff;
        }
        global_model.global_std_dev = std::sqrt(sum_sq_diff / all_payment_days.size());
    }
    
    global_model.total_transactions = total_count;
    global_model.on_time_transactions = total_on_time;
    global_model.global_on_time_percentage = total_count > 0 
        ? (100.0 * total_on_time / total_count) 
        : 0.0;
}

void MLCashflowPredictor::train_client_model(
    const std::string& client_name,
    const std::vector<double>& payment_days,
    const std::vector<double>& payment_amounts,
    double on_time_percentage
) {
    // Build Stage 2: Statistical Model
    StatisticalModel stat_model = build_statistical_model(payment_days, payment_amounts, on_time_percentage);
    statistical_models[client_name] = stat_model;
    
    // Build Stage 3: ML Model
    MLPredictionModel ml_model = build_ml_model(stat_model, payment_days, on_time_percentage);
    ml_models[client_name] = ml_model;
}

StatisticalModel MLCashflowPredictor::build_statistical_model(
    const std::vector<double>& payment_days,
    const std::vector<double>& payment_amounts,
    double on_time_percentage
) {
    StatisticalModel model;
    
    if (payment_days.empty()) return model;
    
    // Calculate mean
    model.client_mean_days = std::accumulate(payment_days.begin(), payment_days.end(), 0.0) / payment_days.size();
    
    // Calculate standard deviation
    double sum_sq_diff = 0.0;
    for (double day : payment_days) {
        double diff = day - model.client_mean_days;
        sum_sq_diff += diff * diff;
    }
    model.client_std_dev = std::sqrt(sum_sq_diff / payment_days.size());
    
    // Calculate median
    std::vector<double> sorted_days = payment_days;
    std::sort(sorted_days.begin(), sorted_days.end());
    if (sorted_days.size() % 2 == 0) {
        model.client_median_days = (sorted_days[sorted_days.size() / 2 - 1] + sorted_days[sorted_days.size() / 2]) / 2.0;
    } else {
        model.client_median_days = sorted_days[sorted_days.size() / 2];
    }
    
    // Calculate percentile 90
    int idx_90 = static_cast<int>(0.9 * sorted_days.size());
    model.client_percentile_90 = sorted_days[std::min(idx_90, static_cast<int>(sorted_days.size()) - 1)];
    
    // Reliability metrics
    model.on_time_percentage = on_time_percentage;
    model.payment_consistency = calculate_payment_consistency(payment_days);
    
    // Early vs late payments
    int early_count = 0;
    int late_count = 0;
    for (double day : payment_days) {
        if (day < model.client_mean_days - model.client_std_dev) early_count++;
        else if (day > model.client_mean_days + model.client_std_dev) late_count++;
    }
    model.early_payment_percentage = 100.0 * early_count / payment_days.size();
    model.late_payment_percentage = 100.0 * late_count / payment_days.size();
    
    // Normal distribution fit
    model.normal_distribution_fit = calculate_normal_distribution_fit(payment_days, model.client_mean_days, model.client_std_dev);
    
    return model;
}

MLPredictionModel MLCashflowPredictor::build_ml_model(
    const StatisticalModel& stat_model,
    const std::vector<double>& payment_days,
    double on_time_percentage
) {
    MLPredictionModel model;
    
    // Feature importance
    model.feature_importance["payment_consistency"] = stat_model.payment_consistency / 100.0;
    model.feature_importance["on_time_percentage"] = on_time_percentage / 100.0;
    model.feature_importance["std_dev_inverse"] = 1.0 / (1.0 + stat_model.client_std_dev);
    
    // Client deviation from global average
    if (global_model.global_mean_payment_days > 0) {
        model.client_deviation_factor = 
            (global_model.global_mean_payment_days - stat_model.client_mean_days) / 
            global_model.global_mean_payment_days;
    }
    
    // Seasonality (placeholder - could be enhanced with date analysis)
    model.seasonality_factor = 1.0 + (on_time_percentage - global_model.global_on_time_percentage) / 100.0 * 0.1;
    
    // Growth momentum (placeholder)
    model.growth_momentum = 0.0;
    
    return model;
}

double MLCashflowPredictor::calculate_payment_consistency(const std::vector<double>& payment_days) {
    if (payment_days.empty()) return 0.0;
    
    double mean = std::accumulate(payment_days.begin(), payment_days.end(), 0.0) / payment_days.size();
    double sum_sq_diff = 0.0;
    for (double day : payment_days) {
        double diff = day - mean;
        sum_sq_diff += diff * diff;
    }
    double std_dev = std::sqrt(sum_sq_diff / payment_days.size());
    
    // Consistency = 100 - coefficient of variation
    double cv = mean > 0 ? (std_dev / mean) * 100.0 : 100.0;
    return std::max(0.0, 100.0 - cv);
}

double MLCashflowPredictor::calculate_normal_distribution_fit(
    const std::vector<double>& values,
    double mean,
    double std_dev
) {
    if (values.empty() || std_dev <= 0) return 0.0;
    
    // Calculate Kolmogorov-Smirnov-like statistic
    double max_diff = 0.0;
    for (size_t i = 0; i < values.size(); ++i) {
        double empirical_cdf = static_cast<double>(i + 1) / values.size();
        double z = (values[i] - mean) / std_dev;
        double theoretical_cdf = 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
        max_diff = std::max(max_diff, std::abs(empirical_cdf - theoretical_cdf));
    }
    
    return 1.0 - max_diff;  // Higher score = better fit
}

PaymentProbabilityPrediction MLCashflowPredictor::predict_payment_probability(
    const std::string& client_name,
    double amount,
    double payment_day_threshold
) {
    PaymentProbabilityPrediction prediction;
    prediction.amount = amount;
    prediction.payment_day = payment_day_threshold;
    
    auto stat_it = statistical_models.find(client_name);
    auto ml_it = ml_models.find(client_name);
    
    if (stat_it == statistical_models.end() || ml_it == ml_models.end()) {
        prediction.probability_percentage = 50.0;
        prediction.confidence_score = 0.3;
        prediction.prediction_reasoning = "Client model not found";
        return prediction;
    }
    
    const auto& stat_model = stat_it->second;
    const auto& ml_model = ml_it->second;
    
    // Stage 1: Global baseline
    prediction.global_baseline = global_model.global_on_time_percentage / 100.0;
    
    // Stage 2: Statistical component
    prediction.statistical_component = stat_model.calculate_probability_pay_amount_by_day(amount, payment_day_threshold);
    
    // Stage 3: ML adjustment
    double client_reliability = stat_model.payment_consistency;
    double ml_prob = ml_model.predict_payment_probability(payment_day_threshold, stat_model, client_reliability);
    prediction.ml_adjustment = ml_prob - prediction.statistical_component;
    
    // Final probability as weighted combination
    prediction.probability_percentage = (
        prediction.global_baseline * 0.2 +           // 20% global
        prediction.statistical_component * 0.4 +      // 40% statistical
        ml_prob * 0.4                                 // 40% ML
    ) * 100.0;
    
    prediction.probability_percentage = std::min(std::max(prediction.probability_percentage, 0.0), 100.0);
    
    // Confidence score
    prediction.confidence_score = stat_model.normal_distribution_fit * (stat_model.payment_consistency / 100.0);
    
    // Reasoning
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "Client avg pays in " << stat_model.client_mean_days << " days (±" 
        << stat_model.client_std_dev << "). ";
    oss << "On-time rate: " << stat_model.on_time_percentage << "%. ";
    oss << "Consistency: " << stat_model.payment_consistency << "/100. ";
    oss << "This amount by day " << static_cast<int>(payment_day_threshold) 
        << ": " << prediction.probability_percentage << "% likely.";
    prediction.prediction_reasoning = oss.str();
    
    return prediction;
}

std::vector<PaymentProbabilityPrediction> MLCashflowPredictor::predict_multiple_scenarios(
    const std::string& client_name,
    double amount,
    const std::vector<double>& day_thresholds
) {
    std::vector<PaymentProbabilityPrediction> predictions;
    for (double day : day_thresholds) {
        predictions.push_back(predict_payment_probability(client_name, amount, day));
    }
    return predictions;
}

json MLCashflowPredictor::get_prediction_with_stages(
    const std::string& client_name,
    double amount,
    double payment_day_threshold
) {
    auto prediction = predict_payment_probability(client_name, amount, payment_day_threshold);
    
    return json{
        {"client_name", client_name},
        {"query", {
            {"amount", amount},
            {"payment_day_threshold", payment_day_threshold}
        }},
        {"prediction", prediction.to_json()},
        {"stage_breakdown", {
            {"stage_1_global_model", {
                {"baseline_probability", prediction.global_baseline},
                {"global_metrics", global_model.to_json()}
            }},
            {"stage_2_statistical_model", {
                {"statistical_probability", prediction.statistical_component},
                {"model", statistical_models.at(client_name).to_json()}
            }},
            {"stage_3_ml_model", {
                {"ml_probability", prediction.statistical_component + prediction.ml_adjustment},
                {"ml_adjustment", prediction.ml_adjustment},
                {"model", ml_models.at(client_name).to_json()}
            }}
        }}
    };
}

json MLCashflowPredictor::forecast_cashflow_ml(
    const std::string& client_name,
    double current_outstanding,
    const std::vector<double>& amounts_to_predict,
    const std::vector<int>& days_to_predict
) {
    json forecast = json::object();
    forecast["client_name"] = client_name;
    forecast["current_outstanding"] = current_outstanding;
    
    json scenarios = json::array();
    
    for (double amount : amounts_to_predict) {
        json amount_scenarios = json::object();
        amount_scenarios["amount"] = amount;
        
        json day_predictions = json::array();
        for (int day : days_to_predict) {
            auto pred = predict_payment_probability(client_name, amount, day);
            day_predictions.push_back(pred.to_json());
        }
        
        amount_scenarios["predictions_by_day"] = day_predictions;
        scenarios.push_back(amount_scenarios);
    }
    
    forecast["forecast_scenarios"] = scenarios;
    forecast["summary"] = {
        {"total_scenarios", amounts_to_predict.size() * days_to_predict.size()},
        {"outstanding_amount", current_outstanding},
        {"forecast_periods", days_to_predict}
    };
    
    return forecast;
}

json MLCashflowPredictor::get_model_summary() const {
    json summary = json::object();
    summary["global_model"] = global_model.to_json();
    
    json client_models = json::object();
    for (const auto& [client_name, stat_model] : statistical_models) {
        client_models[client_name] = {
            {"statistical_model", stat_model.to_json()},
            {"ml_model", ml_models.at(client_name).to_json()}
        };
    }
    summary["client_models"] = client_models;
    
    return summary;
}

json MLCashflowPredictor::get_client_model_details(const std::string& client_name) const {
    auto stat_it = statistical_models.find(client_name);
    if (stat_it == statistical_models.end()) {
        return json{{"error", "Client model not found"}};
    }
    
    return json{
        {"client_name", client_name},
        {"stage_2_statistical_model", stat_it->second.to_json()},
        {"stage_3_ml_model", ml_models.at(client_name).to_json()}
    };
}

}  // namespace MLCashflow
