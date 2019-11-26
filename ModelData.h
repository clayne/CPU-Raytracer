#pragma once
#include "Vector2.h"
#include "Vector3.h"
#include "Material.h"

struct MeshData {
	float   * position_x;
	float   * position_y;
	float   * position_z;
	Vector2 * tex_coords;
	Vector3 * normals;
	int       vertex_count;

	Material material;
};

struct ModelData {
	const MeshData * mesh_data       = nullptr;
	int              mesh_data_count = 0;

	static const ModelData * load(const char * file_path);
};
