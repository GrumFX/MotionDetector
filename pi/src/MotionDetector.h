// MotionDetector.h

#pragma once

#include "Scanner.h"
#include <vector>
#include <map>
#include <string>
#include <chrono>


class MotionDetector
{
public:

    MotionDetector(int collectionDurationSec = 30, double threshold = 10.0);

    int getDuration() const
    {
        return durationSec_;
    }

    void collectData(Scanner& scanner);

    void computeAverages();

    void addSample(const Measurement& m);

    const std::map<std::string, double>& getAverages() const;

    bool isMovement(const Measurement& m) const;


private:
    int durationSec_;
    double threshold_;
    std::vector<Measurement> samples_;
    std::map<std::string, double> averages_;
};
