// MotionDetector.h
#pragma once

#include "Measurement.h"
#include "Scanner.h"
#include <vector>
#include <map>
#include <string>
#include <cmath>

/// MotionDetector collects RSSI samples from both the Raspberry (via Wi-Fi scans)
/// and ESP (via MQTT), computes per-(source,SSID) averages during calibration,
/// and then lets you test individual measurements against those averages to detect movement.
class MotionDetector
{
public:
    /// @param collectionDurationSec  calibration duration (in seconds)
    /// @param threshold              minimum RSSI deviation to count as movement (in dBm)
    MotionDetector(int collectionDurationSec = 30, double threshold = 10.0)
        : durationSec_(collectionDurationSec), threshold_(threshold)
    {
    }

    /// Returns how many seconds the calibration phase lasts
    int getDuration() const { return durationSec_; }

    /// Add a single RSSI measurement (from Wi-Fi scan or MQTT) to the buffer
    void addSample(const Measurement& m)
    {
        samples_.push_back(m);
    }

    /// Compute the average RSSI for each unique (source, SSID) pair
    void computeAverages()
    {
        // Temporary accumulation: map (source,SSID) ? (sumRSSI, count)
        std::map<std::pair<std::string, std::string>, std::pair<double, int>> acc;
        for (auto& m : samples_)
        {
            auto key = std::make_pair(m.source, m.ssid);
            auto& entry = acc[key];
            entry.first += m.rssi;
            entry.second += 1;
        }

        // Build the final averages_ map: (source,SSID) ? avgRSSI
        averages_.clear();
        for (auto& [key, pair] : acc)
        {
            averages_[key] = pair.first / pair.second;
        }
    }

    /// Return the map of (source, SSID) ? average RSSI
    const std::map<std::pair<std::string, std::string>, double>& getAverages() const
    {
        return averages_;
    }

    /// Return true if this measurement’s RSSI is more than threshold_ away from its own average
    bool isMovement(const Measurement& m) const
    {
        auto key = std::make_pair(m.source, m.ssid);
        auto it = averages_.find(key);
        if (it == averages_.end())
        {
            // If no average is computed for this (source, SSID), treat as no movement
            return false;
        }
        return std::fabs(m.rssi - it->second) > threshold_;
    }

private:
    int durationSec_;   ///< Calibration duration in seconds
    double threshold_;  ///< RSSI deviation threshold
    std::vector<Measurement> samples_;
    std::map<std::pair<std::string, std::string>, double> averages_;
};
