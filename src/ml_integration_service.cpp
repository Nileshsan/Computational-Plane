#include "ml_integration_service.h"
#include <sstream>
#include <iomanip>

MLIntegrationService::MLIntegrationService() : is_initialized(false) {
}

void MLIntegrationService::initialize_from_analysis_data(const json& company_analysis_response) {
    try {
        // Extract company-wide metrics for global model
        std::vector<json> client_histories;
        
        if (company_analysis_response.contains("client_analysis") && 
            company_analysis_response["client_analysis"].is_object()) {
            
            const auto& client_analysis = company_analysis_response["client_analysis"];
            
            // Prepare client histories for global model
            for (const auto& [client_name, client_data] : client_analysis.items()) {
                json history;
                
                if (client_data.contains("payment_metrics")) {
                    const auto& metrics = client_data["payment_metrics"];
                    if (metrics.contains("payment_days") && metrics["payment_days"].is_array()) {
                        history["payment_days"] = metrics["payment_days"];
                    }
                }
                
                if (client_data.contains("performance")) {
                    const auto& perf = client_data["performance"];
                    if (perf.contains("on_time_count")) {
                        history["on_time_count"] = perf["on_time_count"];
                    }
                }
                
                if (!history.empty()) {
                    client_histories.push_back(history);
                }
            }
            
            // Initialize global model
            predictor.initialize_global_model(client_histories);
            
            // Train individual client models
            for (const auto& [client_name, client_data] : client_analysis.items()) {
                std::vector<double> payment_days;
                std::vector<double> payment_amounts;
                double on_time_percentage = 0.0;
                
                if (client_data.contains("payment_metrics")) {
                    const auto& metrics = client_data["payment_metrics"];
                    
                    if (metrics.contains("payment_days") && metrics["payment_days"].is_array()) {
                        for (const auto& day : metrics["payment_days"]) {
                            payment_days.push_back(day.get<double>());
                        }
                    }
                }
                
                if (client_data.contains("performance")) {
                    const auto& perf = client_data["performance"];
                    if (perf.contains("on_time_percentage")) {
                        on_time_percentage = perf["on_time_percentage"].get<double>();
                    }
                }
                
                // Use default amounts if not available
                if (payment_amounts.empty() && !payment_days.empty()) {
                    payment_amounts.assign(payment_days.size(), 1000.0);
                }
                
                if (!payment_days.empty()) {
                    predictor.train_client_model(
                        client_name,
                        payment_days,
                        payment_amounts,
                        on_time_percentage
                    );
                }
            }
            
            is_initialized = true;
        }
    } catch (const std::exception& e) {
        is_initialized = false;
    }
}

json MLIntegrationService::enrich_response_with_ml_predictions(
    const json& analysis_response,
    bool include_detailed_stages
) {
    json enriched_response = analysis_response;
    
    if (!is_initialized) {
        return enriched_response;
    }
    
    try {
        json ml_predictions = json::object();
        
        if (enriched_response.contains("client_analysis") && 
            enriched_response["client_analysis"].is_object()) {
            
            const auto& client_analysis = enriched_response["client_analysis"];
            json ml_section = json::object();
            
            for (const auto& [client_name, client_data] : client_analysis.items()) {
                // Get outstanding amount
                double outstanding = 0.0;
                if (client_data.contains("financial")) {
                    if (client_data["financial"].contains("outstanding_amount")) {
                        outstanding = client_data["financial"]["outstanding_amount"].get<double>();
                    }
                }
                
                // Predict multiple scenarios
                std::vector<int> day_thresholds = {3, 5, 6, 7, 10, 15, 30};
                std::vector<double> amounts = {outstanding > 0 ? outstanding : 50000.0};
                
                json client_predictions;
                
                if (include_detailed_stages) {
                    // Get detailed prediction with 3 stages
                    double amount = amounts[0];
                    client_predictions = predictor.get_prediction_with_stages(
                        client_name,
                        amount,
                        day_thresholds[0]
                    );
                } else {
                    // Get quick predictions
                    auto predictions = predictor.predict_multiple_scenarios(
                        client_name,
                        amounts[0],
                        std::vector<double>(day_thresholds.begin(), day_thresholds.end())
                    );
                    
                    json pred_array = json::array();
                    for (const auto& pred : predictions) {
                        pred_array.push_back(pred.to_json());
                    }
                    client_predictions["predictions"] = pred_array;
                }
                
                ml_section[client_name] = client_predictions;
            }
            
            enriched_response["ml_cashflow_predictions"] = ml_section;
        }
        
        // Add ML model summary
        enriched_response["ml_model_info"] = predictor.get_model_summary();
        
    } catch (const std::exception& e) {
        enriched_response["ml_error"] = std::string(e.what());
    }
    
    return enriched_response;
}

json MLIntegrationService::predict_client_payment_scenario(
    const std::string& client_name,
    double amount,
    const std::vector<int>& day_thresholds
) {
    if (!is_initialized) {
        return json{{"error", "ML model not initialized"}};
    }
    
    std::vector<double> days_as_double(day_thresholds.begin(), day_thresholds.end());
    auto predictions = predictor.predict_multiple_scenarios(
        client_name,
        amount,
        days_as_double
    );
    
    json result = json::object();
    result["client_name"] = client_name;
    result["amount"] = amount;
    
    json scenarios = json::array();
    for (const auto& pred : predictions) {
        scenarios.push_back(pred.to_json());
    }
    result["payment_probability_by_day"] = scenarios;
    
    return result;
}

json MLIntegrationService::forecast_with_ml(
    const std::string& client_name,
    double current_outstanding,
    const std::vector<double>& amounts,
    const std::vector<int>& days
) {
    if (!is_initialized) {
        return json{{"error", "ML model not initialized"}};
    }
    
    return predictor.forecast_cashflow_ml(
        client_name,
        current_outstanding,
        amounts,
        days
    );
}

json MLIntegrationService::get_model_insights() const {
    if (!is_initialized) {
        return json{{"error", "ML model not initialized"}};
    }
    
    json insights = json::object();
    insights["model_summary"] = predictor.get_model_summary();
    insights["is_initialized"] = is_initialized;
    insights["architecture"] = {
        {"stage_1", "Global Model - Company-wide payment patterns"},
        {"stage_2", "Statistical Model - Client-specific statistics"},
        {"stage_3", "ML Model - Machine learning adjustments"},
        {"ensemble_method", "Weighted combination: 20% global + 40% statistical + 40% ML"}
    };
    
    return insights;
}
