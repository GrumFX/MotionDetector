// MotionDetector.cpp

#include "MotionDetector.h"
#include <thread>
#include <numeric>
#include <cmath>

MotionDetector::MotionDetector(int collectionDurationSec, double threshold)
    : durationSec_(collectionDurationSec), threshold_(threshold)
{
}

void MotionDetector::collectData(Scanner& scanner)
{
    samples_.clear();
    auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start
        < std::chrono::seconds(durationSec_))
    {
        auto batch = scanner.scan();
        samples_.insert(samples_.end(), batch.begin(), batch.end());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void MotionDetector::computeAverages()
{
    std::map<std::string, std::pair<double, int>> acc;
    for (auto& m : samples_)
    {
        auto& entry = acc[m.source];
        entry.first += m.rssi;
        entry.second += 1;
    }
    averages_.clear();
    for (auto& [src, v] : acc)
    {
        averages_[src] = v.first / v.second;
    }
}

bool MotionDetector::isMovement(const Measurement& m) const
{
    auto it = averages_.find(m.source);
    if (it == averages_.end()) return false;
    return std::fabs(m.rssi - it->second) > threshold_;
}

void MotionDetector::addSample(const Measurement& m)
{
    samples_.push_back(m);
}

const std::map<std::string, double>& MotionDetector::getAverages() const
{
    return averages_;
}