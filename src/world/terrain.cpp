#include "world/terrain.hpp"

#include "core/AssetPaths.hpp"
#include "render/RaylibContext.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_set>
#include <vector>

#include "raymath.h"
#include "rlgl.h"

namespace planet_aces {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

double fade(double t) noexcept {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double lerp(double a, double b, double t) noexcept {
    return a + (b - a) * t;
}

std::uint64_t mix_bits(std::uint64_t value) noexcept {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    return value;
}

Color lerp_color(Color from, Color to, double t) noexcept {
    const auto lerp_channel = [t](unsigned char start, unsigned char end) {
        return static_cast<unsigned char>(std::clamp(
            static_cast<double>(start) + (static_cast<double>(end) - static_cast<double>(start)) * t,
            0.0,
            255.0));
    };

    return {
        lerp_channel(from.r, to.r),
        lerp_channel(from.g, to.g),
        lerp_channel(from.b, to.b),
        lerp_channel(from.a, to.a),
    };
}

Vector3 terrain_vertex(double worldXMeters, double heightMeters, double worldZMeters) {
    return render::to_vector3({worldXMeters, heightMeters, worldZMeters});
}

void append_mesh_vertex(
    std::vector<float>& vertices,
    std::vector<float>& normals,
    std::vector<unsigned char>& colors,
    const Vector3& position,
    const Vector3& normal,
    Color color) {
    vertices.push_back(position.x);
    vertices.push_back(position.y);
    vertices.push_back(position.z);

    normals.push_back(normal.x);
    normals.push_back(normal.y);
    normals.push_back(normal.z);

    colors.push_back(color.r);
    colors.push_back(color.g);
    colors.push_back(color.b);
    colors.push_back(color.a);
}

void append_triangle_indices(
    std::vector<unsigned short>& indices,
    unsigned short a,
    unsigned short b,
    unsigned short c) {
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
}

Color darken(Color color, unsigned char amount) {
    const auto reduce = [amount](unsigned char channel) {
        return static_cast<unsigned char>(channel > amount ? channel - amount : 0);
    };

    return {reduce(color.r), reduce(color.g), reduce(color.b), color.a};
}

Vector3 terrain_normal(
    double leftHeight,
    double rightHeight,
    double nearHeight,
    double farHeight,
    double stepXMeters,
    double stepZMeters) {
    Vector3 normal {
        static_cast<float>(leftHeight - rightHeight),
        static_cast<float>(stepXMeters + stepZMeters),
        static_cast<float>(nearHeight - farHeight),
    };
    return Vector3Normalize(normal);
}

}  // namespace

Terrain::Terrain(TerrainParameters parameters)
    : parameters_(parameters) {
    shader_ = LoadShader(assets::kTerrainVertexShaderPath, assets::kTerrainFragmentShaderPath);
    shaderLoaded_ = shader_.id > 0;
    if (!shaderLoaded_) {
        return;
    }

    const Vector3 lightDirection {0.28f, -1.0f, 0.24f};
    const Vector3 lightColor {0.92f, 0.95f, 0.98f};
    const Vector3 ambientColor {0.34f, 0.36f, 0.40f};
    const float renderUnitsToMeters = 1.0f / render::kMetersToRenderScale;
    const float waterSurfaceMeters = static_cast<float>(parameters_.waterLevelMeters + parameters_.waterSurfaceOffsetMeters);

    const int lightDirLoc = GetShaderLocation(shader_, "lightDir");
    const int lightColorLoc = GetShaderLocation(shader_, "lightColor");
    const int ambientLoc = GetShaderLocation(shader_, "ambientColor");
    const int renderScaleLoc = GetShaderLocation(shader_, "renderUnitsToMeters");
    const int waterSurfaceLoc = GetShaderLocation(shader_, "waterSurfaceMeters");

    if (lightDirLoc >= 0) {
        SetShaderValue(shader_, lightDirLoc, &lightDirection, SHADER_UNIFORM_VEC3);
    }
    if (lightColorLoc >= 0) {
        SetShaderValue(shader_, lightColorLoc, &lightColor, SHADER_UNIFORM_VEC3);
    }
    if (ambientLoc >= 0) {
        SetShaderValue(shader_, ambientLoc, &ambientColor, SHADER_UNIFORM_VEC3);
    }
    if (renderScaleLoc >= 0) {
        SetShaderValue(shader_, renderScaleLoc, &renderUnitsToMeters, SHADER_UNIFORM_FLOAT);
    }
    if (waterSurfaceLoc >= 0) {
        SetShaderValue(shader_, waterSurfaceLoc, &waterSurfaceMeters, SHADER_UNIFORM_FLOAT);
    }
}

Terrain::~Terrain() {
    for (auto& [key, chunk] : chunkCache_) {
        (void)key;
        UnloadModel(chunk.terrainModel);
        if (chunk.hasWater) {
            UnloadModel(chunk.waterModel);
        }
    }

    if (shaderLoaded_) {
        UnloadShader(shader_);
    }
}

std::size_t Terrain::ChunkKeyHash::operator()(const ChunkKey& key) const noexcept {
    const std::uint64_t packedX = static_cast<std::uint32_t>(key.chunkX);
    const std::uint64_t packedZ = static_cast<std::uint32_t>(key.chunkZ);
    const std::uint64_t packedLod = static_cast<std::uint32_t>(key.lodLevel);
    return static_cast<std::size_t>((packedX * 73856093ULL) ^ (packedZ * 19349663ULL) ^ (packedLod * 83492791ULL));
}

double Terrain::sample_height_meters(double worldXMeters, double worldZMeters) const noexcept {
    const double continental = fractal_noise(worldXMeters * 0.00008, worldZMeters * 0.00008, 5, 2.0, 0.5);
    const double rolling = fractal_noise(worldXMeters * 0.00035, worldZMeters * 0.00035, 4, 2.1, 0.55);
    const double mountains = ridge_noise(worldXMeters * 0.00075, worldZMeters * 0.00075, 4, 2.05, 0.55);
    const double plateauMask = clamp((continental + 0.25) * 0.8, 0.0, 1.0);

    const double heightMeters = continental * 850.0
        + rolling * 220.0
        + mountains * 1'450.0 * plateauMask
        - 40.0;
    return clamp(heightMeters, parameters_.minElevationMeters, parameters_.maxElevationMeters);
}

void Terrain::draw(const Camera3D& camera) {
    const double cameraXMeters = camera.position.x / render::kMetersToRenderScale;
    const double cameraZMeters = camera.position.z / render::kMetersToRenderScale;
    const double viewRadiusMeters = visible_radius_meters(camera);
    const Vector3 cameraForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 horizontalForward = Vector3Normalize({cameraForward.x, 0.0f, cameraForward.z});

    prune_cache(cameraXMeters, cameraZMeters, viewRadiusMeters);

    const int chunkRadius = static_cast<int>(std::ceil(viewRadiusMeters / parameters_.chunkSizeMeters));
    const int centerChunkX = static_cast<int>(std::floor(cameraXMeters / parameters_.chunkSizeMeters));
    const int centerChunkZ = static_cast<int>(std::floor(cameraZMeters / parameters_.chunkSizeMeters));

    struct VisibleChunkCandidate {
        ChunkKey key;
        double distanceMeters = 0.0;
    };
    std::vector<VisibleChunkCandidate> visibleChunks;
    visibleChunks.reserve(static_cast<std::size_t>((chunkRadius * 2 + 1) * (chunkRadius * 2 + 1)));

    for (int dz = -chunkRadius; dz <= chunkRadius; ++dz) {
        for (int dx = -chunkRadius; dx <= chunkRadius; ++dx) {
            const int chunkX = centerChunkX + dx;
            const int chunkZ = centerChunkZ + dz;
            const double chunkCenterXMeters = (static_cast<double>(chunkX) + 0.5) * parameters_.chunkSizeMeters;
            const double chunkCenterZMeters = (static_cast<double>(chunkZ) + 0.5) * parameters_.chunkSizeMeters;
            const double distanceMeters = std::hypot(chunkCenterXMeters - cameraXMeters, chunkCenterZMeters - cameraZMeters);
            if (distanceMeters > viewRadiusMeters + parameters_.chunkSizeMeters) {
                continue;
            }

            const Vector3 toChunk = Vector3Normalize({
                static_cast<float>((chunkCenterXMeters - cameraXMeters) * render::kMetersToRenderScale),
                0.0f,
                static_cast<float>((chunkCenterZMeters - cameraZMeters) * render::kMetersToRenderScale),
            });
            const float forwardDot = Vector3DotProduct(horizontalForward, toChunk);
            if (distanceMeters > parameters_.chunkSizeMeters * 1.25 && forwardDot < -0.2f) {
                continue;
            }

            const int lodLevel = lod_level_for_distance(distanceMeters, viewRadiusMeters);
            visibleChunks.push_back({ChunkKey {chunkX, chunkZ, lodLevel}, distanceMeters});
        }
    }

    std::sort(
        visibleChunks.begin(),
        visibleChunks.end(),
        [](const VisibleChunkCandidate& left, const VisibleChunkCandidate& right) {
            return left.distanceMeters < right.distanceMeters;
        });

    std::size_t remainingBuildBudget = parameters_.maxChunkBuildsPerFrame;
    rlDisableBackfaceCulling();
    for (const VisibleChunkCandidate& candidate : visibleChunks) {
        auto it = chunkCache_.find(candidate.key);
        if (it == chunkCache_.end()) {
            if (remainingBuildBudget == 0) {
                continue;
            }

            auto [insertedIt, inserted] = chunkCache_.try_emplace(candidate.key);
            if (inserted) {
                insertedIt->second = build_chunk_mesh(candidate.key.chunkX, candidate.key.chunkZ, candidate.key.lodLevel);
                --remainingBuildBudget;
            }
            it = insertedIt;
        }

        DrawModel(it->second.terrainModel, Vector3Zero(), 1.0f, WHITE);
    }

    for (auto it = visibleChunks.rbegin(); it != visibleChunks.rend(); ++it) {
        const auto chunkIt = chunkCache_.find(it->key);
        if (chunkIt == chunkCache_.end() || !chunkIt->second.hasWater) {
            continue;
        }

        DrawModel(chunkIt->second.waterModel, Vector3Zero(), 1.0f, WHITE);
    }
    rlEnableBackfaceCulling();
}

double Terrain::visible_radius_meters(const Camera3D& camera) const noexcept {
    const double altitudeMeters = std::max(
        0.0,
        static_cast<double>(camera.position.y) / static_cast<double>(render::kMetersToRenderScale));
    return clamp(
        parameters_.baseVisibleRadiusMeters + altitudeMeters * parameters_.visibleRadiusPerAltitudeMeter,
        parameters_.baseVisibleRadiusMeters,
        parameters_.maxVisibleRadiusMeters);
}

int Terrain::lod_level_for_distance(double distanceMeters, double visibleRadiusMeters) const noexcept {
    for (std::size_t level = 0; level < parameters_.lodRadiusFractions.size(); ++level) {
        double levelThresholdMeters = std::min(
            visibleRadiusMeters * parameters_.lodRadiusFractions[level],
            parameters_.lodMaxDistanceMeters[level]);

        if (level == 0) {
            // Chunk LOD is selected from chunk centers, so the chunk containing the camera
            // needs a threshold at least as large as the half-diagonal of a chunk.
            const double containingChunkRadiusMeters = parameters_.chunkSizeMeters * std::sqrt(0.5);
            levelThresholdMeters = std::max(
                levelThresholdMeters,
                containingChunkRadiusMeters + parameters_.lodCellSizeMeters[level]);
        }

        if (distanceMeters <= levelThresholdMeters) {
            return static_cast<int>(level);
        }
    }

    return static_cast<int>(parameters_.lodRadiusFractions.size() - 1);
}

void Terrain::prune_cache(double cameraXMeters, double cameraZMeters, double visibleRadiusMeters) {
    const double keepRadiusMeters = visibleRadiusMeters + parameters_.chunkSizeMeters * 2.0;
    for (auto it = chunkCache_.begin(); it != chunkCache_.end();) {
        const double chunkCenterXMeters = (static_cast<double>(it->first.chunkX) + 0.5) * parameters_.chunkSizeMeters;
        const double chunkCenterZMeters = (static_cast<double>(it->first.chunkZ) + 0.5) * parameters_.chunkSizeMeters;
        const double distanceMeters = std::hypot(chunkCenterXMeters - cameraXMeters, chunkCenterZMeters - cameraZMeters);
        if (distanceMeters > keepRadiusMeters) {
            UnloadModel(it->second.terrainModel);
            if (it->second.hasWater) {
                UnloadModel(it->second.waterModel);
            }
            it = chunkCache_.erase(it);
        } else {
            ++it;
        }
    }
}

Terrain::ChunkMesh Terrain::build_chunk_mesh(int chunkX, int chunkZ, int lodLevel) const {
    const double cellSizeMeters = parameters_.lodCellSizeMeters[static_cast<std::size_t>(lodLevel)];
    const double waterCellSizeMeters = std::min(cellSizeMeters, parameters_.waterMaxCellSizeMeters);
    const int cellsPerSide = static_cast<int>(std::round(parameters_.chunkSizeMeters / cellSizeMeters));
    const int waterCellsPerSide = static_cast<int>(std::round(parameters_.chunkSizeMeters / waterCellSizeMeters));
    const double startXMeters = static_cast<double>(chunkX) * parameters_.chunkSizeMeters;
    const double startZMeters = static_cast<double>(chunkZ) * parameters_.chunkSizeMeters;
    const double skirtDepthMeters = parameters_.skirtDepthMeters + cellSizeMeters * 0.6;
    const double waterSurfaceMeters = parameters_.waterLevelMeters + parameters_.waterSurfaceOffsetMeters;
    const int verticesPerSide = cellsPerSide + 1;
    const int waterVerticesPerSide = waterCellsPerSide + 1;

    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<unsigned char> colors;
    std::vector<unsigned short> indices;

    const std::size_t topVertexCount = static_cast<std::size_t>(verticesPerSide * verticesPerSide);
    const std::size_t estimatedSkirtVertices = static_cast<std::size_t>(cellsPerSide * 16);
    vertices.reserve((topVertexCount + estimatedSkirtVertices) * 3);
    normals.reserve((topVertexCount + estimatedSkirtVertices) * 3);
    colors.reserve((topVertexCount + estimatedSkirtVertices) * 4);
    indices.reserve(static_cast<std::size_t>(cellsPerSide * cellsPerSide * 6 + cellsPerSide * 4 * 6));

    std::vector<double> heights(topVertexCount);
    std::vector<Color> topColors(topVertexCount);
    std::vector<Vector3> topVertices(topVertexCount);
    std::vector<double> waterDepths(topVertexCount);

    const auto grid_index = [verticesPerSide](int xIndex, int zIndex) {
        return static_cast<std::size_t>(zIndex * verticesPerSide + xIndex);
    };

    for (int zIndex = 0; zIndex <= cellsPerSide; ++zIndex) {
        const double zMeters = startZMeters + zIndex * cellSizeMeters;
        for (int xIndex = 0; xIndex <= cellsPerSide; ++xIndex) {
            const double xMeters = startXMeters + xIndex * cellSizeMeters;
            const std::size_t vertexIndex = grid_index(xIndex, zIndex);
            const double height = sample_height_meters(xMeters, zMeters);
            heights[vertexIndex] = height;
            topColors[vertexIndex] = color_for_height(height);
            topVertices[vertexIndex] = terrain_vertex(xMeters, height, zMeters);
            waterDepths[vertexIndex] = std::max(0.0, waterSurfaceMeters - height);
        }
    }

    for (int zIndex = 0; zIndex <= cellsPerSide; ++zIndex) {
        for (int xIndex = 0; xIndex <= cellsPerSide; ++xIndex) {
            const int leftX = std::max(xIndex - 1, 0);
            const int rightX = std::min(xIndex + 1, cellsPerSide);
            const int nearZ = std::max(zIndex - 1, 0);
            const int farZ = std::min(zIndex + 1, cellsPerSide);
            const double stepX = static_cast<double>(rightX - leftX) * cellSizeMeters;
            const double stepZ = static_cast<double>(farZ - nearZ) * cellSizeMeters;
            const std::size_t vertexIndex = grid_index(xIndex, zIndex);

            append_mesh_vertex(
                vertices,
                normals,
                colors,
                topVertices[vertexIndex],
                terrain_normal(
                    heights[grid_index(leftX, zIndex)],
                    heights[grid_index(rightX, zIndex)],
                    heights[grid_index(xIndex, nearZ)],
                    heights[grid_index(xIndex, farZ)],
                    stepX,
                    stepZ),
                topColors[vertexIndex]);
        }
    }

    for (int zIndex = 0; zIndex < cellsPerSide; ++zIndex) {
        for (int xIndex = 0; xIndex < cellsPerSide; ++xIndex) {
            const unsigned short i00 = static_cast<unsigned short>(grid_index(xIndex, zIndex));
            const unsigned short i10 = static_cast<unsigned short>(grid_index(xIndex + 1, zIndex));
            const unsigned short i01 = static_cast<unsigned short>(grid_index(xIndex, zIndex + 1));
            const unsigned short i11 = static_cast<unsigned short>(grid_index(xIndex + 1, zIndex + 1));

            append_triangle_indices(indices, i00, i01, i10);
            append_triangle_indices(indices, i10, i01, i11);
        }
    }

    const auto append_skirt_segment = [&](std::size_t topIndexA, std::size_t topIndexB) {
        const Vector3 topA = topVertices[topIndexA];
        const Vector3 topB = topVertices[topIndexB];
        const Vector3 bottomA {topA.x, topA.y - static_cast<float>(skirtDepthMeters * render::kMetersToRenderScale), topA.z};
        const Vector3 bottomB {topB.x, topB.y - static_cast<float>(skirtDepthMeters * render::kMetersToRenderScale), topB.z};
        const Vector3 skirtNormal = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(bottomA, topA),
            Vector3Subtract(topB, topA)));
        const Color skirtColorA = darken(topColors[topIndexA], 8);
        const Color skirtColorB = darken(topColors[topIndexB], 8);
        const unsigned short baseIndex = static_cast<unsigned short>(vertices.size() / 3);

