#pragma once

#include "ml_cashflow_predictor.h"
#include "decision_tree_predictor.h"
#include "four_stage_predictor.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

// ============================================================================
// Integration between PaymentAnalysisEngine and ML Cashflow Predictor
// 4-STAGE SYSTEM:
// - Stage 1: Global Portfolio Model
// - Stage 2: Statistical Analysis
// - Stage 3: ML Ensemble (Neural Network + Random Forest)
// - Stage 4: Decision Tree Layer (Interpretable predictions)
// ============================================================================

class MLIntegrationService {
public:
    MLIntegrationService();
    
    // Initialize ML models from client payment histories
    void initialize_from_analysis_data(const json& company_analysis_response);
    
    // Add ML predictions to analysis response
    json enrich_response_with_ml_predictions(
        const json& analysis_response,
        bool include_detailed_stages = true
    );
    
    // Specific prediction queries
    json predict_client_payment_scenario(
        const std::string& client_name,
        double amount,
        const std::vector<int>& day_thresholds
    );
    
    // Cashflow forecast with ML
    json forecast_with_ml(
        const std::string& client_name,
        double current_outstanding,
        const std::vector<double>& amounts,
        const std::vector<int>& days
    );
    
    // Get detailed model information
    json get_model_insights() const;
    
private:
    MLCashflow::MLCashflowPredictor predictor;
    bool is_initialized = false;
    
    // Helper to extract training data from analysis response
    void extract_and_train_models(const json& client_analysis);
};
