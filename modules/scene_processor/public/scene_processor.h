#include <cstdint>
#include <vector>
#include <string>
#include <array>

#include <cppcoro/generator.hpp>

#include <microGUID.hpp>

typedef unsigned char stbi_uc;
namespace sp {
	enum class SceneNodeType : uint8_t {
		staticNode,
		dynamicNode,
		pointLight,
		staticMesh 
	};

	struct float3 {
		float v[3];
	};
	struct float4 {
		float v[4];
	};

	struct vertex_data {
		float3 position;
		float3 normal;
		float3 tangent;
		float3 bitangent;
		float3 color;
		float4 uv0_1;
	};

	struct AABB {
		float3 min;
		float3 max;
	};
	struct Mesh {

		float3* positions;
		vertex_data* data;
		uint32_t *indices;
		AABB bounds;
	};
	//opaque
	struct TextureID {
		uint64_t id;
	};
	struct MaterialID {
		uint64_t id;
	};
	struct Material {
		TextureID textures[8];
		MaterialID material;
		std::vector<char> material_data;
	};

	struct Matrix {
		float v[16];
		Matrix() = default;
		Matrix(float* vals) {
			for (int i = 0; i < 16; i++) {
				v[i] = vals[i];
			}
		}
	};

	struct Batch {
		std::vector<uint16_t> meshIDs;
		std::vector<Matrix> matrices;
		AABB bounds;
		
	};

	struct Scene {
		std::vector<Mesh> meshes;
		std::vector<Batch> batches;

		std::vector< Material> sceneMaterials;
	};

	struct DbNode {
		int id;
		std::string name;
		SceneNodeType type;
		Matrix transform;
	};
	struct DbNodeMesh: public DbNode {
		int meshID;
	};
	struct DbNodeLight : public DbNode {
		std::array<float,3> color;
	};
	struct DbMesh {
		guid::BinaryGUID guid;
		int id;
		std::string name;
		char* position_buffer;
		char* normals_buffer;
		char* uv0_buffer;
		std::vector<uint32_t> index_buffer;

		int num_vertices;
		int num_indices;
		int material;
	};
	//use this one as it controls memory of dbmesh properly
	struct ManagedMesh {
		DbMesh mesh;
		std::vector<char> memory_buffer;
		~ManagedMesh();
	};
	struct DbMaterial {
		struct TextureAssignement {
			int texture_slot;
			std::string texture_name;
			guid::BinaryGUID guid;
		};
		int id;
		std::string name;
		std::string json_metadata;
		std::vector<TextureAssignement> textures;
	};
	struct DbTexture {
		int id;
		guid::BinaryGUID guid;
		std::string name;
		std::string path;
		//stbi_uc* stb_pixels	{nullptr};
		void* data_raw{ nullptr };
		int size_x;
		int size_y;
		int channels;
		uint64_t byte_size;
		uint64_t vk_format;

	};

	struct SceneProcessConfig {
		Matrix rootMatrix;
		std::string database_name{"scene.db"};
		bool bLoadMeshes{false};
		bool bLoadNodes{ false };
		bool bLoadTextures{ false };
		bool bLoadMaterials{false};
	};

	struct SceneLoader {

		using LoadRequest = TextureID(const char*);

		static SceneLoader* Create();

		//set this for image load
		LoadRequest* texture_load_callback;

		void load_from_file(const char* scene_path, Matrix rootMatrix);


		virtual int open_db(const char* database_path) = 0;
		virtual int transform_scene(const char* scene_path,const SceneProcessConfig& config) = 0;
		virtual int load_textures_from_db(const char* scene_path, std::vector<DbTexture>& out_textures) = 0;
		virtual cppcoro::generator<DbTexture> load_all_textures()=0;
		virtual int load_materials_from_db(const char* scene_path, std::vector < DbMaterial > & out_materials) = 0;
		virtual int load_meshes_from_db(const char* scene_path, std::vector < ManagedMesh >& out_meshes) = 0;

		//virtual int load_nodes_from_db(const char* scene_path, std::vector < std::unique_ptr<DbNode> >& out_nodes) = 0;

		virtual int load_db_texture(std::string texture_name, DbTexture& outTexture) = 0;
		virtual int load_db_texture(std::string texture_name, void* outData) = 0;
	};
}

