#pragma once

#include <stdint.h>

#include <string>
#include <vector>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

struct alignas(16) Meshlet
{
	glm::vec3 center;
	float radius;
	int8_t cone_axis[3];
	int8_t cone_cutoff;

	uint32_t dataOffset; // dataOffset..dataOffset+vertexCount-1 stores vertex indices, we store indices packed in 4b units after that
	uint32_t baseVertex;
	uint8_t vertexCount;
	uint8_t triangleCount;
	uint8_t shortRefs;
	uint8_t padding;
};

struct alignas(16) Material
{
	int albedoTexture;
	int normalTexture;
	int specularTexture;
	int emissiveTexture;

	glm::vec4 diffuseFactor;
	glm::vec4 specularFactor;
	glm::vec3 emissiveFactor;
};

struct alignas(16) MeshDraw
{
	glm::vec3 position;
	float scale;
	glm::quat orientation;

	uint32_t meshIndex;
	uint32_t meshletVisibilityOffset;
	uint32_t postPass;
	uint32_t materialIndex;
};

struct Vertex
{
	uint16_t vx, vy, vz;
	uint16_t tp; // packed tangent: 8-8 octahedral
	uint32_t np; // packed normal: 10-10-10-2 vector + bitangent sign
	uint16_t tu, tv;
};

struct MeshLod
{
	uint32_t indexOffset;
	uint32_t indexCount;
	uint32_t meshletOffset;
	uint32_t meshletCount;
	float error;
};

struct alignas(16) Mesh
{
	glm::vec3 center;
	float radius;

	uint32_t vertexOffset;
	uint32_t vertexCount;

	uint32_t lodCount;
	MeshLod lods[8];
};

struct Geometry
{
	// TODO: remove these vectors - they are just scratch copies that waste space
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Meshlet> meshlets;
	std::vector<uint32_t> meshletdata;
	std::vector<Mesh> meshes;
};

struct Camera
{
	glm::vec3 position;
	glm::quat orientation;
	float fovY;
	float znear;
};

struct Keyframe
{
	glm::vec3 translation;
	float scale;
	glm::quat rotation;
};

struct Animation
{
	uint32_t drawIndex;

	float startTime;
	float period;
	std::vector<Keyframe> keyframes;
};
