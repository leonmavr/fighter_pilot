#include "fx/SmokeProfileAsset.hpp"

#include <fstream>
#include <iostream>
#include <string>

namespace planet_aces {

namespace {

std::string trim_copy(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

}  // namespace

SmokeTrailParameters load_smoke_trail_file(const std::string& filePath) {
    SmokeTrailParameters parameters;

    std::ifstream assetFile(filePath);
    if (!assetFile) {
        std::cerr << "warning: could not open smoke asset at " << filePath << '\n';
        return parameters;
    }

    std::string line;
    std::vector<float> sizes;
    while (std::getline(assetFile, line)) {
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = trim_copy(trimmed.substr(0, equals));
        const std::string value = trim_copy(trimmed.substr(equals + 1));

        try {
            if (key == "spawn_interval_ms") {
                parameters.spawnIntervalSeconds = std::stof(value) / 1000.0f;
            } else if (key == "step_interval_ms") {
                parameters.stepIntervalSeconds = std::stof(value) / 1000.0f;
            } else if (key == "size") {
                sizes.push_back(std::stof(value));
            }
        } catch (const std::exception&) {
            std::cerr << "warning: failed to parse smoke asset line: " << trimmed << '\n';
        }
    }

    if (!sizes.empty()) {
        parameters.sizeOverLife = std::move(sizes);
    }
    parameters.entityParameters.lifetimeSeconds = parameters.stepIntervalSeconds
        * static_cast<float>(parameters.sizeOverLife.size());

    return parameters;
}

}  // namespace planet_aces