#include "advanced_cashflow_engine.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>

// ============================================================================
// Utility Functions
// ============================================================================

long AdvancedCashflowEngine::DateToDays(const std::string& date_str) {
    // Parse YYYY-MM-DD format: "2026-02-03"
    int year, month, day;
    if (sscanf(date_str.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return 0;
    }
    
    // Days since epoch (1970-01-01)
    int days = 0;
    for (int y = 1970; y < year; ++y) {
        days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    }
    
    int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
        days_in_month[2] = 29;
    }
    
    for (int m = 1; m < month; ++m) {
        days += days_in_month[m];
    }
    days += day;
    
    return days;
}

int AdvancedCashflowEngine::DaysBetween(const std::string& from_date, const std::string& to_date) {
    return static_cast<int>(DateToDays(to_date) - DateToDays(from_date));
}

float AdvancedCashflowEngine::CalculateMean(const std::vector<float>& values) {
    if (values.empty()) return 0.0f;
    return std::accumulate(values.begin(), values.end(), 0.0f) / values.size();
}

float AdvancedCashflowEngine::CalculateStdDev(const std::vector<float>& values, float mean) {
    if (values.size() < 2) return 0.0f;
    
    float sum_sq_diff = 0.0f;
    for (float v : values) {
        float diff = v - mean;
        sum_sq_diff += diff * diff;
    }
    
    return std::sqrt(sum_sq_diff / (values.size() - 1));
}

char AdvancedCashflowEngine::CategorizePaymentBehavior(float avg_payment_days) {
    if (avg_payment_days <= CATEGORY_A_THRESHOLD) return 'A';
    if (avg_payment_days <= CATEGORY_B_THRESHOLD) return 'B';
    return 'C';
}

std::pair<char, float> AdvancedCashflowEngine::GetCategoryAndDiscount(float avg_payment_days) {
    char category = CategorizePaymentBehavior(avg_payment_days);
    float discount = 0.0f;
    
    switch (category) {
        case 'A': discount = CATEGORY_A_DISCOUNT; break;
        case 'B': discount = CATEGORY_B_DISCOUNT; break;
        default: discount = CATEGORY_C_DISCOUNT;
    }
    
    return {category, discount};
}

float AdvancedCashflowEngine::CalculateConfidenceScore(int sample_size, float std_dev, float mean) {
    if (mean <= 0.0f || sample_size < 2) return 0.5f;
    return 1.0f - std::min(std_dev / mean, 1.0f);
}

std::map<std::string, std::vector<json>> AdvancedCashflowEngine::GroupTransactionsByParty(
    const std::vector<json>& transactions) {
    
    std::map<std::string, std::vector<json>> groups;
    for (const auto& txn : transactions) {
        if (txn.contains("party_name")) {
            std::string party = txn["party_name"].get<std::string>();
            groups[party].push_back(txn);
        }
    }
    return groups;
}

std::vector<AdvancedCashflowEngine::PaymentMatch> AdvancedCashflowEngine::MatchSalesToReceipts(
    const std::vector<json>& sales,
    const std::vector<json>& receipts) {
    
    std::vector<PaymentMatch> matches;
    
    if (sales.empty() || receipts.empty()) {
        return matches;
    }
    
    // Copy for modification
    auto receipts_copy = receipts;
    int receipt_index = 0;
    
    // FIFO matching: iterate through sales in date order
    for (const auto& sale : sales) {
        if (!sale.contains("date") || !sale.contains("amount")) continue;
        
        float sale_amount = sale["amount"].get<float>();
        std::string sale_date_str = sale["date"].get<std::string>();
        float remaining_amount = sale_amount;
        
        // Match this sale with receipts
        while (remaining_amount > 0.001f && receipt_index < static_cast<int>(receipts_copy.size())) {
            auto& receipt = receipts_copy[receipt_index];
            
            if (!receipt.contains("date") || !receipt.contains("amount")) {
                receipt_index++;
                continue;
            }
            
            float receipt_amount = receipt["amount"].get<float>();
            std::string receipt_date_str = receipt["date"].get<std::string>();
            
            int days_to_pay = DaysBetween(sale_date_str, receipt_date_str);
            
            if (receipt_amount <= remaining_amount + 0.001f) {
                // This receipt matches part or all of the sale
                if (days_to_pay >= 0) {
                    matches.push_back({static_cast<float>(days_to_pay), receipt_amount});
                }
                remaining_amount -= receipt_amount;
                receipt_index++;
            } else {
                // Partial match
                if (days_to_pay >= 0) {
                    matches.push_back({static_cast<float>(days_to_pay), remaining_amount});
                }
                receipt["amount"] = receipt_amount - remaining_amount;
                remaining_amount = 0.0f;
            }
        }
    }
    
    return matches;
}

