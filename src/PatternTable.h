#ifndef PATTERNTABLE_H
#define PATTERNTABLE_H

#include <omnetpp.h>
#include <map>
#include <vector>
#include <utility>
#include <string>

using namespace omnetpp;

/**
 * Pattern Table using STL map to track (fromPage, toPage) → count
 * Implements pattern-based prediction for HTTP request sequences
 */
class PatternTable
{
public:
    // Type definitions for cleaner code
    typedef std::pair<int, int> PageTransition;  // (fromPage, toPage)
    typedef std::map<PageTransition, int> TransitionMap;
    typedef std::map<int, std::vector<int>> PagePredictionMap;
    
private:
    TransitionMap transitions;  // Map of (fromPage, toPage) → count
    PagePredictionMap predictions;  // Cached predictions for each page
    int totalTransitions;
    double confidenceThreshold;  // Minimum confidence for predictions
    int maxPredictions;  // Maximum number of predictions per page
    bool enableLearning;  // Enable/disable pattern learning
    
    // Statistics
    int totalUpdates;
    int predictionRequests;
    int successfulPredictions;
    
public:
    // Constructors
    PatternTable(double threshold = 0.1, int maxPred = 5);
    PatternTable(const PatternTable& other);
    
    // Destructor
    ~PatternTable();
    
    // Assignment operator
    PatternTable& operator=(const PatternTable& other);
    
    // Pattern learning methods
    void recordTransition(int fromPage, int toPage);
    void recordSequence(const std::vector<int>& pageSequence);
    void updatePattern(int fromPage, int toPage, int count = 1);
    
    // Pattern prediction methods
    std::vector<int> getPredictions(int currentPage) const;
    std::vector<std::pair<int, double>> getPredictionsWithConfidence(int currentPage) const;
    int getMostLikelyNextPage(int currentPage) const;
    double getTransitionProbability(int fromPage, int toPage) const;
    
    // Pattern analysis methods
    int getTransitionCount(int fromPage, int toPage) const;
    int getTotalTransitionsFrom(int fromPage) const;
    std::vector<PageTransition> getTopTransitions(int limit = 10) const;
    std::vector<int> getReachablePages(int fromPage) const;
    
    // Configuration methods
    void setConfidenceThreshold(double threshold) { confidenceThreshold = threshold; }
    void setMaxPredictions(int maxPred) { maxPredictions = maxPred; }
    void setEnableLearning(bool enable) { enableLearning = enable; }
    
    // Getters
    double getConfidenceThreshold() const { return confidenceThreshold; }
    int getMaxPredictions() const { return maxPredictions; }
    bool isLearningEnabled() const { return enableLearning; }
    int getTotalTransitions() const { return totalTransitions; }
    size_t getPatternCount() const { return transitions.size(); }
    
    // Statistics methods
    int getTotalUpdates() const { return totalUpdates; }
    int getPredictionRequests() const { return predictionRequests; }
    int getSuccessfulPredictions() const { return successfulPredictions; }
    double getPredictionAccuracy() const;
    
    // Maintenance methods
    void clear();  // Clear all patterns
    void clearPredictionsCache();  // Clear cached predictions
    void compact(int minCount = 1);  // Remove patterns with count < minCount
    void decay(double factor = 0.9);  // Apply decay factor to all counts
    
    // Persistence methods (for saving/loading patterns)
    std::string serialize() const;
    bool deserialize(const std::string& data);
    
    // Debug and utility methods
    std::string toString() const;
    void printStatistics() const;
    void printTopPatterns(int limit = 20) const;
    
    // Iterator support for external analysis
    TransitionMap::const_iterator begin() const { return transitions.begin(); }
    TransitionMap::const_iterator end() const { return transitions.end(); }
    
private:
    // Helper methods
    void updatePredictionsCache(int fromPage);
    bool isValidPage(int pageId) const;
    std::vector<std::pair<int, double>> calculateProbabilities(int fromPage) const;
};

#endif // PATTERNTABLE_H