#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>

namespace PBS::ML {

// ============================================================================
// DECISION TREE NODE STRUCTURE
// ============================================================================

struct DecisionTreeNode {
    bool is_leaf;
    double prediction_value;
    
    // For internal nodes
    int feature_index;
    double threshold;
    DecisionTreeNode* left_child;
    DecisionTreeNode* right_child;
    
    // Statistics
    int samples;
    double entropy;
    
    DecisionTreeNode(bool isLeaf = true, double value = 0.0);
    ~DecisionTreeNode();
};

// ============================================================================
// DECISION TREE CLASSIFIER
// ============================================================================

class DecisionTreeClassifier {
public:
    DecisionTreeClassifier(int maxDepth = 10, int minSamplesSplit = 5, double minGiniDecrease = 0.01);
    ~DecisionTreeClassifier();
    
    // Training
    void Train(const std::vector<std::vector<double>>& features,
               const std::vector<int>& labels);
    
    // Prediction
    double Predict(const std::vector<double>& sample) const;
    std::vector<double> PredictBatch(const std::vector<std::vector<double>>& samples) const;
    
private:
    int max_depth;
    int min_samples_split;
    double min_gini_decrease;
    DecisionTreeNode* root;
    
    DecisionTreeNode* BuildTree(const std::vector<std::vector<double>>& features,
                               const std::vector<int>& labels,
                               int depth);
    
    double CalculateEntropy(const std::vector<int>& labels);
    double CalculateGini(const std::vector<int>& labels);
};

// ============================================================================
// CLIENT PAYMENT HISTORY FOR DECISION TREE
// ============================================================================

struct ClientPaymentHistory {
    std::string client_name;
    double avg_payment_days;
    double stdev_payment_days;
    double on_time_percentage;
    double outstanding_amount;
    int transaction_count;
};

// ============================================================================
// PAYMENT PROBABILITY DECISION TREE
// ============================================================================

struct PaymentProbability {
    double target_amount;
    int days_window;
    double probability_percent;
    double confidence;
    std::string reasoning;
};

class PaymentProbabilityDecisionTree {
public:
    PaymentProbabilityDecisionTree();
    
    // Training on historical data
    void Train(const std::vector<ClientPaymentHistory>& trainingData);
    
    // Predict probability of specific payment amount within timeframe
    // "What are the chances Client A pays 50k in 5 days?" → 35%
    PaymentProbability PredictPaymentProbability(
        const ClientPaymentHistory& client,
        double targetAmount,
        int daysWindow);
    
private:
    DecisionTreeClassifier decision_tree;
    
    std::vector<double> ExtractFeatures(const ClientPaymentHistory& client);
    double CalculateConfidence(const ClientPaymentHistory& client, int daysWindow);
    std::string GenerateReasoning(const ClientPaymentHistory& client, 
                                 int daysWindow, double probability);
};

// ============================================================================
// CASHFLOW DECISION TREE
// ============================================================================

struct CashflowDataPoint {
    double avg_collection_days;
    double outstanding_amount;
    int num_clients;
    double consistency_score;
    double seasonal_factor;
    double predicted_collection;
};

struct CashflowDecision {
    std::string health_level;        // poor, fair, good, excellent
    std::string recommendation;      // specific action
    double confidence;
    std::string reasoning;
};

class CashflowDecisionTree {
public:
    CashflowDecisionTree();
    
    // Training on historical cashflow data
    void Train(const std::vector<CashflowDataPoint>& trainingData);
    
    // Predict cashflow health and recommendation
    CashflowDecision PredictCashflow(
        double avgCollectionDays,
        double outstandingAmount,
        int numClients,
        double consistencyScore,
        double seasonalFactor);
    
private:
    DecisionTreeClassifier classifier;
};

// ============================================================================
// ENSEMBLE DECISION TREE (RANDOM FOREST STYLE)
// ============================================================================

class EnsembleDecisionTree {
public:
    EnsembleDecisionTree();
    
    // Train multiple trees with bootstrap samples
    void Train(const std::vector<std::vector<double>>& features,
               const std::vector<int>& labels);
    
    // Predict using ensemble voting
    double Predict(const std::vector<double>& sample) const;
    std::vector<double> PredictBatch(const std::vector<std::vector<double>>& samples) const;
    
private:
    std::vector<DecisionTreeClassifier> trees;
    double bootstrap_sample_ratio;
};

} // namespace PBS::ML