std::map<std::string, json> AdvancedCashflowEngine::GetClosingBalances(
    const std::vector<json>& transactions) {
    
    std::map<std::string, float> sales_total;
    std::map<std::string, float> receipts_total;
    std::map<std::string, std::string> last_sale_date;
    
    // Sum by party and register type
    for (const auto& txn : transactions) {
        if (!txn.contains("party_name") || !txn.contains("amount")) continue;
        
        std::string party = txn["party_name"].get<std::string>();
        float amount = txn["amount"].get<float>();
        std::string register_type = txn.value("register_type", "");
        std::string date_str = txn.value("date", "");
        
        if (register_type == "sales") {
            sales_total[party] += amount;
            last_sale_date[party] = date_str;
        } else if (register_type == "receipt") {
            receipts_total[party] += amount;
        }
    }
    
    // Calculate remaining balances
    std::map<std::string, json> balances;
    for (const auto& [party, total_sales] : sales_total) {
        float received = receipts_total.count(party) > 0 ? receipts_total[party] : 0.0f;
        float balance = total_sales - received;
        
        if (balance > 0.001f) {
            balances[party] = json{
                {"balance", balance},
                {"last_sale_date", last_sale_date[party]}
            };
        }
    }
    
    return balances;
}

// ============================================================================
// STEP 1: Categorize Clients (A/B/C)
// ============================================================================

json AdvancedCashflowEngine::CategorizeClients(const json& transactions_json) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Parse input
        if (!transactions_json.contains("transactions")) {
            return json{
                {"status", "error"},
                {"message", "Missing 'transactions' in request"}
            };
        }
        
        auto transactions_arr = transactions_json["transactions"].get<std::vector<json>>();
        
        // Group by party
        auto party_groups = GroupTransactionsByParty(transactions_arr);
        
        json result = json{
            {"A", json::array()},
            {"B", json::array()},
            {"C", json::array()}
        };
        
        // Categorize each party
        for (const auto& [party_name, txns] : party_groups) {
            // Separate sales and receipts
            std::vector<json> sales, receipts;
            for (const auto& txn : txns) {
                std::string register_type = txn.value("register_type", "");
                if (register_type == "sales") sales.push_back(txn);
                else if (register_type == "receipt") receipts.push_back(txn);
            }
            
            if (sales.empty() || receipts.empty()) continue;
            
            // FIFO matching
            auto matches = MatchSalesToReceipts(sales, receipts);
            if (matches.empty()) continue;
            
            // Calculate statistics
            std::vector<float> payment_days;
            for (const auto& match : matches) {
                payment_days.push_back(match.days_to_pay);
            }
            
            float mean_days = CalculateMean(payment_days);
            float std_dev = CalculateStdDev(payment_days, mean_days);
            auto [category, discount] = GetCategoryAndDiscount(mean_days);
            float confidence = CalculateConfidenceScore(static_cast<int>(payment_days.size()), std_dev, mean_days);
            
            json party_result = json{
                {"party", party_name},
                {"avg_payment_days", std::round(mean_days * 100) / 100.0f},
                {"std_deviation", std::round(std_dev * 100) / 100.0f},
                {"category", std::string(1, category)},
                {"discount_percent", discount},
                {"sample_size", static_cast<int>(payment_days.size())},
                {"confidence_score", std::round(confidence * 100) / 100.0f}
            };
            
            result[std::string(1, category)].push_back(party_result);
        }
        
        // Sort each category by payment days
        for (auto& cat_key : {"A", "B", "C"}) {
            auto& category_arr = result[cat_key];
            std::sort(category_arr.begin(), category_arr.end(),
                [](const json& a, const json& b) {
                    return a["avg_payment_days"] < b["avg_payment_days"];
                });
        }
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
        
        return json{
            {"status", "success"},
            {"categories", result},
            {"total_parties", party_groups.size()},
            {"execution_time_ms", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()}
        };
        
    } catch (const std::exception& e) {
        return json{
            {"status", "error"},
            {"message", std::string("CategorizeClients failed: ") + e.what()}
        };
    }
}

