#include "PatternTable.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>

// Constructors
PatternTable::PatternTable(double threshold, int maxPred)
{
    confidenceThreshold = threshold;
    maxPredictions = maxPred;
    totalTransitions = 0;
    enableLearning = true;
    
    // Initialize statistics
    totalUpdates = 0;
    predictionRequests = 0;
    successfulPredictions = 0;
}

PatternTable::PatternTable(const PatternTable& other)
{
    transitions = other.transitions;
    predictions = other.predictions;
    totalTransitions = other.totalTransitions;
    confidenceThreshold = other.confidenceThreshold;
    maxPredictions = other.maxPredictions;
    enableLearning = other.enableLearning;
    totalUpdates = other.totalUpdates;
    predictionRequests = other.predictionRequests;
    successfulPredictions = other.successfulPredictions;
}

// Destructor
PatternTable::~PatternTable()
{
    // No dynamic memory to clean up
}

// Assignment operator
PatternTable& PatternTable::operator=(const PatternTable& other)
{
    if (this == &other) return *this;
    
    transitions = other.transitions;
    predictions = other.predictions;
    totalTransitions = other.totalTransitions;
    confidenceThreshold = other.confidenceThreshold;
    maxPredictions = other.maxPredictions;
    enableLearning = other.enableLearning;
    totalUpdates = other.totalUpdates;
    predictionRequests = other.predictionRequests;
    successfulPredictions = other.successfulPredictions;
    
    return *this;
}

// Pattern learning methods
void PatternTable::recordTransition(int fromPage, int toPage)
{
    if (!enableLearning || !isValidPage(fromPage) || !isValidPage(toPage)) {
        return;
    }
    
    PageTransition transition(fromPage, toPage);
    transitions[transition]++;
    totalTransitions++;
    totalUpdates++;
    
    // Invalidate cached predictions for this fromPage
    predictions.erase(fromPage);
}

void PatternTable::recordSequence(const std::vector<int>& pageSequence)
{
    if (!enableLearning || pageSequence.size() < 2) {
        return;
    }
    
    for (size_t i = 0; i < pageSequence.size() - 1; i++) {
        recordTransition(pageSequence[i], pageSequence[i + 1]);
    }
}

void PatternTable::updatePattern(int fromPage, int toPage, int count)
{
    if (!enableLearning || !isValidPage(fromPage) || !isValidPage(toPage) || count <= 0) {
        return;
    }
    
    PageTransition transition(fromPage, toPage);
    transitions[transition] += count;
    totalTransitions += count;
    totalUpdates++;
    
    // Invalidate cached predictions for this fromPage
    predictions.erase(fromPage);
}

// Pattern prediction methods
std::vector<int> PatternTable::getPredictions(int currentPage) const
{
    const_cast<PatternTable*>(this)->predictionRequests++;
    
    if (!isValidPage(currentPage)) {
        return std::vector<int>();
    }
    
    // Check cache first
    auto cacheIt = predictions.find(currentPage);
    if (cacheIt != predictions.end()) {
        return cacheIt->second;
    }
    
    // Calculate predictions
    std::vector<std::pair<int, double>> probabilities = calculateProbabilities(currentPage);
    std::vector<int> result;
    
    for (const auto& prob : probabilities) {
        if (prob.second >= confidenceThreshold && result.size() < static_cast<size_t>(maxPredictions)) {
            result.push_back(prob.first);
        }
    }
    
    // Cache the result
    const_cast<PatternTable*>(this)->predictions[currentPage] = result;
    
    return result;
}

std::vector<std::pair<int, double>> PatternTable::getPredictionsWithConfidence(int currentPage) const
{
    const_cast<PatternTable*>(this)->predictionRequests++;
    
    if (!isValidPage(currentPage)) {
        return std::vector<std::pair<int, double>>();
    }
    
    return calculateProbabilities(currentPage);
}

int PatternTable::getMostLikelyNextPage(int currentPage) const
{
    auto probabilities = calculateProbabilities(currentPage);
    
    if (probabilities.empty() || probabilities[0].second < confidenceThreshold) {
        return -1;  // No valid prediction
    }
    
    return probabilities[0].first;
}

double PatternTable::getTransitionProbability(int fromPage, int toPage) const
{
    if (!isValidPage(fromPage) || !isValidPage(toPage)) {
        return 0.0;
    }
    
    PageTransition transition(fromPage, toPage);
    auto it = transitions.find(transition);
    
    if (it == transitions.end()) {
        return 0.0;
    }
    
    int totalFromPage = getTotalTransitionsFrom(fromPage);
    if (totalFromPage == 0) {
        return 0.0;
    }
    
    return static_cast<double>(it->second) / totalFromPage;
}

