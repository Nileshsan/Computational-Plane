#pragma once

#include "decision_tree_predictor.h"
#include "ml_predictor.h"
#include <vector>
#include <string>
#include <map>
#include <memory>

namespace PBS::ML {

// ============================================================================
// 4-STAGE ML CASHFLOW PREDICTION SYSTEM
// ============================================================================

/**
 * 4-STAGE ARCHITECTURE FOR ADVANCED ML CASHFLOW PREDICTION
 * 
 * STAGE 1: GLOBAL MODEL
 * - Historical patterns from entire portfolio
 * - Macro-level trends and seasonality
 * - Portfolio-wide statistics
 * 
 * STAGE 2: MAIN LOGIC SYSTEM (Stats-based)
 * - Individual client statistics (mean, stdev, percentiles)
 * - Payment consistency metrics
 * - Risk scoring based on historical behavior
 * 
 * STAGE 3: ML MODEL (Neural Network / Random Forest)
 * - Advanced pattern recognition
 * - Non-linear relationship detection
 * - Ensemble predictions combining multiple features
 * 
 * STAGE 4: DECISION TREE LAYER
 * - Interpretable rules and decision paths
 * - Explainable predictions with reasoning
 * - Specific amount + timeframe queries
 * - Final ensemble with other models
 */

// ============================================================================
// STAGE 1: GLOBAL MODEL
// ============================================================================

struct GlobalPortfolioModel {
    // Portfolio-wide statistics
    double portfolio_avg_payment_days;
    double portfolio_stdev_days;
    double portfolio_on_time_rate;
    double portfolio_payment_quality;  // 0-1 score
    
    // Seasonal patterns
    std::vector<double> monthly_collection_rates;  // 12 months
    
    // Trends
    double trend_direction;  // +1 improving, -1 declining
    double trend_strength;   // 0-1 confidence
    
    // Economic factors
    double economic_index;   // Industry health indicator
    double market_volatility;
};

class GlobalPortfolioAnalyzer {
public:
    GlobalPortfolioModel AnalyzePortfolio(
        const std::vector<ClientPaymentHistory>& allClients);
    
    double GetSeasonalAdjustment(int month);
    double GetEconomicAdjustment();
};

// ============================================================================
// STAGE 2: MAIN LOGIC SYSTEM (Statistics-based)
// ============================================================================

struct ClientStatisticalProfile {
    // Individual statistics
    double mean_payment_days;
    double stdev_payment_days;
    double percentile_90_days;
    double percentile_95_days;
    double on_time_percentage;
    
    // Risk scoring
    double consistency_score;    // 0-100
    double reliability_score;    // 0-100
    double risk_level;           // 0-100 (0=low risk, 100=high risk)
    
    // Classification
    std::string classification;  // A, B, C, D
    
    // Confidence
    double confidence;           // 0-100
};

class StatisticalPredictionEngine {
public:
    ClientStatisticalProfile AnalyzeClient(
        const ClientPaymentHistory& client);
    
    // Statistical probability prediction
    // "What's probability Client A pays 50k in 5 days?"
    double PredictPaymentProbabilityStats(
        const ClientStatisticalProfile& profile,
        int daysWindow);
    
    // Calculate payment likelihood for specific amount
    double CalculateAmountPaymentLikelihood(
        double outstandingAmount,
        double targetAmount,
        const ClientStatisticalProfile& profile);
};

// ============================================================================
// STAGE 3: ML MODEL (Neural Network / Random Forest)
// ============================================================================

struct MLPredictionResult {
    double base_probability;
    double confidence;
    double uncertainty;
    
    // Component predictions
    double neural_net_prediction;
    double random_forest_prediction;
    
    // Reasoning
    std::string ml_insights;
};

class MLPredictionEngine {
public:
    MLPredictionEngine();
    
    // Train ML models
    void TrainModels(const std::vector<ClientPaymentHistory>& trainingData);
    
