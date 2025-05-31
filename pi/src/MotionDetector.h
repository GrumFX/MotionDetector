#pragma once

#include "Measurement.h"
#include "Scanner.h"
#include <vector>
#include <map>
#include <string>
#include <cmath>

/// MotionDetector collects RSSI samples from both the Raspberry (via Wi-Fi scans)
/// and ESP (via MQTT), computes per-source averages during calibration, and then
/// lets you test individual measurements against those averages to detect movement.
class MotionDetector
{
public:
    /// @param collectionDurationSec  calibration duration (in seconds)
    /// @param threshold              minimum RSSI deviation to count as movement
    MotionDetector(int collectionDurationSec = 30, double threshold = 10.0)
        : durationSec_(collectionDurationSec), threshold_(threshold)
    {
    }

    /// Returns the calibration duration (in seconds)
    int getDuration() const
    {
        return durationSec_;
    }

    /// Add a single RSSI measurement to the sample buffer (from Wi-Fi scan or MQTT)
    void addSample(const Measurement& m)
    {
        samples_.push_back(m);
    }

    /// Compute per-source average RSSI from all collected samples
    void computeAverages()
    {
        std::map<std::string, std::pair<double, int>> acc;
        for (auto& m : samples_)
        {
            auto& entry = acc[m.source];
            entry.first += m.rssi;
            entry.second += 1;
        }
        averages_.clear();
        for (auto& [src, pair] : acc)
        {
            averages_[src] = pair.first / pair.second;
        }
    }

    /// Return the map of <source ? average RSSI>
    const std::map<std::string, double>& getAverages() const
    {
        return averages_;
    }

    /// Return true if this measurement deviates from the average by more than threshold_
    bool isMovement(const Measurement& m) const
    {
        auto it = averages_.find(m.source);
        if (it == averages_.end())
        {
            // If no average was computed for this source, treat as "no movement"
            return false;
        }
        return std::fabs(m.rssi - it->second) > threshold_;
    }

private:
    int durationSec_;                          /// Calibration duration in seconds
    double threshold_;                         /// RSSI deviation threshold
    std::vector<Measurement> samples_;         /// All collected samples during calibration
    std::map<std::string, double> averages_;   /// Computed per-source average RSSI
};
