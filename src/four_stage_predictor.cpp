#include "four_stage_predictor.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <sstream>

namespace PBS::ML {

// ============================================================================
// STAGE 1: GLOBAL PORTFOLIO ANALYZER
// ============================================================================

GlobalPortfolioModel GlobalPortfolioAnalyzer::AnalyzePortfolio(
    const std::vector<ClientPaymentHistory>& allClients) {
    
    GlobalPortfolioModel model;
    
    if (allClients.empty()) {
        return model;
    }
    
    // Calculate portfolio-wide statistics
    std::vector<double> allPaymentDays;
    std::vector<double> onTimeRates;
    
    for (const auto& client : allClients) {
        allPaymentDays.push_back(client.avg_payment_days);
        onTimeRates.push_back(client.on_time_percentage);
    }
    
    // Mean and stdev
    double sum = std::accumulate(allPaymentDays.begin(), allPaymentDays.end(), 0.0);
    model.portfolio_avg_payment_days = sum / allClients.size();
    
    double sq_sum = 0.0;
    for (double days : allPaymentDays) {
        sq_sum += (days - model.portfolio_avg_payment_days) * (days - model.portfolio_avg_payment_days);
    }
    model.portfolio_stdev_days = std::sqrt(sq_sum / allClients.size());
    
    // On-time rate
    double onTimeSum = std::accumulate(onTimeRates.begin(), onTimeRates.end(), 0.0);
    model.portfolio_on_time_rate = onTimeSum / onTimeRates.size() / 100.0;
    
    // Payment quality score (0-1)
    model.portfolio_payment_quality = model.portfolio_on_time_rate * 
                                     (1.0 - std::min(model.portfolio_stdev_days / 30.0, 1.0));
    
    // Seasonal patterns (simulated - should be from actual data)
    model.monthly_collection_rates = {
        0.92, 0.90, 0.88,  // Q1: Dec effect
        0.95, 0.93, 0.91,  // Q2: Spring peak
        0.89, 0.87, 0.85,  // Q3: Summer slump
        0.91, 0.93, 0.95   // Q4: Year-end push
    };
    
    // Trend (positive improving, negative declining)
    model.trend_direction = 0.05;  // Slightly improving
    model.trend_strength = 0.6;    // Moderate confidence
    
    // Economic factors
    model.economic_index = 1.0;
    model.market_volatility = 0.15;
    
    return model;
}

double GlobalPortfolioAnalyzer::GetSeasonalAdjustment(int month) {
    if (month < 1 || month > 12) return 1.0;
    return 0.85 + (0.15 * (0.5 + 0.5 * std::sin((month - 1) * 3.14159 / 6.0)));
}

double GlobalPortfolioAnalyzer::GetEconomicAdjustment() {
    return 1.0;  // Neutral economic environment
}

// ============================================================================
// STAGE 2: STATISTICAL PREDICTION ENGINE
// ============================================================================

ClientStatisticalProfile StatisticalPredictionEngine::AnalyzeClient(
    const ClientPaymentHistory& client) {
    
    ClientStatisticalProfile profile;
    
    profile.mean_payment_days = client.avg_payment_days;
    profile.stdev_payment_days = client.stdev_payment_days;
    
    // Calculate percentiles (assuming normal distribution)
    profile.percentile_90_days = client.avg_payment_days + (1.28 * client.stdev_payment_days);
    profile.percentile_95_days = client.avg_payment_days + (1.645 * client.stdev_payment_days);
    
    profile.on_time_percentage = client.on_time_percentage;
    
    // Consistency score (inverse of coefficient of variation)
    double cv = client.stdev_payment_days / std::max(client.avg_payment_days, 1.0);
    profile.consistency_score = std::max(100.0 * (1.0 - cv), 0.0);
    
    // Reliability score
    profile.reliability_score = (client.on_time_percentage * 0.7) + (profile.consistency_score * 0.3);
    
    // Risk level
    profile.risk_level = 100.0 - profile.reliability_score;
    
    // Classification based on payment days
    if (client.avg_payment_days <= 10) {
        profile.classification = "A";
    } else if (client.avg_payment_days <= 25) {
        profile.classification = "B";
    } else if (client.avg_payment_days <= 35) {
        profile.classification = "C";
    } else {
        profile.classification = "D";
    }
    
    // Confidence based on transaction count
    profile.confidence = std::min(static_cast<double>(client.transaction_count) * 2.0, 100.0);
    
    return profile;
}

double StatisticalPredictionEngine::PredictPaymentProbabilityStats(
    const ClientStatisticalProfile& profile,
    int daysWindow) {
    
    // Z-score calculation
    double z_score = static_cast<double>(daysWindow - profile.mean_payment_days) / 
                    std::max(profile.stdev_payment_days, 0.1);
    
    // Normal CDF approximation
    double probability = 0.5 * (1.0 + std::tanh(0.7978845608 * (z_score + 0.2316419 * 
                                    (1.0 / (1.0 + 0.2316419 * std::abs(z_score))))));
    
    // Adjust based on on-time percentage
    probability *= (profile.on_time_percentage / 100.0);
    
    return std::min(probability, 1.0);
}

double StatisticalPredictionEngine::CalculateAmountPaymentLikelihood(
    double outstandingAmount,
    double targetAmount,
    const ClientStatisticalProfile& profile) {
    
    // Amount ratio
    double amountRatio = std::min(targetAmount / std::max(outstandingAmount, 1.0), 1.0);
    
    // Combined with reliability
    return amountRatio * (profile.reliability_score / 100.0);
}

// ============================================================================
// STAGE 3: ML PREDICTION ENGINE
// ============================================================================

MLPredictionEngine::MLPredictionEngine() {
    neural_net = std::make_shared<NeuralNetworkPredictor>();
    random_forest = std::make_shared<RandomForestPredictor>(10, 8);
}

void MLPredictionEngine::TrainModels(const std::vector<ClientPaymentHistory>& trainingData) {
    // Extract features
    std::vector<std::vector<double>> features;
    std::vector<double> labels;
    
    for (const auto& client : trainingData) {
        std::vector<double> feat;
        feat.push_back(client.avg_payment_days / 30.0);
        feat.push_back(client.stdev_payment_days / 20.0);
        feat.push_back(client.on_time_percentage / 100.0);
        feat.push_back(std::log(client.outstanding_amount / 10000.0 + 1.0));
        feat.push_back(static_cast<double>(client.transaction_count) / 20.0);
        
        features.push_back(feat);
        labels.push_back(client.on_time_percentage / 100.0);
    }
    
    neural_net->Train(features, labels);
    random_forest->Train(features, labels);
}

MLPredictionResult MLPredictionEngine::PredictMLEnsemble(
    const ClientPaymentHistory& client,
    double targetAmount,
    int daysWindow) {
    
    MLPredictionResult result;
    
    // Prepare features
    std::vector<double> features;
    features.push_back(client.avg_payment_days / 30.0);
    features.push_back(client.stdev_payment_days / 20.0);
    features.push_back(client.on_time_percentage / 100.0);
    features.push_back(std::log(targetAmount / 10000.0 + 1.0));
    features.push_back(static_cast<double>(daysWindow) / 30.0);
    
    // Get predictions
    result.neural_net_prediction = neural_net->Predict(features);
    result.random_forest_prediction = random_forest->Predict(features);
    
    // Ensemble
    result.base_probability = (result.neural_net_prediction * 0.5 + 
                              result.random_forest_prediction * 0.5);
    
    result.confidence = 85.0;  // ML models generally high confidence
    result.uncertainty = 15.0;
    
    result.ml_insights = "ML ensemble combining neural network and random forest models";
    
    return result;
}

// ============================================================================
// STAGE 4: DECISION TREE PREDICTION ENGINE
// ============================================================================

DecisionTreePredictionEngine::DecisionTreePredictionEngine() {
    payment_dt = std::make_shared<PaymentProbabilityDecisionTree>();
    cashflow_dt = std::make_shared<CashflowDecisionTree>();
}

void DecisionTreePredictionEngine::TrainDecisionTrees(
    const std::vector<ClientPaymentHistory>& trainingData) {
    
    payment_dt->Train(trainingData);
}

DecisionTreePredictionResult DecisionTreePredictionEngine::PredictDecisionTree(
    const ClientPaymentHistory& client,
    double targetAmount,
    int daysWindow) {
    
    DecisionTreePredictionResult result;
    
    auto paymentProb = payment_dt->PredictPaymentProbability(client, targetAmount, daysWindow);
    
    result.probability = paymentProb.probability_percent / 100.0;
    result.reasoning = paymentProb.reasoning;
    
    // Generate decision path explanation
    if (client.avg_payment_days <= 10) {
        result.decision_path += "[Quick Payer Classification] ";
    } else if (client.avg_payment_days <= 25) {
        result.decision_path += "[Normal Payer Classification] ";
    } else {
        result.decision_path += "[Slow Payer Classification] ";
    }
    
    if (daysWindow >= client.avg_payment_days) {
        result.decision_path += "→ [Within typical payment window] ";
    } else {
        result.decision_path += "→ [Earlier than typical] ";
    }
    
    if (client.on_time_percentage >= 90) {
        result.decision_path += "→ [High consistency] = HIGH PROBABILITY";
    } else if (client.on_time_percentage >= 70) {
        result.decision_path += "→ [Moderate consistency] = MODERATE PROBABILITY";
    } else {
        result.decision_path += "→ [Low consistency] = LOW PROBABILITY";
    }
    
    return result;
}

std::vector<std::string> DecisionTreePredictionEngine::ExtractDecisionRules() {
    std::vector<std::string> rules;
    rules.push_back("IF payment_days <= 10 THEN classification = A (high probability)");
    rules.push_back("IF payment_days > 10 AND <= 25 THEN classification = B (moderate probability)");
    rules.push_back("IF payment_days > 25 THEN classification = C/D (low probability)");
    rules.push_back("IF on_time_percentage >= 90 THEN confidence = HIGH");
    rules.push_back("IF days_window >= avg_payment_days THEN probability increases");
    return rules;
}

// ============================================================================
// UNIFIED 4-STAGE PREDICTOR
// ============================================================================

UnifiedFourStagePredictor::UnifiedFourStagePredictor()
    : weight_global(0.15), weight_stats(0.25), 
      weight_ml(0.35), weight_decision_tree(0.25) {}

void UnifiedFourStagePredictor::TrainAllStages(
    const std::vector<ClientPaymentHistory>& trainingData,
    const GlobalPortfolioModel& portfolio) {
    
    current_portfolio = portfolio;
    
    // Stage 2: Stats already initialized
    // Stage 3: Train ML models
    ml_engine.TrainModels(trainingData);
    
    // Stage 4: Train decision trees
    dt_engine.TrainDecisionTrees(trainingData);
}

ComprehensivePaymentAnalysis UnifiedFourStagePredictor::PredictPaymentProbability(
    const ClientPaymentHistory& client,
    double targetAmount,
    int daysWindow) {
    
    ComprehensivePaymentAnalysis analysis;
    
    analysis.client_name = client.client_name;
    analysis.query_amount = targetAmount;
    analysis.query_days = daysWindow;
    
    // ===== STAGE 1: GLOBAL MODEL =====
    double globalAdjustment = current_portfolio.portfolio_payment_quality * 
                             GlobalPortfolioAnalyzer().GetSeasonalAdjustment(2);  // Feb
    analysis.global_model_prediction = globalAdjustment;
    
    // ===== STAGE 2: STATS-BASED =====
    auto statsProfile = stats_engine.AnalyzeClient(client);
    double statsProb = stats_engine.PredictPaymentProbabilityStats(statsProfile, daysWindow);
    double amountAdjustment = stats_engine.CalculateAmountPaymentLikelihood(
        client.outstanding_amount, targetAmount, statsProfile);
    analysis.stats_based_prediction = statsProb * amountAdjustment;
    
    analysis.contributing_factors.push_back("Classification: " + statsProfile.classification);
    analysis.contributing_factors.push_back("On-time rate: " + std::to_string(static_cast<int>(statsProfile.on_time_percentage)) + "%");
    analysis.contributing_factors.push_back("Consistency: " + std::to_string(static_cast<int>(statsProfile.consistency_score)) + "/100");
    
    if (statsProfile.risk_level > 50) {
        analysis.risk_factors.push_back("Higher variability in payment timing");
    }
    
    // ===== STAGE 3: ML MODEL =====
    auto mlResult = ml_engine.PredictMLEnsemble(client, targetAmount, daysWindow);
    analysis.ml_ensemble_prediction = mlResult.base_probability;
    
    analysis.contributing_factors.push_back("ML confidence: " + std::to_string(static_cast<int>(mlResult.confidence)) + "%");
    
    // ===== STAGE 4: DECISION TREE =====
    auto dtResult = dt_engine.PredictDecisionTree(client, targetAmount, daysWindow);
    analysis.decision_tree_prediction = dtResult.probability;
    analysis.decision_tree_rules = dt_engine.ExtractDecisionRules();
    
    // ===== FINAL ENSEMBLE =====
    analysis.final_probability = EnsembleStageResults(
        analysis.global_model_prediction,
        analysis.stats_based_prediction,
        analysis.ml_ensemble_prediction,
        analysis.decision_tree_prediction);
    
    // Count model agreement
    analysis.model_agreement = 0;
    double avgPrediction = analysis.final_probability;
    if (std::abs(analysis.global_model_prediction - avgPrediction) < 0.1) analysis.model_agreement++;
    if (std::abs(analysis.stats_based_prediction - avgPrediction) < 0.1) analysis.model_agreement++;
    if (std::abs(analysis.ml_ensemble_prediction - avgPrediction) < 0.1) analysis.model_agreement++;
    if (std::abs(analysis.decision_tree_prediction - avgPrediction) < 0.1) analysis.model_agreement++;
    
    analysis.prediction_reliable = analysis.model_agreement >= 3;
    
    // Generate reasoning
    std::stringstream ss;
    ss << "Stage 1 (Global): " << (analysis.global_model_prediction * 100) << "% | ";
    ss << "Stage 2 (Stats): " << (analysis.stats_based_prediction * 100) << "% | ";
    ss << "Stage 3 (ML): " << (analysis.ml_ensemble_prediction * 100) << "% | ";
    ss << "Stage 4 (Decision Tree): " << (analysis.decision_tree_prediction * 100) << "%";
    analysis.primary_reasoning = ss.str();
    
    // Confidence calculation
    analysis.confidence_score = 70.0 + (analysis.model_agreement * 7.5);
    
    return analysis;
}

std::vector<QuarterlyPaymentPrediction> UnifiedFourStagePredictor::GenerateQuarterlyForecast(
    const ClientPaymentHistory& client,
    double targetAmount) {
    
    std::vector<QuarterlyPaymentPrediction> forecast;
    
    std::vector<int> queryDays = {5, 6, 10};
    
    for (int days : queryDays) {
        auto analysis = PredictPaymentProbability(client, targetAmount, days);
        
        QuarterlyPaymentPrediction pred;
        pred.days_from_now = days;
        pred.probability_percent = analysis.final_probability * 100.0;
        pred.confidence = analysis.confidence_score;
        pred.likelihood_category = LikelihoodCategory(analysis.final_probability);
        
        forecast.push_back(pred);
    }
    
    return forecast;
}

std::string UnifiedFourStagePredictor::GetModelExplanation(
    const ComprehensivePaymentAnalysis& analysis) {
    
    std::stringstream ss;
    ss << "COMPREHENSIVE 4-STAGE ANALYSIS\n";
    ss << "==============================\n";
    ss << "Query: Will " << analysis.client_name << " pay $" << analysis.query_amount 
       << " in " << analysis.query_days << " days?\n";
    ss << "Answer: " << static_cast<int>(analysis.final_probability * 100) << "%\n\n";
    
    ss << "Stage Breakdown:\n";
    ss << "1. Global Model: " << static_cast<int>(analysis.global_model_prediction * 100) << "%\n";
    ss << "2. Statistical: " << static_cast<int>(analysis.stats_based_prediction * 100) << "%\n";
    ss << "3. ML Ensemble: " << static_cast<int>(analysis.ml_ensemble_prediction * 100) << "%\n";
    ss << "4. Decision Tree: " << static_cast<int>(analysis.decision_tree_prediction * 100) << "%\n\n";
    
    ss << "Confidence: " << static_cast<int>(analysis.confidence_score) << "%\n";
    ss << "Reliability: " << (analysis.prediction_reliable ? "HIGH" : "MODERATE") << "\n";
    ss << "Model Agreement: " << analysis.model_agreement << "/4 stages\n\n";
    
    ss << "Contributing Factors:\n";
    for (const auto& factor : analysis.contributing_factors) {
        ss << "  • " << factor << "\n";
    }
    
    if (!analysis.risk_factors.empty()) {
        ss << "\nRisk Factors:\n";
        for (const auto& risk : analysis.risk_factors) {
            ss << "  ⚠ " << risk << "\n";
        }
    }
    
    return ss.str();
}

double UnifiedFourStagePredictor::EnsembleStageResults(
    double globalPred,
    double statsPred,
    double mlPred,
    double treePred) {
    
    return (weight_global * globalPred) +
           (weight_stats * statsPred) +
           (weight_ml * mlPred) +
           (weight_decision_tree * treePred);
}

std::string UnifiedFourStagePredictor::LikelihoodCategory(double probability) {
    if (probability >= 0.85) return "Very High";
    if (probability >= 0.65) return "High";
    if (probability >= 0.45) return "Moderate";
    if (probability >= 0.25) return "Low";
    return "Very Low";
}

} // namespace PBS::ML
