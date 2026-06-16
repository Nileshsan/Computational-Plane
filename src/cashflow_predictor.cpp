#include "cashflow_predictor.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>

CashflowPredictor::CashflowPredictor() {}

AnalysisResult CashflowPredictor::predict_cashflow(
    double current_balance,
    const std::vector<PaymentPattern>& patterns,
    const std::vector<FixedExpense>& expenses,
    int days_ahead
) {
    AnalysisResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        predictions.clear();
        
        double running_balance = current_balance;
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        
        json predictions_array = json::array();
        double total_confidence = 0.0;
        
        for (int i = 1; i <= days_ahead; ++i) {
            auto future_time = time_t_now + (i * 86400);
            
            CashflowPrediction pred;
            
            // Format date
            std::ostringstream oss;
            oss << std::put_time(std::gmtime(&future_time), "%Y-%m-%d");
            pred.date = oss.str();
            
            // Calculate receipts for this date
            pred.receipts = calculate_receipts_for_date(pred.date, patterns);
            
            // Calculate expenses for this date
            pred.expenses = calculate_expenses_for_date(pred.date, expenses);
            
            // Update running balance
            running_balance += (pred.receipts - pred.expenses);
            pred.predicted_balance = running_balance;
            
            // Apply confidence intervals
            apply_confidence_intervals(pred, patterns, expenses);
            
            total_confidence += pred.confidence;
            
            predictions.push_back(pred);
            predictions_array.push_back(pred.to_json());
        }
        
        double avg_confidence = days_ahead > 0 ? total_confidence / days_ahead : 0.5;
        
        result.success = true;
        result.data = {
            {"predictions", predictions_array},
            {"total_days", days_ahead},
            {"final_predicted_balance", running_balance},
            {"avg_confidence", avg_confidence}
        };
        result.confidence_score = avg_confidence;
        result.sample_size = days_ahead;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Cashflow prediction failed: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    return result;
}

std::vector<CashflowPrediction> CashflowPredictor::get_predictions() const {
    return predictions;
}

double CashflowPredictor::calculate_receipts_for_date(
    const std::string& date,
    const std::vector<PaymentPattern>& patterns
) {
    double total = 0.0;
    
    for (const auto& pattern : patterns) {
        // Simplified: check if this date aligns with expected payment date
        // In real scenario, would calculate based on avg_payment_days from pattern
        if (!pattern.expected_payment_date.empty() && pattern.expected_payment_date == date) {
            total += 1000.0;  // Placeholder amount
        }
    }
    
    return total;
}

double CashflowPredictor::calculate_expenses_for_date(
    const std::string& date,
    const std::vector<FixedExpense>& expenses
) {
    double total = 0.0;
    
    for (const auto& expense : expenses) {
        if (!expense.next_date.empty() && expense.next_date == date && expense.is_active) {
            total += expense.amount;
        }
    }
    
    return total;
}

void CashflowPredictor::apply_confidence_intervals(
    CashflowPrediction& prediction,
    const std::vector<PaymentPattern>& patterns,
    const std::vector<FixedExpense>& expenses
) {
    // Calculate min/max confidence bands
    double uncertainty = 0.0;
    
    // Add uncertainty from payment pattern variations
    for (const auto& pattern : patterns) {
        uncertainty += (pattern.delay_std_deviation / (pattern.avg_payment_days + 1)) * 0.5;
    }
    
    // Add uncertainty from expense variations
    for (const auto& expense : expenses) {
        uncertainty += (expense.interval_std_deviation / (expense.interval_days + 1)) * 0.3;
    }
    
    double band = std::abs(prediction.predicted_balance * 0.1 * uncertainty);
    
    prediction.min_balance = prediction.predicted_balance - band;
    prediction.max_balance = prediction.predicted_balance + band;
    
    // Confidence decreases with higher uncertainty
    prediction.confidence = std::max(0.3, 1.0 - uncertainty);
}