        append_mesh_vertex(vertices, normals, colors, topA, skirtNormal, topColors[topIndexA]);
        append_mesh_vertex(vertices, normals, colors, topB, skirtNormal, topColors[topIndexB]);
        append_mesh_vertex(vertices, normals, colors, bottomA, skirtNormal, skirtColorA);
        append_mesh_vertex(vertices, normals, colors, bottomB, skirtNormal, skirtColorB);

        append_triangle_indices(indices, baseIndex, static_cast<unsigned short>(baseIndex + 2), static_cast<unsigned short>(baseIndex + 1));
        append_triangle_indices(indices, static_cast<unsigned short>(baseIndex + 1), static_cast<unsigned short>(baseIndex + 2), static_cast<unsigned short>(baseIndex + 3));
    };

    for (int xIndex = 0; xIndex < cellsPerSide; ++xIndex) {
        append_skirt_segment(grid_index(xIndex, 0), grid_index(xIndex + 1, 0));
        append_skirt_segment(grid_index(xIndex, cellsPerSide), grid_index(xIndex + 1, cellsPerSide));
    }

    for (int zIndex = 0; zIndex < cellsPerSide; ++zIndex) {
        append_skirt_segment(grid_index(0, zIndex), grid_index(0, zIndex + 1));
        append_skirt_segment(grid_index(cellsPerSide, zIndex), grid_index(cellsPerSide, zIndex + 1));
    }

    Mesh mesh {};
    mesh.vertexCount = static_cast<int>(vertices.size() / 3);
    mesh.triangleCount = static_cast<int>(indices.size() / 3);
    mesh.vertices = static_cast<float*>(MemAlloc(vertices.size() * sizeof(float)));
    mesh.normals = static_cast<float*>(MemAlloc(normals.size() * sizeof(float)));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(colors.size() * sizeof(unsigned char)));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(indices.size() * sizeof(unsigned short)));
    std::memcpy(mesh.vertices, vertices.data(), vertices.size() * sizeof(float));
    std::memcpy(mesh.normals, normals.data(), normals.size() * sizeof(float));
    std::memcpy(mesh.colors, colors.data(), colors.size() * sizeof(unsigned char));
    std::memcpy(mesh.indices, indices.data(), indices.size() * sizeof(unsigned short));
    UploadMesh(&mesh, false);

    ChunkMesh chunk;
    chunk.terrainModel = LoadModelFromMesh(mesh);
    if (shaderLoaded_) {
        chunk.terrainModel.materials[0].shader = shader_;
    }
    chunk.terrainModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;

    std::vector<float> waterVertices;
    std::vector<float> waterNormals;
    std::vector<unsigned char> waterColors;
    std::vector<unsigned short> waterIndices;
    const std::size_t waterVertexCount = static_cast<std::size_t>(waterVerticesPerSide * waterVerticesPerSide);
    waterVertices.reserve(waterVertexCount * 3);
    waterNormals.reserve(waterVertexCount * 3);
    waterColors.reserve(waterVertexCount * 4);
    waterIndices.reserve(static_cast<std::size_t>(waterCellsPerSide * waterCellsPerSide * 6));

    const auto water_color_for_depth = [&](double depthMeters) {
        if (depthMeters <= 0.0) {
            return Color {44, 96, 176, 0};
        }

        const double depthRatio = clamp(depthMeters / parameters_.waterDepthFadeMeters, 0.0, 1.0);
        const double totalWeight = std::max(0.001, parameters_.waterBlueWeight + parameters_.waterTerrainWeight);
        const double blueBlendWeight = clamp(parameters_.waterBlueWeight / totalWeight, 0.0, 1.0);
        const double attenuation = lerp(
            parameters_.waterMaxAttenuation,
            parameters_.waterMinAttenuation,
            std::sqrt(depthRatio));

        return Color {
            static_cast<unsigned char>(std::clamp(44.0 * attenuation, 0.0, 255.0)),
            static_cast<unsigned char>(std::clamp(96.0 * attenuation, 0.0, 255.0)),
            static_cast<unsigned char>(std::clamp(176.0 * attenuation, 0.0, 255.0)),
            static_cast<unsigned char>(std::clamp(255.0 * blueBlendWeight, 0.0, 255.0)),
        };
    };

    const auto water_grid_index = [waterVerticesPerSide](int xIndex, int zIndex) {
        return static_cast<std::size_t>(zIndex * waterVerticesPerSide + xIndex);
    };

    bool hasWater = false;
    for (int zIndex = 0; zIndex <= waterCellsPerSide; ++zIndex) {
        const double zMeters = startZMeters + zIndex * waterCellSizeMeters;
        for (int xIndex = 0; xIndex <= waterCellsPerSide; ++xIndex) {
            const double xMeters = startXMeters + xIndex * waterCellSizeMeters;
            const double terrainHeight = sample_height_meters(xMeters, zMeters);
            const Color waterColor = water_color_for_depth(std::max(0.0, waterSurfaceMeters - terrainHeight));
            hasWater = hasWater || waterColor.a > 0;

            append_mesh_vertex(
                waterVertices,
                waterNormals,
                waterColors,
                terrain_vertex(xMeters, waterSurfaceMeters, zMeters),
                Vector3 {0.0f, 1.0f, 0.0f},
                waterColor);
        }
    }

    if (hasWater) {
        for (int zIndex = 0; zIndex < waterCellsPerSide; ++zIndex) {
            for (int xIndex = 0; xIndex < waterCellsPerSide; ++xIndex) {
                const unsigned short i00 = static_cast<unsigned short>(water_grid_index(xIndex, zIndex));
                const unsigned short i10 = static_cast<unsigned short>(water_grid_index(xIndex + 1, zIndex));
                const unsigned short i01 = static_cast<unsigned short>(water_grid_index(xIndex, zIndex + 1));
                const unsigned short i11 = static_cast<unsigned short>(water_grid_index(xIndex + 1, zIndex + 1));

                const bool cellTouchesWater = waterColors[static_cast<std::size_t>(i00) * 4 + 3] > 0
                    || waterColors[static_cast<std::size_t>(i10) * 4 + 3] > 0
                    || waterColors[static_cast<std::size_t>(i01) * 4 + 3] > 0
                    || waterColors[static_cast<std::size_t>(i11) * 4 + 3] > 0;
                if (!cellTouchesWater) {
                    continue;
                }

                append_triangle_indices(waterIndices, i00, i01, i10);
                append_triangle_indices(waterIndices, i10, i01, i11);
            }
        }
    }

    if (hasWater && !waterIndices.empty()) {
        Mesh waterMesh {};
        waterMesh.vertexCount = static_cast<int>(waterVertices.size() / 3);
        waterMesh.triangleCount = static_cast<int>(waterIndices.size() / 3);
        waterMesh.vertices = static_cast<float*>(MemAlloc(waterVertices.size() * sizeof(float)));
        waterMesh.normals = static_cast<float*>(MemAlloc(waterNormals.size() * sizeof(float)));
        waterMesh.colors = static_cast<unsigned char*>(MemAlloc(waterColors.size() * sizeof(unsigned char)));
        waterMesh.indices = static_cast<unsigned short*>(MemAlloc(waterIndices.size() * sizeof(unsigned short)));
        std::memcpy(waterMesh.vertices, waterVertices.data(), waterVertices.size() * sizeof(float));
        std::memcpy(waterMesh.normals, waterNormals.data(), waterNormals.size() * sizeof(float));
        std::memcpy(waterMesh.colors, waterColors.data(), waterColors.size() * sizeof(unsigned char));
        std::memcpy(waterMesh.indices, waterIndices.data(), waterIndices.size() * sizeof(unsigned short));
        UploadMesh(&waterMesh, false);

        chunk.waterModel = LoadModelFromMesh(waterMesh);
        chunk.waterModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        chunk.hasWater = true;
    }

    return chunk;
}

