#include "core/AppConfig.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace {
    std::string trim(std::string_view input) {
        auto begin = input.begin();
        auto end = input.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
        while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) --end;
        return std::string(begin, end);
    }

    template <typename T>
    T parseNumber(const std::unordered_map<std::string, std::string> &values,
                  std::string_view key,
                  T fallback) {
        const auto it = values.find(std::string(key));
        if (it == values.end()) return fallback;

        T result{};
        const std::string &raw = it->second;
        const auto *begin = raw.data();
        const auto *end = raw.data() + raw.size();
        if constexpr (std::is_floating_point_v<T>) {
            // std::from_chars for floating point is still uneven across some
            // Windows compiler/STL combinations, so streams are the safer path.
            std::istringstream stream(raw);
            stream >> result;
            return stream ? result : fallback;
        } else {
            auto [ptr, ec] = std::from_chars(begin, end, result);
            return ec == std::errc{} ? result : fallback;
        }
    }

    bool parseBool(const std::unordered_map<std::string, std::string> &values,
                   std::string_view key,
                   bool fallback) {
        const auto it = values.find(std::string(key));
        if (it == values.end()) return fallback;

        std::string value = it->second;
        std::ranges::transform(value, value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
        if (value == "0" || value == "false" || value == "no" || value == "off") return false;
        return fallback;
    }

    glm::vec3 parseVec3(const std::unordered_map<std::string, std::string> &values,
                        std::string_view key,
                        glm::vec3 fallback) {
        const auto it = values.find(std::string(key));
        if (it == values.end()) return fallback;

        std::istringstream stream(it->second);
        char comma1 = 0;
        char comma2 = 0;
        glm::vec3 parsed{};
        stream >> parsed.r >> comma1 >> parsed.g >> comma2 >> parsed.b;
        if (!stream || comma1 != ',' || comma2 != ',') return fallback;

        return glm::clamp(parsed, glm::vec3(0.0f), glm::vec3(1.0f));
    }

    std::unordered_map<std::string, std::string> readIni(const std::filesystem::path &path) {
        std::unordered_map<std::string, std::string> values;
        std::ifstream file(path);
        if (!file) return values;

        // Minimal INI format: key=value pairs, with # or ; comments. Sections
        // are intentionally not supported because the config is small and flat.
        std::string line;
        while (std::getline(file, line)) {
            const auto comment = line.find_first_of("#;");
            if (comment != std::string::npos) line.erase(comment);

            const auto equals = line.find('=');
            if (equals == std::string::npos) continue;

            std::string key = trim(std::string_view(line).substr(0, equals));
            std::string value = trim(std::string_view(line).substr(equals + 1));
            if (!key.empty()) values[key] = value;
        }

        return values;
    }

    AppConfig fromIni(const std::filesystem::path &path) {
        AppConfig config;
        config.sourcePath = path;

        const auto values = readIni(path);
        // Clamp every user-editable value at the boundary. A bad config file
        // should never create an invalid quantum state or an enormous GPU load.
        config.windowWidth = std::clamp(parseNumber(values, "windowWidth", config.windowWidth), 800, 7680);
        config.windowHeight = std::clamp(parseNumber(values, "windowHeight", config.windowHeight), 600, 4320);
        config.pointCount = std::clamp(parseNumber(values, "pointCount", config.pointCount),
                                       kMinimumPointCount, kMaximumPointCount);
        config.densityThreshold = std::clamp(parseNumber(values, "densityThreshold", config.densityThreshold),
                                             0.0f, 0.95f);
        config.pointSize = std::clamp(parseNumber(values, "pointSize", config.pointSize), 1.5f, 32.0f);
        config.colorIntensity = std::clamp(parseNumber(values, "colorIntensity", config.colorIntensity),
                                           0.1f, 10.0f);
        config.animationSpeed = std::clamp(parseNumber(values, "animationSpeed", config.animationSpeed),
                                           0.0f, 4.0f);
        config.clipPlane = std::clamp(parseNumber(values, "clipPlane", config.clipPlane), -600.0f, 600.0f);
        config.clipEnabled = parseBool(values, "clipEnabled", config.clipEnabled);
        config.clipMode = static_cast<ClipMode>(std::clamp(parseNumber(values, "clipMode", 2), 0, 2));
        config.vsync = parseBool(values, "vsync", config.vsync);

        config.renderMode = static_cast<RenderMode>(std::clamp(parseNumber(values, "renderMode", 0), 0, 4));
        config.colorMap = static_cast<ColorMap>(std::clamp(parseNumber(values, "colorMap", 0), 0, 4));
        config.theme = static_cast<UiTheme>(std::clamp(parseNumber(values, "theme", 0), 0, 2));
        config.pointTint = parseVec3(values, "pointTint", config.pointTint);
        config.backgroundColor = parseVec3(values, "backgroundColor", config.backgroundColor);
        return config;
    }
}

AppConfig AppConfig::loadDefaultLocations() {
    // The same executable is used from the build tree, an installed directory,
    // and GitHub release archives. Check the common relative config locations.
    const std::array<std::filesystem::path, 4> candidates = {
        "config/QuantumAtom.ini",
        "../config/QuantumAtom.ini",
        "../../config/QuantumAtom.ini",
        "QuantumAtom.ini"
    };

    for (const auto &candidate: candidates) {
        if (std::filesystem::exists(candidate)) {
            return fromIni(candidate);
        }
    }

    AppConfig config;
    config.sourcePath = "config/QuantumAtom.ini";
    return config;
}

bool AppConfig::save() const {
    std::error_code ec;
    if (sourcePath.has_parent_path()) {
        // Ignore directory creation errors here; the actual file open below is
        // the authoritative success/failure check.
        std::filesystem::create_directories(sourcePath.parent_path(), ec);
    }

    std::ofstream file(sourcePath);
    if (!file) return false;

    file << "# QuantumAtom runtime defaults\n";
    file << "windowWidth=" << windowWidth << '\n';
    file << "windowHeight=" << windowHeight << '\n';
    file << "pointCount=" << pointCount << '\n';
    file << "densityThreshold=" << densityThreshold << '\n';
    file << "pointSize=" << pointSize << '\n';
    file << "colorIntensity=" << colorIntensity << '\n';
    file << "animationSpeed=" << animationSpeed << '\n';
    file << "clipEnabled=" << (clipEnabled ? "true" : "false") << '\n';
    file << "clipPlane=" << clipPlane << '\n';
    file << "clipMode=" << static_cast<int>(clipMode) << '\n';
    file << "vsync=" << (vsync ? "true" : "false") << '\n';
    file << "renderMode=" << static_cast<int>(renderMode) << '\n';
    file << "colorMap=" << static_cast<int>(colorMap) << '\n';
    file << "theme=" << static_cast<int>(theme) << '\n';
    file << "pointTint=" << pointTint.r << ',' << pointTint.g << ',' << pointTint.b << '\n';
    file << "backgroundColor=" << backgroundColor.r << ',' << backgroundColor.g << ',' << backgroundColor.b << '\n';
    return true;
}