// ============================================================================
// STEP 2: Detect Fixed Expenses
// ============================================================================

json AdvancedCashflowEngine::DetectFixedExpenses(const json& transactions_json) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        if (!transactions_json.contains("transactions")) {
            return json{
                {"status", "error"},
                {"message", "Missing 'transactions' in request"}
            };
        }
        
        auto transactions = transactions_json["transactions"].get<std::vector<json>>();
        
        // Filter for payment transactions only
        std::vector<json> payments;
        for (const auto& txn : transactions) {
            if (txn.value("register_type", "") == "payment") {
                payments.push_back(txn);
            }
        }
        
        // Group by (party, amount) -> dates
        std::map<std::pair<std::string, float>, std::vector<std::string>> expense_groups;
        for (const auto& payment : payments) {
            std::string party = payment.value("party_name", "");
            float amount = payment.value("amount", 0.0f);
            std::string date = payment.value("date", "");
            
            if (!party.empty() && amount > 0.001f && !date.empty()) {
                expense_groups[{party, amount}].push_back(date);
            }
        }
        
        // Analyze for recurring patterns
        json result = json::object();
        
        for (const auto& [key, dates] : expense_groups) {
            const auto& [party_name, amount] = key;
            
            if (static_cast<int>(dates.size()) < MIN_OCCURRENCES_FOR_RECURRING) continue;
            
            // Calculate intervals between dates
            std::vector<int> intervals;
            std::sort(const_cast<std::vector<std::string>&>(dates).begin(), 
                      const_cast<std::vector<std::string>&>(dates).end());
            
            for (size_t i = 1; i < dates.size(); ++i) {
                int interval = DaysBetween(dates[i-1], dates[i]);
                if (interval > 0) {
                    intervals.push_back(interval);
                }
            }
            
            if (intervals.empty()) continue;
            
            // Calculate statistics
            float mean_interval = CalculateMean(std::vector<float>(
                intervals.begin(), intervals.end()));
            float std_dev_interval = CalculateStdDev(
                std::vector<float>(intervals.begin(), intervals.end()),
                mean_interval);
            
            // Check if recurring (low std dev or monthly pattern)
            if (std_dev_interval <= DATE_TOLERANCE_DAYS || mean_interval <= 35.0f) {
                float confidence = 1.0f - std::min(std_dev_interval / mean_interval, 1.0f);
                
                // Calculate next due date
                std::string last_date = dates.back();
                int days_to_add = static_cast<int>(std::round(mean_interval));
                
                // Simple date addition
                long day_num = DateToDays(last_date) + days_to_add;
                
                // Convert back to date string
                int year = 1970;
                while (day_num > 365) {
                    int days_in_year = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
                    if (day_num <= days_in_year) break;
                    day_num -= days_in_year;
                    year++;
                }
                
                int month = 1, day = day_num;
                int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) days_in_month[2] = 29;
                
                for (int m = 1; m <= 12; ++m) {
                    if (day <= days_in_month[m]) {
                        month = m;
                        break;
                    }
                    day -= days_in_month[m];
                }
                
                char next_due_date[11];
                snprintf(next_due_date, sizeof(next_due_date), "%04d-%02d-%02d", year, month, day);
                
                std::string description = party_name + " (Recurring)";
                result[description] = json{
                    {"amount", amount},
                    {"interval_days", static_cast<int>(std::round(mean_interval))},
                    {"confidence", std::round(confidence * 100) / 100.0f},
                    {"next_due_date", std::string(next_due_date)},
                    {"std_deviation", std::round(std_dev_interval * 100) / 100.0f},
                    {"transaction_count", static_cast<int>(dates.size())},
                    {"party", party_name}
                };
            }
        }
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
        
        return json{
            {"status", "success"},
            {"expenses", result},
            {"total_recurring", result.size()},
            {"execution_time_ms", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()}
        };
        
    } catch (const std::exception& e) {
        return json{
            {"status", "error"},
            {"message", std::string("DetectFixedExpenses failed: ") + e.what()}
        };
    }
}

