#include "BottomLevelBVH.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

#include "ScopeTimer.h"

#define MESH_USE_BVH  0
#define MESH_USE_SBVH 1

#define MESH_ACCELERATOR MESH_USE_SBVH

static std::unordered_map<std::string, BottomLevelBVH *> cache;

static int load_materials(BottomLevelBVH * bvh, std::vector<tinyobj::material_t> & materials, const char * path) {
	bvh->material_offset = Material::materials.size();

	// Load Materials
	int material_count = materials.size();
	if (material_count > 0) {
		for (int i = 0; i < material_count; i++) {
			const tinyobj::material_t & material = materials[i];

			Material new_material;
			new_material.diffuse = Vector3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);

			if (material.diffuse_texname.length() > 0) {
				new_material.texture = Texture::load((std::string(path) + material.diffuse_texname).c_str());
			}

			new_material.reflection = Vector3(material.specular[0], material.specular[1], material.specular[2]);

			new_material.transmittance       = Vector3(material.transmittance[0], material.transmittance[1], material.transmittance[2]);
			new_material.index_of_refraction = material.ior;

			Material::materials.push_back(new_material);
		}
	} else {
		material_count = 1;

		Material new_material;
		new_material.diffuse = Vector3(1.0f, 0.0f, 1.0f);

		Material::materials.push_back(new_material);
	}

	return material_count;
}

BottomLevelBVH * BottomLevelBVH::load(const char * filename) {
	BottomLevelBVH *& bvh = cache[filename];

	// If the cache already contains the requested BVH simply return it
	if (bvh) return bvh;

	bvh = new BottomLevelBVH();

	std::string bvh_filename = std::string(filename) + ".bvh";
	
	const char * path = Util::get_path(filename);

	if (std::filesystem::exists(bvh_filename)) {
		printf("Loading BVH %s from disk.\n", bvh_filename.c_str());

		bvh->load_from_disk(bvh_filename.c_str());

		// Load only the mtl file
		std::map<std::string, int> material_map;
		std::vector<tinyobj::material_t> materials;

		std::string warning;
		std::string error;

		std::string str(filename);

		std::filebuf fb;
		if (!fb.open(str.substr(0, str.length() - 4) + ".mtl", std::ios::in)) abort();
		std::istream is(&fb);

		tinyobj::LoadMtl(&material_map, &materials, &is, &warning, &error);

		load_materials(bvh, materials, path);
	} else {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		std::string warning;
		std::string error;

		bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, filename, path);
		if (!success) abort();

		int material_count = load_materials(bvh, materials, path);

		// Load Meshes
		int total_vertex_count = 0;
		int max_vertex_count = -1;

		// Count total amount of vertices over all Shapes
		for (int s = 0; s < shapes.size(); s++) {
			int vertex_count = shapes[s].mesh.indices.size();
			total_vertex_count += vertex_count;

			if (vertex_count > max_vertex_count) {
				max_vertex_count = vertex_count;
			}
		}

		Vector3 * positions  = new Vector3[max_vertex_count];
		Vector2 * tex_coords = new Vector2[max_vertex_count];
		Vector3 * normals    = new Vector3[max_vertex_count];

		int        triangle_count = total_vertex_count / 3;
		Triangle * triangles = new Triangle[triangle_count];

		bvh->init(triangle_count);
		
		int triangle_offset = 0;

		for (int s = 0; s < shapes.size(); s++) {
			int vertex_count = shapes[s].mesh.indices.size();
			assert(vertex_count % 3 == 0);

			// Iterate over vertices and assign attributes
			for (int v = 0; v < vertex_count; v++) {
				int vertex_index    = shapes[s].mesh.indices[v].vertex_index;
				int tex_coord_index = shapes[s].mesh.indices[v].texcoord_index;
				int normal_index    = shapes[s].mesh.indices[v].normal_index;
			
				if (vertex_index != INVALID) {
					positions[v] = Vector3(
						attrib.vertices[3*vertex_index    ], 
						attrib.vertices[3*vertex_index + 1], 
						attrib.vertices[3*vertex_index + 2]
					);
				}
				if (tex_coord_index != INVALID) {
					tex_coords[v] = Vector2(
							   attrib.texcoords[2*tex_coord_index    ], 
						1.0f - attrib.texcoords[2*tex_coord_index + 1] // Flip uv along y
					);
				}
				if (normal_index != INVALID) {
					normals[v] = Vector3(
						attrib.normals[3*normal_index    ], 
						attrib.normals[3*normal_index + 1], 
						attrib.normals[3*normal_index + 2]
					);
				}
			}

			// Iterate over faces
			for (int v = 0; v < vertex_count / 3; v++) {
				int index = triangle_offset + v;

				triangles[index].position0 = positions[3*v];
				triangles[index].position1 = positions[3*v + 1];
				triangles[index].position2 = positions[3*v + 2];

				triangles[index].calc_aabb();

				bvh->position0     [index] = positions[3*v];
				bvh->position_edge1[index] = positions[3*v + 1] - positions[3*v];
				bvh->position_edge2[index] = positions[3*v + 2] - positions[3*v];

				bvh->tex_coord0     [index] = tex_coords[3*v];
				bvh->tex_coord_edge1[index] = tex_coords[3*v + 1] - tex_coords[3*v];
				bvh->tex_coord_edge2[index] = tex_coords[3*v + 2] - tex_coords[3*v];

				bvh->normal0     [index] = normals[3*v];
				bvh->normal_edge1[index] = normals[3*v + 1] - normals[3*v];
				bvh->normal_edge2[index] = normals[3*v + 2] - normals[3*v];

				int material_id = shapes[s].mesh.material_ids[v];
				if (material_id == INVALID) material_id = 0;
			
				assert(material_id < material_count);

				bvh->material_id[index] = material_id;
			}
		
			triangle_offset += vertex_count / 3;
		}

		assert(triangle_offset == triangle_count);

		printf("Loaded Mesh %s from disk, consisting of %u triangles.\n", filename, triangle_count);
	
		delete [] positions;
		delete [] tex_coords;
		delete [] normals;
		
#if MESH_ACCELERATOR == MESH_USE_BVH
		{
			ScopeTimer timer("Mesh BVH Construction");
			bvh->build_bvh(triangles);
		}
#elif MESH_ACCELERATOR == MESH_USE_SBVH
		{
			ScopeTimer timer("Mesh SBVH Construction");
			bvh->build_sbvh(triangles);
		}
#endif

		delete [] triangles;

		bvh->save_to_disk(bvh_filename.c_str());
	}
	
	delete [] path;

	return bvh;
}