    // Predict with ML ensemble
    MLPredictionResult PredictMLEnsemble(
        const ClientPaymentHistory& client,
        double targetAmount,
        int daysWindow);
    
private:
    std::shared_ptr<NeuralNetworkPredictor> neural_net;
    std::shared_ptr<RandomForestPredictor> random_forest;
};

// ============================================================================
// STAGE 4: DECISION TREE LAYER
// ============================================================================

struct DecisionTreePredictionResult {
    double probability;
    std::string decision_path;     // Human-readable path taken
    std::string reasoning;         // Why this prediction
    std::vector<std::string> rules; // Applicable decision rules
};

class DecisionTreePredictionEngine {
public:
    DecisionTreePredictionEngine();
    
    // Train decision trees
    void TrainDecisionTrees(const std::vector<ClientPaymentHistory>& trainingData);
    
    // Predict with decision tree
    DecisionTreePredictionResult PredictDecisionTree(
        const ClientPaymentHistory& client,
        double targetAmount,
        int daysWindow);
    
    // Get decision rules for transparency
    std::vector<std::string> ExtractDecisionRules();
    
private:
    std::shared_ptr<PaymentProbabilityDecisionTree> payment_dt;
    std::shared_ptr<CashflowDecisionTree> cashflow_dt;
};

// ============================================================================
// UNIFIED 4-STAGE PREDICTOR
// ============================================================================

struct QuarterlyPaymentPrediction {
    int days_from_now;
    double probability_percent;
    double confidence;
    std::string likelihood_category;  // "Very Low", "Low", "Moderate", "High", "Very High"
};

struct ComprehensivePaymentAnalysis {
    // Original query
    std::string client_name;
    double query_amount;
    int query_days;
    
    // Stage-by-stage predictions
    double global_model_prediction;
    double stats_based_prediction;
    double ml_ensemble_prediction;
    double decision_tree_prediction;
    
    // Final ensemble result
    double final_probability;
    double confidence_score;
    
    // Breakdown
    std::vector<QuarterlyPaymentPrediction> prediction_timeline;
    
    // Explainability
    std::string primary_reasoning;
    std::vector<std::string> contributing_factors;
    std::vector<std::string> risk_factors;
    std::vector<std::string> decision_tree_rules;
    
    // Metadata
    int model_agreement;  // 0-4 stages agree
    bool prediction_reliable;
};

class UnifiedFourStagePredictor {
public:
    UnifiedFourStagePredictor();
    
    // One-time training on historical data
    void TrainAllStages(const std::vector<ClientPaymentHistory>& trainingData,
                       const GlobalPortfolioModel& portfolio);
    
    // The main query: "What are chances Client A pays 50k in 5 days?"
    // Returns: "35%"
    ComprehensivePaymentAnalysis PredictPaymentProbability(
        const ClientPaymentHistory& client,
        double targetAmount,
        int daysWindow);
    
    // Generate quarterly forecast: 5, 6, 10 days predictions
    std::vector<QuarterlyPaymentPrediction> GenerateQuarterlyForecast(
        const ClientPaymentHistory& client,
        double targetAmount);
    
    // Get explainability metrics
    std::string GetModelExplanation(const ComprehensivePaymentAnalysis& analysis);
    
private:
    GlobalPortfolioAnalyzer global_analyzer;
    StatisticalPredictionEngine stats_engine;
    MLPredictionEngine ml_engine;
    DecisionTreePredictionEngine dt_engine;
    
    GlobalPortfolioModel current_portfolio;
    
    // Stage weighting (adjustable)
    double weight_global;      // 0.15 (15%)
    double weight_stats;       // 0.25 (25%)
    double weight_ml;          // 0.35 (35%)
    double weight_decision_tree; // 0.25 (25%)
    
    double EnsembleStageResults(
        double globalPred,
        double statsPred,
        double mlPred,
        double treePred);
    
    std::string LikelihoodCategory(double probability);
};

} // namespace PBS::ML
