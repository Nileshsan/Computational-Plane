#pragma once

#include "data_structures.h"
#include "payment_analyzer.h"
#include "expense_analyzer.h"
#include <vector>
#include <memory>

/**
 * Predicts cashflow for the next N days.
 * Combines:
 * - Payment pattern predictions
 * - Fixed expense schedules
 * - Current bank balance
 * - Min/max confidence ranges
 */
class CashflowPredictor {
public:
    CashflowPredictor();
    
    /**
     * Predict cashflow for N days ahead
     */
    AnalysisResult predict_cashflow(
        double current_balance,
        const std::vector<PaymentPattern>& patterns,
        const std::vector<FixedExpense>& expenses,
        int days_ahead = 30
    );
    
    /**
     * Get list of predicted cashflow items
     */
    std::vector<CashflowPrediction> get_predictions() const;
    
private:
    std::vector<CashflowPrediction> predictions;
    
    /**
     * Calculate receipts expected on a given date
     */
    double calculate_receipts_for_date(
        const std::string& date,
        const std::vector<PaymentPattern>& patterns
    );
    
    /**
     * Calculate expenses expected on a given date
     */
    double calculate_expenses_for_date(
        const std::string& date,
        const std::vector<FixedExpense>& expenses
    );
    
    /**
     * Calculate confidence interval based on pattern consistency
     */
    void apply_confidence_intervals(
        CashflowPrediction& prediction,
        const std::vector<PaymentPattern>& patterns,
        const std::vector<FixedExpense>& expenses
    );
};
