#pragma once

#include "payment_analyzer.h"
#include "expense_analyzer.h"
#include "cashflow_predictor.h"
#include "client_categorizer.h"
#include "payment_analysis_engine.h"
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * ProcessingService - Handles data analysis and computation requests
 * Note: gRPC integration deferred. Currently uses JSON over HTTP via Node.js proxy
 */
class ProcessingServiceImpl {
public:
    ProcessingServiceImpl();
    
    // Process payment analysis for a party/ledger
    std::string AnalyzePayments(const std::string& json_request);
    
    // Process expense analysis
    std::string AnalyzeExpenses(const std::string& json_request);
    
    // Generate payment forecast
    std::string PredictCashflow(const std::string& json_request);
    
    // Categorize clients based on payment behavior
    std::string CategorizeClients(const std::string& json_request);
    
private:
    std::unique_ptr<PaymentAnalyzer> payment_analyzer;
    std::unique_ptr<ExpenseAnalyzer> expense_analyzer;
    std::unique_ptr<CashflowPredictor> cashflow_predictor;
    std::unique_ptr<ClientCategorizer> client_categorizer;
    std::unique_ptr<PaymentAnalysisEngine> analysis_engine;
};
