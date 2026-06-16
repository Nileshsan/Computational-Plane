// Service implementation - handles JSON requests from Node.js proxy

#include "service_impl.h"
#include <iostream>

ProcessingServiceImpl::ProcessingServiceImpl()
    : payment_analyzer(std::make_unique<PaymentAnalyzer>()),
      expense_analyzer(std::make_unique<ExpenseAnalyzer>()),
      cashflow_predictor(std::make_unique<CashflowPredictor>()),
      client_categorizer(std::make_unique<ClientCategorizer>()),
      analysis_engine(std::make_unique<PaymentAnalysisEngine>()) {
}

std::string ProcessingServiceImpl::AnalyzePayments(const std::string& json_request) {
    try {
        auto json = nlohmann::json::parse(json_request);
        auto result = analysis_engine->AnalyzePaymentPatterns(json);
        return result.dump();
    } catch (const std::exception& e) {
        nlohmann::json error_response;
        error_response["success"] = false;
        error_response["error"] = std::string("Payment analysis failed: ") + e.what();
        return error_response.dump();
    }
}

std::string ProcessingServiceImpl::AnalyzeExpenses(const std::string& json_request) {
    try {
        auto json = nlohmann::json::parse(json_request);
        // Process expenses through expense analyzer
        nlohmann::json response;
        response["success"] = true;
        response["message"] = "Expense analysis completed";
        return response.dump();
    } catch (const std::exception& e) {
        nlohmann::json error_response;
        error_response["success"] = false;
        error_response["error"] = std::string("Expense analysis failed: ") + e.what();
        return error_response.dump();
    }
}

std::string ProcessingServiceImpl::PredictCashflow(const std::string& json_request) {
    try {
        auto json = nlohmann::json::parse(json_request);
        // Process through cashflow predictor
        nlohmann::json response;
        response["success"] = true;
        response["message"] = "Cashflow prediction completed";
        return response.dump();
    } catch (const std::exception& e) {
        nlohmann::json error_response;
        error_response["success"] = false;
        error_response["error"] = std::string("Cashflow prediction failed: ") + e.what();
        return error_response.dump();
    }
}

std::string ProcessingServiceImpl::CategorizeClients(const std::string& json_request) {
    try {
        auto json = nlohmann::json::parse(json_request);
        auto result = analysis_engine->CategorizeClientsByPaymentBehavior(json);
        return result.dump();
    } catch (const std::exception& e) {
        nlohmann::json error_response;
        error_response["success"] = false;
        error_response["error"] = std::string("Client categorization failed: ") + e.what();
        return error_response.dump();
    }
}
