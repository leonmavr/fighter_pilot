#pragma once

#include "core/Math.hpp"

#include "raylib.h"

#include <array>
#include <cstddef>
#include <unordered_map>

namespace planet_aces {

struct TerrainParameters {
    double waterLevelMeters = 0.0;
    double waterSurfaceOffsetMeters = -6.0;
    double waterDepthFadeMeters = 220.0;
    double waterMinAttenuation = 0.5;
    double waterMaxAttenuation = 1.0;
    double waterBlueWeight = 0.78;
    double waterTerrainWeight = 0.22;
    double waterMaxCellSizeMeters = 100.0;
    double maxElevationMeters = 2'500.0;
    double minElevationMeters = -250.0;
    double baseVisibleRadiusMeters = 4'500.0;
    double maxVisibleRadiusMeters = 18'000.0;
    double visibleRadiusPerAltitudeMeter = 8.0;
    double chunkSizeMeters = 1'600.0;
    double skirtDepthMeters = 180.0;
    std::size_t maxChunkBuildsPerFrame = 6;
    std::array<double, 5> lodRadiusFractions {0.28, 0.42, 0.62, 0.82, 1.0};
    std::array<double, 5> lodMaxDistanceMeters {1'250.0, 2'400.0, 4'800.0, 8'800.0, 18'000.0};
    std::array<double, 5> lodCellSizeMeters {25.0, 50.0, 100.0, 200.0, 400.0};
    unsigned int noiseSeed = 1'330U;
};

class Terrain {
public:
    explicit Terrain(TerrainParameters parameters = {});
    ~Terrain();

    Terrain(const Terrain&) = delete;
    Terrain& operator=(const Terrain&) = delete;

    double sample_height_meters(double worldXMeters, double worldZMeters) const noexcept;
    void draw(const Camera3D& camera);

private:
    static constexpr double kLowElevation = 100.0;
    static constexpr double kMidElevation = 800.0;
    static constexpr double kHighElevation = 1600.0;
    static constexpr double kSnowElevation = 2200.0;

    struct ChunkKey {
        int chunkX = 0;
        int chunkZ = 0;
        int lodLevel = 0;

        bool operator==(const ChunkKey& other) const noexcept {
            return chunkX == other.chunkX && chunkZ == other.chunkZ && lodLevel == other.lodLevel;
        }
    };

    struct ChunkKeyHash {
        std::size_t operator()(const ChunkKey& key) const noexcept;
    };

    struct ChunkMesh {
        Model terrainModel {};
        Model waterModel {};
        bool hasWater = false;
    };

    double visible_radius_meters(const Camera3D& camera) const noexcept;
    int lod_level_for_distance(double distanceMeters, double visibleRadiusMeters) const noexcept;
    void prune_cache(double cameraXMeters, double cameraZMeters, double visibleRadiusMeters);
    ChunkMesh build_chunk_mesh(int chunkX, int chunkZ, int lodLevel) const;
    double fractal_noise(double x, double z, int octaves, double lacunarity, double gain) const noexcept;
    double ridge_noise(double x, double z, int octaves, double lacunarity, double gain) const noexcept;
    double perlin_noise(double x, double z) const noexcept;
    double gradient_noise(int gridX, int gridZ, double offsetX, double offsetZ) const noexcept;
    Color color_for_height(double heightMeters) const noexcept;

    TerrainParameters parameters_;
    std::unordered_map<ChunkKey, ChunkMesh, ChunkKeyHash> chunkCache_;
    Shader shader_ {};
    bool shaderLoaded_ = false;
};

}  // namespace planet_aces