// ============================================================================
// STEP 3: Generate Discount Offers
// ============================================================================

json AdvancedCashflowEngine::GenerateDiscountOffers(const json& request) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        if (!request.contains("closing_balances") || !request.contains("client_categories")) {
            return json{
                {"status", "error"},
                {"message", "Missing 'closing_balances' or 'client_categories'"}
            };
        }
        
        auto closing_balances = request["closing_balances"].get<std::map<std::string, json>>();
        auto client_categories = request["client_categories"];
        
        json offers = json::array();
        
        for (const auto& [party, balance_data] : closing_balances) {
            float closing_balance = balance_data.value("balance", 0.0f);
            std::string last_sale_date = balance_data.value("last_sale_date", "");
            
            if (closing_balance <= 0.001f || last_sale_date.empty()) continue;
            
            // Find party in categories to get payment behavior
            float avg_payment_days = 30.0f;  // default
            for (const auto& cat : {"A", "B", "C"}) {
                for (const auto& party_info : client_categories[cat]) {
                    if (party_info.value("party", "") == party) {
                        avg_payment_days = party_info.value("avg_payment_days", 30.0f);
                        break;
                    }
                }
            }
            
            auto [category, base_discount] = GetCategoryAndDiscount(avg_payment_days);
            
            // Calculate expected due date
            long days_to_due = static_cast<long>(std::round(avg_payment_days));
            
            // Simple date addition (approximation)
            int days_remaining = static_cast<int>(days_to_due);
            
            // Calculate discount amount: Balance × Annual Rate × Days Remaining / 365
            float discount_amount = closing_balance * ANNUAL_INTEREST_RATE * days_remaining / 365.0f;
            
            if (days_remaining <= 0) {
                discount_amount = 0.0f;
            }
            
            offers.push_back(json{
                {"party", party},
                {"closing_balance", closing_balance},
                {"days_remaining", days_remaining},
                {"annual_interest_rate", ANNUAL_INTEREST_RATE},
                {"category", std::string(1, category)},
                {"discount_percent", base_discount},
                {"discount_amount", std::round(discount_amount * 100) / 100.0f},
                {"payment_behavior_days", avg_payment_days}
            });
        }
        
        // Sort by discount_amount (highest first)
        std::sort(offers.begin(), offers.end(),
            [](const json& a, const json& b) {
                return a["discount_amount"].get<float>() > b["discount_amount"].get<float>();
            });
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
        
        return json{
            {"status", "success"},
            {"offers", offers},
            {"total_offers", offers.size()},
            {"total_potential_discounts", 
             std::accumulate(offers.begin(), offers.end(), 0.0f,
                [](float sum, const json& offer) {
                    return sum + offer["discount_amount"].get<float>();
                })},
            {"execution_time_ms", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()}
        };
        
    } catch (const std::exception& e) {
        return json{
            {"status", "error"},
            {"message", std::string("GenerateDiscountOffers failed: ") + e.what()}
        };
    }
}