// Pattern analysis methods
int PatternTable::getTransitionCount(int fromPage, int toPage) const
{
    PageTransition transition(fromPage, toPage);
    auto it = transitions.find(transition);
    return (it != transitions.end()) ? it->second : 0;
}

int PatternTable::getTotalTransitionsFrom(int fromPage) const
{
    int total = 0;
    for (const auto& entry : transitions) {
        if (entry.first.first == fromPage) {
            total += entry.second;
        }
    }
    return total;
}

std::vector<PatternTable::PageTransition> PatternTable::getTopTransitions(int limit) const
{
    std::vector<std::pair<PageTransition, int>> sortedTransitions;
    
    for (const auto& entry : transitions) {
        sortedTransitions.push_back(entry);
    }
    
    std::sort(sortedTransitions.begin(), sortedTransitions.end(),
              [](const std::pair<PageTransition, int>& a, const std::pair<PageTransition, int>& b) {
                  return a.second > b.second;  // Sort by count descending
              });
    
    std::vector<PageTransition> result;
    int count = 0;
    for (const auto& entry : sortedTransitions) {
        if (count >= limit) break;
        result.push_back(entry.first);
        count++;
    }
    
    return result;
}

std::vector<int> PatternTable::getReachablePages(int fromPage) const
{
    std::vector<int> pages;
    
    for (const auto& entry : transitions) {
        if (entry.first.first == fromPage) {
            pages.push_back(entry.first.second);
        }
    }
    
    return pages;
}

// Statistics methods
double PatternTable::getPredictionAccuracy() const
{
    if (predictionRequests == 0) return 0.0;
    return static_cast<double>(successfulPredictions) / predictionRequests;
}

// Maintenance methods
void PatternTable::clear()
{
    transitions.clear();
    predictions.clear();
    totalTransitions = 0;
    totalUpdates = 0;
    predictionRequests = 0;
    successfulPredictions = 0;
}

void PatternTable::clearPredictionsCache()
{
    predictions.clear();
}

void PatternTable::compact(int minCount)
{
    auto it = transitions.begin();
    while (it != transitions.end()) {
        if (it->second < minCount) {
            totalTransitions -= it->second;
            it = transitions.erase(it);
        } else {
            ++it;
        }
    }
    clearPredictionsCache();
}

void PatternTable::decay(double factor)
{
    if (factor <= 0.0 || factor >= 1.0) return;
    
    for (auto& entry : transitions) {
        entry.second = static_cast<int>(entry.second * factor);
        if (entry.second == 0) entry.second = 1;  // Keep at least 1
    }
    
    totalTransitions = static_cast<int>(totalTransitions * factor);
    clearPredictionsCache();
}

// Debug and utility methods
std::string PatternTable::toString() const
{
    std::ostringstream oss;
    oss << "PatternTable{patterns=" << transitions.size()
        << ", totalTransitions=" << totalTransitions
        << ", accuracy=" << std::fixed << std::setprecision(3) << getPredictionAccuracy()
        << "}";
    return oss.str();
}

void PatternTable::printStatistics() const
{
    std::cout << "Pattern Table Statistics:" << std::endl;
    std::cout << "  Total patterns: " << transitions.size() << std::endl;
    std::cout << "  Total transitions: " << totalTransitions << std::endl;
    std::cout << "  Total updates: " << totalUpdates << std::endl;
    std::cout << "  Prediction requests: " << predictionRequests << std::endl;
    std::cout << "  Successful predictions: " << successfulPredictions << std::endl;
    std::cout << "  Prediction accuracy: " << std::fixed << std::setprecision(3) 
              << getPredictionAccuracy() << std::endl;
    std::cout << "  Confidence threshold: " << confidenceThreshold << std::endl;
    std::cout << "  Max predictions: " << maxPredictions << std::endl;
}

// Private helper methods
void PatternTable::updatePredictionsCache(int fromPage)
{
    predictions[fromPage] = getPredictions(fromPage);
}

bool PatternTable::isValidPage(int pageId) const
{
    return pageId >= 0;  // Simple validation - non-negative page IDs
}

std::vector<std::pair<int, double>> PatternTable::calculateProbabilities(int fromPage) const
{
    std::vector<std::pair<int, double>> probabilities;
    int totalFromPage = getTotalTransitionsFrom(fromPage);
    
    if (totalFromPage == 0) {
        return probabilities;
    }
    
    for (const auto& entry : transitions) {
        if (entry.first.first == fromPage) {
            double probability = static_cast<double>(entry.second) / totalFromPage;
            probabilities.push_back(std::make_pair(entry.first.second, probability));
        }
    }
    
    // Sort by probability descending
    std::sort(probabilities.begin(), probabilities.end(),
              [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
                  return a.second > b.second;
              });
    
    return probabilities;
}