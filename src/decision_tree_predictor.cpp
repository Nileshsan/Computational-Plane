#include "decision_tree_predictor.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

namespace PBS::ML {

// ============================================================================
// DECISION TREE NODE IMPLEMENTATION
// ============================================================================

DecisionTreeNode::DecisionTreeNode(bool isLeaf, double value)
    : is_leaf(isLeaf), prediction_value(value), 
      feature_index(-1), threshold(0.0),
      left_child(nullptr), right_child(nullptr),
      samples(0), entropy(0.0) {}

DecisionTreeNode::~DecisionTreeNode() {
    delete left_child;
    delete right_child;
}

// ============================================================================
// DECISION TREE CLASSIFIER IMPLEMENTATION
// ============================================================================

DecisionTreeClassifier::DecisionTreeClassifier(int maxDepth, int minSamplesSplit, double minGiniDecrease)
    : max_depth(maxDepth), min_samples_split(minSamplesSplit), 
      min_gini_decrease(minGiniDecrease), root(nullptr) {}

DecisionTreeClassifier::~DecisionTreeClassifier() {
    delete root;
}

double DecisionTreeClassifier::CalculateEntropy(const std::vector<int>& labels) {
    if (labels.empty()) return 0.0;
    
    std::map<int, int> counts;
    for (int label : labels) {
        counts[label]++;
    }
    
    double entropy = 0.0;
    int total = labels.size();
    
    for (const auto& pair : counts) {
        double prob = static_cast<double>(pair.second) / total;
        if (prob > 0.0) {
            entropy -= prob * std::log2(prob);
        }
    }
    
    return entropy;
}

double DecisionTreeClassifier::CalculateGini(const std::vector<int>& labels) {
    if (labels.empty()) return 0.0;
    
    std::map<int, int> counts;
    for (int label : labels) {
        counts[label]++;
    }
    
    double gini = 1.0;
    int total = labels.size();
    
    for (const auto& pair : counts) {
        double prob = static_cast<double>(pair.second) / total;
        gini -= prob * prob;
    }
    
    return gini;
}

DecisionTreeNode* DecisionTreeClassifier::BuildTree(
    const std::vector<std::vector<double>>& features,
    const std::vector<int>& labels,
    int depth) {
    
    if (features.empty() || labels.empty()) {
        return new DecisionTreeNode(true, 0.0);
    }
    
    // Check stopping conditions
    if (depth >= max_depth || labels.size() < min_samples_split) {
        double avgLabel = std::accumulate(labels.begin(), labels.end(), 0.0) / labels.size();
        return new DecisionTreeNode(true, avgLabel);
    }
    
    // Check if all labels are the same (pure node)
    bool allSame = std::all_of(labels.begin(), labels.end(),
                               [&](int l) { return l == labels[0]; });
    if (allSame) {
        return new DecisionTreeNode(true, static_cast<double>(labels[0]));
    }
    
    // Find best split
    int numFeatures = features[0].size();
    double bestGiniGain = 0.0;
    int bestFeature = -1;
    double bestThreshold = 0.0;
    
    double parentGini = CalculateGini(labels);
    
    for (int featureIdx = 0; featureIdx < numFeatures; ++featureIdx) {
        // Get unique values for this feature
        std::vector<double> uniqueVals;
        for (const auto& sample : features) {
            uniqueVals.push_back(sample[featureIdx]);
        }
        std::sort(uniqueVals.begin(), uniqueVals.end());
        uniqueVals.erase(std::unique(uniqueVals.begin(), uniqueVals.end()), 
                        uniqueVals.end());
        
        // Try each threshold
        for (size_t i = 0; i < uniqueVals.size() - 1; ++i) {
            double threshold = (uniqueVals[i] + uniqueVals[i + 1]) / 2.0;
            
            // Split data
            std::vector<int> leftLabels, rightLabels;
            for (size_t j = 0; j < features.size(); ++j) {
                if (features[j][featureIdx] <= threshold) {
                    leftLabels.push_back(labels[j]);
                } else {
                    rightLabels.push_back(labels[j]);
                }
            }
            
            if (leftLabels.empty() || rightLabels.empty()) continue;
            
            // Calculate weighted gini
            double leftGini = CalculateGini(leftLabels);
            double rightGini = CalculateGini(rightLabels);
            double totalSamples = labels.size();
            double weightedGini = (leftLabels.size() / totalSamples) * leftGini +
                                 (rightLabels.size() / totalSamples) * rightGini;
            
            double giniGain = parentGini - weightedGini;
            
            if (giniGain > bestGiniGain) {
                bestGiniGain = giniGain;
                bestFeature = featureIdx;
                bestThreshold = threshold;
            }
        }
    }
    
    // If no good split found, create leaf
    if (bestFeature == -1 || bestGiniGain < min_gini_decrease) {
        double avgLabel = std::accumulate(labels.begin(), labels.end(), 0.0) / labels.size();
        return new DecisionTreeNode(true, avgLabel);
    }
    
    // Split data and recurse
    std::vector<std::vector<double>> leftFeatures, rightFeatures;
    std::vector<int> leftLabels, rightLabels;
    
    for (size_t i = 0; i < features.size(); ++i) {
        if (features[i][bestFeature] <= bestThreshold) {
            leftFeatures.push_back(features[i]);
            leftLabels.push_back(labels[i]);
        } else {
            rightFeatures.push_back(features[i]);
            rightLabels.push_back(labels[i]);
        }
    }
    
    // Create internal node
    DecisionTreeNode* node = new DecisionTreeNode(false, 0.0);
    node->feature_index = bestFeature;
    node->threshold = bestThreshold;
    node->samples = labels.size();
    node->entropy = parentGini;
    
    node->left_child = BuildTree(leftFeatures, leftLabels, depth + 1);
    node->right_child = BuildTree(rightFeatures, rightLabels, depth + 1);
    
    return node;
}

void DecisionTreeClassifier::Train(
    const std::vector<std::vector<double>>& features,
    const std::vector<int>& labels) {
    
    delete root;
    root = BuildTree(features, labels, 0);
}

double DecisionTreeClassifier::Predict(const std::vector<double>& sample) const {
    if (!root) return 0.0;
    
    DecisionTreeNode* current = root;
    
    while (!current->is_leaf) {
        if (sample[current->feature_index] <= current->threshold) {
            current = current->left_child;
        } else {
            current = current->right_child;
        }
    }
    
    return current->prediction_value;
}

std::vector<double> DecisionTreeClassifier::PredictBatch(
    const std::vector<std::vector<double>>& samples) const {
    
    std::vector<double> predictions;
    for (const auto& sample : samples) {
        predictions.push_back(Predict(sample));
    }
    return predictions;
}

// ============================================================================
// PAYMENT PROBABILITY DECISION TREE
// ============================================================================

PaymentProbabilityDecisionTree::PaymentProbabilityDecisionTree()
    : decision_tree(10, 5, 0.01) {}

void PaymentProbabilityDecisionTree::Train(
    const std::vector<ClientPaymentHistory>& trainingData) {
    
    std::vector<std::vector<double>> features;
    std::vector<int> labels;
    
    for (const auto& client : trainingData) {
        // Extract features from client history
        std::vector<double> clientFeatures = ExtractFeatures(client);
        features.push_back(clientFeatures);
        
        // Label: whether they typically pay on time (1) or late (0)
        int label = client.avg_payment_days <= 10 ? 1 : 0;
        labels.push_back(label);
    }
    
    decision_tree.Train(features, labels);
}

std::vector<double> PaymentProbabilityDecisionTree::ExtractFeatures(
    const ClientPaymentHistory& client) {
    
    std::vector<double> features;
    
    // Feature 1: Average payment days (normalized)
    features.push_back(std::min(client.avg_payment_days / 30.0, 1.0));
    
    // Feature 2: Payment consistency (1 - coefficient of variation)
    double cv = client.stdev_payment_days / std::max(client.avg_payment_days, 1.0);
    features.push_back(std::max(1.0 - cv, 0.0));
    
    // Feature 3: On-time payment percentage
    features.push_back(client.on_time_percentage / 100.0);
    
    // Feature 4: Outstanding amount ratio (normalized)
    double totalTransactions = client.transaction_count * 25000.0; // Assume avg 25k per transaction
    features.push_back(std::min(client.outstanding_amount / totalTransactions, 2.0));
    
    // Feature 5: Payment reliability (custom score)
    double reliability = (client.on_time_percentage / 100.0) * 
                        (1.0 - std::min(client.stdev_payment_days / client.avg_payment_days, 1.0));
    features.push_back(reliability);
    
    return features;
}

PaymentProbability PaymentProbabilityDecisionTree::PredictPaymentProbability(
    const ClientPaymentHistory& client,
    double targetAmount,
    int daysWindow) {
    
    PaymentProbability result;
    result.target_amount = targetAmount;
    result.days_window = daysWindow;
    
    // Get base prediction from decision tree
    std::vector<double> features = ExtractFeatures(client);
    double baseTreePrediction = decision_tree.Predict(features);
    
    // Calculate probability based on multiple factors
    double paymentCapability = std::min(targetAmount / std::max(client.outstanding_amount, 1.0), 1.0);
    double timeFactor = std::max(1.0 - (static_cast<double>(daysWindow) / 30.0), 0.1);
    double reliabilityScore = (client.on_time_percentage / 100.0);
    
    // Combine decision tree prediction with statistical factors
    double treeWeight = 0.4;
    double statsWeight = 0.6;
    
    double probabilityFromStats = paymentCapability * timeFactor * reliabilityScore;
    double combinedProbability = (treeWeight * baseTreePrediction) + 
                                (statsWeight * probabilityFromStats);
    
    result.probability_percent = combinedProbability * 100.0;
    result.confidence = CalculateConfidence(client, daysWindow);
    result.reasoning = GenerateReasoning(client, daysWindow, combinedProbability);
    
    return result;
}

double PaymentProbabilityDecisionTree::CalculateConfidence(
    const ClientPaymentHistory& client,
    int daysWindow) {
    
    // Confidence based on consistency and history size
    double consistency = 1.0 - std::min(client.stdev_payment_days / std::max(client.avg_payment_days, 1.0), 1.0);
    double historyFactor = std::min(static_cast<double>(client.transaction_count) / 20.0, 1.0);
    
    return (consistency * 0.6 + historyFactor * 0.4) * 100.0;
}

std::string PaymentProbabilityDecisionTree::GenerateReasoning(
    const ClientPaymentHistory& client,
    int daysWindow,
    double probability) {
    
    std::string reasoning;
    
    reasoning += "Decision Tree Analysis: ";
    
    if (client.avg_payment_days <= 10) {
        reasoning += "QUICK PAYER (≤10 days) → ";
    } else if (client.avg_payment_days <= 25) {
        reasoning += "NORMAL PAYER (11-25 days) → ";
    } else {
        reasoning += "SLOW PAYER (>25 days) → ";
    }
    
    if (probability > 80) {
        reasoning += "High likelihood within " + std::to_string(daysWindow) + " days ";
    } else if (probability > 50) {
        reasoning += "Moderate likelihood within " + std::to_string(daysWindow) + " days ";
    } else {
        reasoning += "Low likelihood within " + std::to_string(daysWindow) + " days ";
    }
    
    reasoning += "(Consistency: " + std::to_string(static_cast<int>(client.on_time_percentage)) + "%)";
    
    return reasoning;
}

// ============================================================================
// CASHFLOW DECISION TREE
// ============================================================================

CashflowDecisionTree::CashflowDecisionTree()
    : classifier(12, 3, 0.001) {}

void CashflowDecisionTree::Train(
    const std::vector<CashflowDataPoint>& trainingData) {
    
    std::vector<std::vector<double>> features;
    std::vector<int> labels;
    
    for (const auto& datapoint : trainingData) {
        std::vector<double> feat;
        
        // Feature 1: Average collection period (normalized)
        feat.push_back(std::min(datapoint.avg_collection_days / 30.0, 1.0));
        
        // Feature 2: Outstanding amount (normalized)
        feat.push_back(std::min(datapoint.outstanding_amount / 100000.0, 1.0));
        
        // Feature 3: Number of clients
        feat.push_back(std::min(static_cast<double>(datapoint.num_clients) / 50.0, 1.0));
        
        // Feature 4: Payment consistency
        feat.push_back(datapoint.consistency_score / 100.0);
        
        // Feature 5: Seasonal factor
        feat.push_back(datapoint.seasonal_factor);
        
        features.push_back(feat);
        
        // Label: Cashflow health (0=poor, 1=fair, 2=good, 3=excellent)
        int label = 0;
        if (datapoint.predicted_collection > datapoint.outstanding_amount * 0.9) {
            label = 3;
        } else if (datapoint.predicted_collection > datapoint.outstanding_amount * 0.7) {
            label = 2;
        } else if (datapoint.predicted_collection > datapoint.outstanding_amount * 0.5) {
            label = 1;
        }
        labels.push_back(label);
    }
    
    classifier.Train(features, labels);
}

CashflowDecision CashflowDecisionTree::PredictCashflow(
    double avgCollectionDays,
    double outstandingAmount,
    int numClients,
    double consistencyScore,
    double seasonalFactor) {
    
    CashflowDecision decision;
    
    std::vector<double> features = {
        std::min(avgCollectionDays / 30.0, 1.0),
        std::min(outstandingAmount / 100000.0, 1.0),
        std::min(static_cast<double>(numClients) / 50.0, 1.0),
        consistencyScore / 100.0,
        seasonalFactor
    };
    
    double treeDecision = classifier.Predict(features);
    
    int healthLevel = static_cast<int>(treeDecision);
    
    switch (healthLevel) {
        case 3:
            decision.health_level = "excellent";
            decision.recommendation = "maximize_credit_lines";
            decision.confidence = 95.0;
            break;
        case 2:
            decision.health_level = "good";
            decision.recommendation = "maintain_current_terms";
            decision.confidence = 85.0;
            break;
        case 1:
            decision.health_level = "fair";
            decision.recommendation = "implement_early_payment_incentives";
            decision.confidence = 75.0;
            break;
        default:
            decision.health_level = "poor";
            decision.recommendation = "implement_strict_payment_terms";
            decision.confidence = 70.0;
    }
    
    decision.reasoning = "Decision tree classified cashflow health as '" + decision.health_level + 
                        "' based on collection patterns, consistency, and outstanding amounts.";
    
    return decision;
}

// ============================================================================
// ENSEMBLE DECISION TREE
// ============================================================================

EnsembleDecisionTree::EnsembleDecisionTree()
    : trees(5, DecisionTreeClassifier(8, 4, 0.01)),
      bootstrap_sample_ratio(0.8) {}

void EnsembleDecisionTree::Train(
    const std::vector<std::vector<double>>& features,
    const std::vector<int>& labels) {
    
    int numSamples = features.size();
    int bootstrapSize = static_cast<int>(numSamples * bootstrap_sample_ratio);
    
    // Train each tree on bootstrap sample
    for (auto& tree : trees) {
        std::vector<std::vector<double>> bootstrapFeatures;
        std::vector<int> bootstrapLabels;
        
        for (int i = 0; i < bootstrapSize; ++i) {
            int idx = rand() % numSamples;
            bootstrapFeatures.push_back(features[idx]);
            bootstrapLabels.push_back(labels[idx]);
        }
        
        tree.Train(bootstrapFeatures, bootstrapLabels);
    }
}

double EnsembleDecisionTree::Predict(const std::vector<double>& sample) const {
    double sum = 0.0;
    for (const auto& tree : trees) {
        sum += tree.Predict(sample);
    }
    return sum / trees.size();
}

std::vector<double> EnsembleDecisionTree::PredictBatch(
    const std::vector<std::vector<double>>& samples) const {
    
    std::vector<double> predictions;
    for (const auto& sample : samples) {
        predictions.push_back(Predict(sample));
    }
    return predictions;
}

} // namespace PBS::ML