// ============================================================================
// STEP 4: Predict Cashflow (90 days)
// ============================================================================

std::vector<json> AdvancedCashflowEngine::PredictReceiptsForDate(
    const std::string& date_str,
    const std::map<std::string, json>& closing_balances,
    const json& client_categories) {
    
    std::vector<json> receipts;
    
    for (const auto& [party, balance_data] : closing_balances) {
        float balance = balance_data.value("balance", 0.0f);
        std::string last_sale_date = balance_data.value("last_sale_date", "");
        
        if (balance <= 0.001f || last_sale_date.empty()) continue;
        
        // Get party's average payment days
        float avg_days = 30.0f;
        for (const auto& cat : {"A", "B", "C"}) {
            for (const auto& party_info : client_categories[cat]) {
                if (party_info.value("party", "") == party) {
                    avg_days = party_info.value("avg_payment_days", 30.0f);
                    break;
                }
            }
        }
        
        int days_to_payment = static_cast<int>(std::round(avg_days));
        int days_since_sale = DaysBetween(last_sale_date, date_str);
        
        // Allow ±2 day window
        if (days_since_sale >= days_to_payment - 2 && days_since_sale <= days_to_payment + 2) {
            auto [category, _] = GetCategoryAndDiscount(avg_days);
            receipts.push_back(json{
                {"party", party},
                {"amount", balance},
                {"expected_date", last_sale_date},  // would be calculated
                {"category", std::string(1, category)},
                {"confidence", 0.85f}
            });
        }
    }
    
    return receipts;
}

std::vector<json> AdvancedCashflowEngine::PredictExpensesForDate(
    const std::string& date_str,
    const json& fixed_expenses) {
    
    std::vector<json> expenses;
    
    for (const auto& [description, expense_data] : fixed_expenses.items()) {
        std::string next_due_date = expense_data.value("next_due_date", "");
        int interval = expense_data.value("interval_days", 30);
        float amount = expense_data.value("amount", 0.0f);
        float confidence = expense_data.value("confidence", 0.8f);
        
        if (next_due_date.empty()) continue;
        
        int days_from_due = DaysBetween(next_due_date, date_str);
        
        // Allow ±2 day window
        if (days_from_due >= -2 && days_from_due <= 2) {
            expenses.push_back(json{
                {"description", description},
                {"amount", amount},
                {"type", "fixed"},
                {"confidence", confidence}
            });
        }
    }
    
    return expenses;
}