double Terrain::fractal_noise(double x, double z, int octaves, double lacunarity, double gain) const noexcept {
    double amplitude = 1.0;
    double frequency = 1.0;
    double total = 0.0;
    double normalization = 0.0;

    for (int octave = 0; octave < octaves; ++octave) {
        total += perlin_noise(x * frequency, z * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return normalization > 0.0 ? total / normalization : 0.0;
}

double Terrain::ridge_noise(double x, double z, int octaves, double lacunarity, double gain) const noexcept {
    double amplitude = 1.0;
    double frequency = 1.0;
    double total = 0.0;
    double normalization = 0.0;

    for (int octave = 0; octave < octaves; ++octave) {
        const double ridge = 1.0 - std::abs(perlin_noise(x * frequency, z * frequency));
        total += ridge * ridge * amplitude;
        normalization += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return normalization > 0.0 ? total / normalization : 0.0;
}

double Terrain::perlin_noise(double x, double z) const noexcept {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const double tx = x - static_cast<double>(x0);
    const double tz = z - static_cast<double>(z0);

    const double n00 = gradient_noise(x0, z0, tx, tz);
    const double n10 = gradient_noise(x1, z0, tx - 1.0, tz);
    const double n01 = gradient_noise(x0, z1, tx, tz - 1.0);
    const double n11 = gradient_noise(x1, z1, tx - 1.0, tz - 1.0);

    const double sx = fade(tx);
    const double sz = fade(tz);
    return lerp(lerp(n00, n10, sx), lerp(n01, n11, sx), sz);
}

double Terrain::gradient_noise(int gridX, int gridZ, double offsetX, double offsetZ) const noexcept {
    const std::uint64_t hash = mix_bits(
        static_cast<std::uint64_t>(parameters_.noiseSeed)
        ^ mix_bits(static_cast<std::uint64_t>(static_cast<std::uint32_t>(gridX)) + 0x9e3779b97f4a7c15ULL)
        ^ mix_bits(static_cast<std::uint64_t>(static_cast<std::uint32_t>(gridZ)) + 0x632be59bd9b4e019ULL));
    const double angle = (static_cast<double>(hash & 0xffffU) / 65535.0) * kTwoPi;
    const double gradientX = std::cos(angle);
    const double gradientZ = std::sin(angle);
    return gradientX * offsetX + gradientZ * offsetZ;
}

Color Terrain::color_for_height(double heightMeters) const noexcept {
    const Color waterColor {36, 92, 182, 255};
    const Color lowGreen {164, 196, 118, 255};
    const Color darkGreen {56, 108, 58, 255};
    const Color darkBrown {92, 68, 44, 255};
    const Color lightBrown {164, 150, 132, 255};
    const Color snowWhite {244, 246, 250, 255};

    if (heightMeters < parameters_.waterLevelMeters) {
        return waterColor;
    }
    if (heightMeters < 100.0) {
        return lerp_color(lowGreen, darkGreen, heightMeters / 100.0);
    }
    if (heightMeters < 1'000.0) {
        return lerp_color(darkGreen, darkBrown, (heightMeters - 100.0) / 900.0);
    }
    if (heightMeters < 2'000.0) {
        return lerp_color(darkBrown, lightBrown, (heightMeters - 1'000.0) / 1'000.0);
    }

    return lerp_color(lightBrown, snowWhite, clamp((heightMeters - 2'000.0) / 500.0, 0.0, 1.0));
}

}  // namespace planet_aces