void BottomLevelBVH::init(int count) {
	assert(count > 0);

	primitive_count = count; 
	position0      = new Vector3[primitive_count];
	position_edge1 = new Vector3[primitive_count];
	position_edge2 = new Vector3[primitive_count];
		
	tex_coord0      = new Vector2[primitive_count];
	tex_coord_edge1 = new Vector2[primitive_count];
	tex_coord_edge2 = new Vector2[primitive_count];
		
	normal0      = new Vector3[primitive_count];
	normal_edge1 = new Vector3[primitive_count];
	normal_edge2 = new Vector3[primitive_count];

	material_id = new int[primitive_count];

	indices = nullptr;

	// Construct Node pool
	nodes = reinterpret_cast<BVHNode *>(ALLIGNED_MALLOC(2 * primitive_count * sizeof(BVHNode), 64));
	assert((unsigned long long)nodes % 64 == 0);
}

void BottomLevelBVH::build_bvh(const Triangle * triangles) {
	int * indices_x = new int[primitive_count];
	int * indices_y = new int[primitive_count];
	int * indices_z = new int[primitive_count];

	for (int i = 0; i < primitive_count; i++) {
		indices_x[i] = i;
		indices_y[i] = i;
		indices_z[i] = i;
	}

	std::sort(indices_x, indices_x + primitive_count, [&](int a, int b) { return triangles[a].get_position().x < triangles[b].get_position().x; });
	std::sort(indices_y, indices_y + primitive_count, [&](int a, int b) { return triangles[a].get_position().y < triangles[b].get_position().y; });
	std::sort(indices_z, indices_z + primitive_count, [&](int a, int b) { return triangles[a].get_position().z < triangles[b].get_position().z; });
		
	int * indices_xyz[3] = { indices_x, indices_y, indices_z };

	float * sah  = new float[primitive_count];
	int   * temp = new int[primitive_count];

	node_count = 2;
	BVHBuilders::build_bvh(nodes[0], triangles, indices_xyz, nodes, node_count, 0, primitive_count, sah, temp);

	assert(node_count <= 2 * primitive_count);

	index_count = primitive_count;

	// Use indices_x to index the Primitives array, and delete the other two
	indices = indices_x;
	delete [] indices_y;
	delete [] indices_z;

	delete [] temp;
	delete [] sah;
}

