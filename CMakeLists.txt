cmake_minimum_required(VERSION 3.14)
project(PBS_Finsight_CppPaymentAnalysis)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings for socket programming
if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

# Use FetchContent to get nlohmann/json
include(FetchContent)

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

FetchContent_MakeAvailable(json)

# ============================================================================
# UNIFIED C++ PAYMENT ANALYSIS SERVER
# ============================================================================
# Single executable that uses ALL analysis modules:
# - HTTP Server (communication with Django)
# - Payment Analysis Engine (core analysis logic)
# - Payment Analyzer (transaction analysis)
# - Cashflow Predictor (30/60/90 day forecasts)
# - Client Categorizer (A/B/C/D classification)
# - Expense Analyzer (expense detection)
# - Advanced Cashflow Engine (complex forecasting)
# 
# Data Flow:
# 1. Django sends POST request with client payment data
# 2. HTTP Server receives request
# 3. PaymentAnalysisEngine coordinates all modules
# 4. Services process and analyze the data
# 5. Comprehensive results returned to Django
# ============================================================================

add_executable(cpp_payment_analyzer
        src/http_server.cpp
        src/payment_analysis_engine.cpp
        src/service_impl.cpp
        src/payment_analyzer.cpp
        src/cashflow_predictor.cpp
        src/client_categorizer.cpp
        src/expense_analyzer.cpp
        src/advanced_cashflow_engine.cpp
        src/ml_cashflow_predictor.cpp
        src/ml_integration_service.cpp
        src/decision_tree_predictor.cpp
        src/four_stage_predictor.cpp
)

target_include_directories(cpp_payment_analyzer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(cpp_payment_analyzer
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(cpp_payment_analyzer ws2_32 wsock32)
endif()

if(UNIX)
    target_link_libraries(cpp_payment_analyzer pthread)
endif()

# Installation
install(TARGETS cpp_payment_analyzer DESTINATION bin)