json AdvancedCashflowEngine::PredictCashflow(const json& request) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        if (!request.contains("bank_balance")) {
            return json{
                {"status", "error"},
                {"message", "Missing 'bank_balance'"}
            };
        }
        
        float bank_balance = request["bank_balance"].get<float>();
        int days = request.value("days", 90);
        auto closing_balances = request.value("closing_balances", json::object());
        auto client_categories = request.value("client_categories", json::object());
        auto fixed_expenses = request.value("fixed_expenses", json::object());
        
        if (days < 1 || days > 90) {
            return json{
                {"status", "error"},
                {"message", "Days must be between 1 and 90"}
            };
        }
        
        json predictions = json::array();
        float running_balance = bank_balance;
        float total_receipts = 0.0f;
        float total_expenses = 0.0f;
        
        // Current date (would be passed or use system date)
        std::string current_date = "2026-02-03";
        
        for (int day_num = 1; day_num <= days; ++day_num) {
            // Calculate prediction date (simplified)
            long current_day_num = DateToDays(current_date) + day_num;
            
            // Convert back to date string
            int year = 1970, month = 1, day = current_day_num;
            int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            
            while (day > 365) {
                int days_in_year = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
                if (day <= days_in_year) break;
                day -= days_in_year;
                year++;
            }
            
            if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) days_in_month[2] = 29;
            
            for (int m = 1; m <= 12; ++m) {
                if (day <= days_in_month[m]) {
                    month = m;
                    break;
                }
                day -= days_in_month[m];
            }
            
            char prediction_date[11];
            snprintf(prediction_date, sizeof(prediction_date), "%04d-%02d-%02d", year, month, day);
            
            auto day_receipts = PredictReceiptsForDate(prediction_date, 
                closing_balances.get<std::map<std::string, json>>(), 
                client_categories);
            auto day_expenses = PredictExpensesForDate(prediction_date, fixed_expenses);
            
            float day_receipts_total = 0.0f;
            for (const auto& r : day_receipts) {
                day_receipts_total += r["amount"].get<float>();
            }
            
            float day_expenses_total = 0.0f;
            for (const auto& e : day_expenses) {
                day_expenses_total += e["amount"].get<float>();
            }
            
            running_balance += day_receipts_total - day_expenses_total;
            total_receipts += day_receipts_total;
            total_expenses += day_expenses_total;
            
            predictions.push_back(json{
                {"date", std::string(prediction_date)},
                {"day_number", day_num},
                {"balance", std::round(running_balance * 100) / 100.0f},
                {"receipts", day_receipts},
                {"expenses", day_expenses},
                {"net_change", std::round((day_receipts_total - day_expenses_total) * 100) / 100.0f}
            });
        }
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
        
        return json{
            {"status", "success"},
            {"current_balance", bank_balance},
            {"predictions", predictions},
            {"summary", json{
                {"days_forecast", days},
                {"starting_balance", bank_balance},
                {"ending_balance", std::round(running_balance * 100) / 100.0f},
                {"total_predicted_receipts", std::round(total_receipts * 100) / 100.0f},
                {"total_predicted_expenses", std::round(total_expenses * 100) / 100.0f}
            }},
            {"execution_time_ms", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()}
        };
        
    } catch (const std::exception& e) {
        return json{
            {"status", "error"},
            {"message", std::string("PredictCashflow failed: ") + e.what()}
        };
    }
}

// ============================================================================
// STEP 5: Full Pipeline Execute
// ============================================================================

json AdvancedCashflowEngine::ExecuteFullPipeline(const json& request) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Step 1: Categorize clients
        auto categorize_result = CategorizeClients(request);
        if (categorize_result.contains("status") && categorize_result["status"].get<std::string>() != "success") {
            return categorize_result;
        }
        
        auto client_categories = categorize_result.value("categories", json::object());
        
        // Step 2: Detect fixed expenses
        auto expenses_result = DetectFixedExpenses(request);
        auto fixed_expenses = expenses_result.value("expenses", json::object());
        
        // Step 3: Get closing balances
        std::vector<json> transactions = request.value("transactions", json::array());
        auto closing_balances = GetClosingBalances(transactions);
        
        // Prepare for Step 4
        json cashflow_request = {
            {"bank_balance", request.value("bank_balance", 100000.0f)},
            {"days", request.value("days", 90)},
            {"closing_balances", closing_balances},
            {"client_categories", client_categories},
            {"fixed_expenses", fixed_expenses}
        };
        
        // Step 4: Predict cashflow
        auto cashflow_result = PredictCashflow(cashflow_request);
        
        // Step 5: Generate discount offers
        json discount_request = {
            {"closing_balances", closing_balances},
            {"client_categories", client_categories}
        };
        
        auto discount_result = GenerateDiscountOffers(discount_request);
        auto discount_offers = discount_result.value("offers", json::array());
        
        auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
        
        return json{
            {"status", "success"},
            {"client_categories", client_categories},
            {"fixed_expenses", fixed_expenses},
            {"discount_offers", discount_offers},
            {"predictions", cashflow_result.value("predictions", json::array())},
            {"summary", cashflow_result.value("summary", json::object())},
            {"total_execution_time_ms", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()}
        };
        
    } catch (const std::exception& e) {
        return json{
            {"status", "error"},
            {"message", std::string("ExecuteFullPipeline failed: ") + e.what()}
        };
    }
}