void BottomLevelBVH::build_sbvh(const Triangle * triangles) {
	const int overallocation = 2; // SBVH requires more space

	int * indices_x = new int[overallocation * primitive_count];
	int * indices_y = new int[overallocation * primitive_count];
	int * indices_z = new int[overallocation * primitive_count];

	for (int i = 0; i < primitive_count; i++) {
		indices_x[i] = i;
		indices_y[i] = i;
		indices_z[i] = i;
	}
		
	std::sort(indices_x, indices_x + primitive_count, [&](int a, int b) { return triangles[a].get_position().x < triangles[b].get_position().x; });
	std::sort(indices_y, indices_y + primitive_count, [&](int a, int b) { return triangles[a].get_position().y < triangles[b].get_position().y; });
	std::sort(indices_z, indices_z + primitive_count, [&](int a, int b) { return triangles[a].get_position().z < triangles[b].get_position().z; });

	int * indices_xyz[3] = { indices_x, indices_y, indices_z };
		
	float * sah     = new float[primitive_count];
	int   * temp[2] = { new int[primitive_count], new int[primitive_count] };

	AABB root_aabb = BVHPartitions::calculate_bounds(triangles, indices_xyz[0], 0, primitive_count);

	node_count = 2;
	index_count = BVHBuilders::build_sbvh(nodes[0], triangles, indices_xyz, nodes, node_count, 0, primitive_count, sah, temp, 1.0f / root_aabb.surface_area(), root_aabb);

	printf("SBVH Leaf count: %i\n", index_count);

	assert(node_count <= 2 * primitive_count);

	// Use indices_x to index the Primitives array, and delete the other two
	indices = indices_x;
	delete [] indices_y;
	delete [] indices_z;

	delete [] temp[0];
	delete [] temp[1];
	delete [] sah;
}

void BottomLevelBVH::save_to_disk(const char * bvh_filename) const {
	FILE * file;
	fopen_s(&file, bvh_filename, "wb");

	if (file == nullptr) abort();

	fwrite(&primitive_count, sizeof(int), 1, file);

	fwrite(position0,      sizeof(Vector3), primitive_count, file);
	fwrite(position_edge1, sizeof(Vector3), primitive_count, file);
	fwrite(position_edge2, sizeof(Vector3), primitive_count, file);

	fwrite(tex_coord0,      sizeof(Vector2), primitive_count, file);
	fwrite(tex_coord_edge1, sizeof(Vector2), primitive_count, file);
	fwrite(tex_coord_edge2, sizeof(Vector2), primitive_count, file);

	fwrite(normal0,      sizeof(Vector3), primitive_count, file);
	fwrite(normal_edge1, sizeof(Vector3), primitive_count, file);
	fwrite(normal_edge2, sizeof(Vector3), primitive_count, file);

	fwrite(material_id, sizeof(int), primitive_count, file);

	fwrite(&node_count, sizeof(int), 1, file);
	fwrite(nodes, sizeof(BVHNode), node_count, file);

	fwrite(&index_count, sizeof(int), 1, file);
		
	fwrite(indices, sizeof(int), index_count, file);

	fclose(file);
}

void BottomLevelBVH::load_from_disk(const char * bvh_filename) {
	FILE * file;
	fopen_s(&file, bvh_filename, "rb"); 
		
	if (file == nullptr) abort();

	fread(&primitive_count, sizeof(int), 1, file);
	init(primitive_count);

	fread(position0,      sizeof(Vector3), primitive_count, file);
	fread(position_edge1, sizeof(Vector3), primitive_count, file);
	fread(position_edge2, sizeof(Vector3), primitive_count, file);

	fread(tex_coord0,      sizeof(Vector2), primitive_count, file);
	fread(tex_coord_edge1, sizeof(Vector2), primitive_count, file);
	fread(tex_coord_edge2, sizeof(Vector2), primitive_count, file);

	fread(normal0,      sizeof(Vector3), primitive_count, file);
	fread(normal_edge1, sizeof(Vector3), primitive_count, file);
	fread(normal_edge2, sizeof(Vector3), primitive_count, file);

	fread(material_id, sizeof(int), primitive_count, file);
		
	fread(&node_count, sizeof(int), 1, file);

	fread(nodes, sizeof(BVHNode), node_count, file);

	fread(&index_count, sizeof(int), 1, file);
	
	indices = new int[index_count];
	fread(indices, sizeof(int), index_count, file);

	fclose(file);
}

void BottomLevelBVH::triangle_soa_trace(int index, const Ray & ray, RayHit & ray_hit, const Matrix4 & world, int bvh_step) const {
	const SIMD_float zero(0.0f);
	const SIMD_float one (1.0f);
	
	const SIMD_float pos_epsilon( Ray::EPSILON);
	const SIMD_float neg_epsilon(-Ray::EPSILON);

	SIMD_Vector3 edge0(position_edge1[index]);
	SIMD_Vector3 edge1(position_edge2[index]);

	SIMD_Vector3 h = SIMD_Vector3::cross(ray.direction, edge1);
	SIMD_float   a = SIMD_Vector3::dot(edge0, h);

	SIMD_float   f = SIMD_float::rcp(a);
	SIMD_Vector3 s = ray.origin - SIMD_Vector3(position0[index]);
	SIMD_float   u = f * SIMD_Vector3::dot(s, h);

	// If the barycentric coordinate on the edge between vertices i and i+1 
	// is outside the interval [0, 1] we know no intersection is possible
	SIMD_float mask = (u > zero) & (u < one);
	if (SIMD_float::all_false(mask)) return;

	SIMD_Vector3 q = SIMD_Vector3::cross(s, edge0);
	SIMD_float   v = f * SIMD_Vector3::dot(ray.direction, q);

	// If the barycentric coordinate on the edge between vertices i and i+2 
	// is outside the interval [0, 1] we know no intersection is possible
	mask = mask & (v       > zero);
	mask = mask & ((u + v) < one);
	if (SIMD_float::all_false(mask)) return;

	SIMD_float t = f * SIMD_Vector3::dot(edge1, q);

	// Check if we are in the right distance range
	mask = mask & (t > pos_epsilon);
	mask = mask & (t < ray_hit.distance);

	int int_mask = SIMD_float::mask(mask);
	if (int_mask == 0x0) return;
		
	ray_hit.hit      = ray_hit.hit | mask;
	ray_hit.distance = SIMD_float::blend(ray_hit.distance, t, mask);

	SIMD_Vector3 point  = Matrix4::transform_position(world, ray.origin + ray.direction * t);
	SIMD_Vector3 normal = Matrix4::transform_direction(world, 
		SIMD_Vector3::normalize(Math::barycentric(
			SIMD_Vector3(normal0[index]), 
			SIMD_Vector3(normal_edge1[index]), 
			SIMD_Vector3(normal_edge2[index]), 
		u, v))
	);

	ray_hit.point  = SIMD_Vector3::blend(ray_hit.point,  point,  mask);
	ray_hit.normal = SIMD_Vector3::blend(ray_hit.normal, normal, mask);

	// Obtain u,v by barycentric interpolation of the texture coordinates of the three current vertices
	SIMD_Vector3 tex_coord_a(Vector3(tex_coord0     [index].x, tex_coord0     [index].y, 1.0f));
	SIMD_Vector3 tex_coord_b(Vector3(tex_coord_edge1[index].x, tex_coord_edge1[index].y, 1.0f));
	SIMD_Vector3 tex_coord_c(Vector3(tex_coord_edge2[index].x, tex_coord_edge2[index].y, 1.0f));

	SIMD_Vector3 tex_coords = Math::barycentric(tex_coord_a, tex_coord_b, tex_coord_c, u, v);
	ray_hit.u = SIMD_float::blend(ray_hit.u, tex_coords.x, mask);
	ray_hit.v = SIMD_float::blend(ray_hit.v, tex_coords.y, mask);
		
	ray_hit.bvh_steps = SIMD_float::blend(ray_hit.bvh_steps, SIMD_float(bvh_step), mask);

	for (int j = 0; j < SIMD_LANE_SIZE; j++) {
		if (int_mask & (1 << j)) {
			ray_hit.material_id[j] = material_offset + material_id[index];
		}
	}
}

SIMD_float BottomLevelBVH::triangle_soa_intersect(int index, const Ray & ray, SIMD_float max_distance) const {
	const SIMD_float zero(0.0f);
	const SIMD_float one (1.0f);

	const SIMD_float pos_epsilon( Ray::EPSILON);
	const SIMD_float neg_epsilon(-Ray::EPSILON);

	SIMD_Vector3 edge0(position_edge1[index]);
	SIMD_Vector3 edge1(position_edge2[index]);

	SIMD_Vector3 h = SIMD_Vector3::cross(ray.direction, edge1);
	SIMD_float   a = SIMD_Vector3::dot(edge0, h);

	SIMD_float   f = SIMD_float::rcp(a);
	SIMD_Vector3 s = ray.origin - SIMD_Vector3(position0[index]);
	SIMD_float   u = f * SIMD_Vector3::dot(s, h);

	// If the barycentric coordinate on the edge between vertices i and i+1 
	// is outside the interval [0, 1] we know no intersection is possible
	SIMD_float mask = (u > zero) & (u < one);
	if (SIMD_float::all_false(mask)) return mask;

	SIMD_Vector3 q = SIMD_Vector3::cross(s, edge0);
	SIMD_float   v = f * SIMD_Vector3::dot(ray.direction, q);

	// If the barycentric coordinate on the edge between vertices i and i+2 
	// is outside the interval [0, 1] we know no intersection is possible
	mask = mask & (v       > zero);
	mask = mask & ((u + v) < one);
	if (SIMD_float::all_false(mask)) return mask;

	SIMD_float t = f * SIMD_Vector3::dot(edge1, q);

	// Check if we are in the right distance range
	mask = mask & (t > pos_epsilon);
	mask = mask & (t < max_distance);

	return mask;
}

void BottomLevelBVH::trace(const Ray & ray, RayHit & ray_hit, const Matrix4 & world) const {
	int stack[128];
	int stack_size = 1;

	// Push root on stack
	stack[0] = 0;

	int step = 0;

	while (stack_size > 0) {
		// Pop Node of the stack
		const BVHNode & node = nodes[stack[--stack_size]];

		SIMD_float mask = node.aabb.intersect(ray, ray_hit.distance);
		if (SIMD_float::all_false(mask)) continue;

		if (node.is_leaf()) {
			for (int i = node.first; i < node.first + node.count; i++) {
				triangle_soa_trace(indices[i], ray, ray_hit, world, step);
			}
		} else {
			if (node.should_visit_left_first(ray)) {
				stack[stack_size++] = node.left + 1;
				stack[stack_size++] = node.left;
			} else {
				stack[stack_size++] = node.left;
				stack[stack_size++] = node.left + 1;
			}
		}

		step++;
	}
}

SIMD_float BottomLevelBVH::intersect(const Ray & ray, SIMD_float max_distance) const {
	int stack[128];
	int stack_size = 1;

	// Push root on stack
	stack[0] = 0;

	int step = 0;

	SIMD_float hit(0.0f);

	while (stack_size > 0) {
		// Pop Node of the stack
		const BVHNode & node = nodes[stack[--stack_size]];

		SIMD_float mask = node.aabb.intersect(ray, max_distance);
		if (SIMD_float::all_false(mask)) continue;

		if (node.is_leaf()) {
			for (int i = node.first; i < node.first + node.count; i++) {
				hit = hit | triangle_soa_intersect(indices[i], ray, max_distance);

				if (SIMD_float::all_true(hit)) return hit;
			}
		} else {
			if (node.should_visit_left_first(ray)) {
				stack[stack_size++] = node.left + 1;
				stack[stack_size++] = node.left;
			} else {
				stack[stack_size++] = node.left;
				stack[stack_size++] = node.left + 1;
			}
		}

		step++;
	}

	return hit;
}
