#define NOMINMAX


#include "scene_processor.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>


#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <functional>

#include <nlohmann/json.hpp>

#include "stb_image.h"
#include <vulkan/vulkan.hpp>
#include <gli/gli.hpp>
#include <vk_format.h>
using namespace sp;
class RealSceneLoader : public  sp::SceneLoader {

public:


	
	void add_request_from_assimp(DbMaterial& mat, const aiScene* scene, aiMaterial* material, aiTextureType textype, const std::string& scenepath);

	void add_request_from_assimp(std::unordered_map<std::string, DbTexture>& texturemap, const aiScene* scene, aiMaterial* material, aiTextureType textype, const std::string& scenepath);
	virtual int transform_scene(const char* scene_path, const SceneProcessConfig& config)override;
	


	virtual int load_meshes_from_db(const char* scene_path, std::vector < ManagedMesh >& out_meshes) override;
	virtual int load_textures_from_db(const char* scene_path, std::vector<DbTexture>& out_textures)override;
	virtual int load_materials_from_db(const char* scene_path, std::vector<DbMaterial>& out_materials) override;


	virtual int open_db(const char* database_path) override;


	virtual int load_db_texture(std::string texture_name, DbTexture& outTexture) override;

	virtual int load_db_texture(std::string texture_name, void* outData) override;
	sqlite3* loaded_db;
	sqlite3_stmt* load_texture_query;


	std::vector<gli::texture*> gli_delete_queue;
	std::vector<stbi_uc*> stb_delete_queue;
};

sp::SceneLoader* sp::SceneLoader::Create()
{
	return new RealSceneLoader();
}

void sp::SceneLoader::load_from_file(const char* scene_path, Matrix rootMatrix)
{
}





static const char * query_create_node_table = R"(
DROP TABLE IF EXISTS SceneNodes;
CREATE TABLE SceneNodes(id INT PRIMARY KEY , name TEXT, type INT, metadata TEXT);
)";

static const char* query_insert_scene_node = R"(
INSERT INTO SceneNodes(id, name,metadata) VALUES (?,  ? , ?);
)";

static const char* query_create_mesh_table = R"(
DROP TABLE IF EXISTS Meshes;
CREATE TABLE Meshes(id INT PRIMARY KEY, 
	name TEXT, 
	num_vertices INT,
	num_indices INT,
	material INT, 
	position_buffer BLOB,
	normals_buffer BLOB,
	uv0_buffer BLOB,
	index_buffer BLOB);
)";

static const char* query_insert_mesh = R"(
INSERT INTO Meshes(id, name,num_vertices,num_indices,material,position_buffer,normals_buffer,uv0_buffer,index_buffer) VALUES (?, ? , ? , ?,?, ? , ? , ?,?);
)";

static const char* query_create_texture_table = R"(
DROP TABLE IF EXISTS Textures;
CREATE TABLE Textures(
	id INT , 
	name TEXT PRIMARY KEY, 
	size_x INT,
	size_y INT,	
	channels INT,
	pixels BLOB,
	metadata TEXT);
)";

static const char* query_insert_texture = R"(
INSERT INTO Textures(id, name,size_x,size_y,channels,pixels,metadata) VALUES (?, ? , ? , ?,?,?,?);
)";

static const char* query_create_material_table = R"(
DROP TABLE IF EXISTS Materials;
CREATE TABLE Materials(
	id INT , 
	name TEXT PRIMARY KEY, 	
	metadata TEXT);
)";

static const char* query_insert_material = R"(
INSERT INTO Materials(id, name,metadata) VALUES (?, ? , ?);
)";



int clear_db(sqlite3* db, const SceneProcessConfig & config)
{
	char* err_msg = 0;
	int rc;
	
	if (config.bLoadNodes) {
		rc = sqlite3_exec(db, query_create_node_table, 0, 0, &err_msg);

		if (rc != SQLITE_OK) {

			fprintf(stderr, "Failed to create table\n");
			fprintf(stderr, "SQL error: %s\n", err_msg);
			sqlite3_free(err_msg);

		}
		else {

			fprintf(stdout, "Table Friends created successfully\n");

		}
	}
	if (config.bLoadMeshes) {
		rc = sqlite3_exec(db, query_create_mesh_table, 0, 0, &err_msg);

		if (rc != SQLITE_OK) {

			fprintf(stderr, "Failed to create table\n");
			fprintf(stderr, "SQL error: %s\n", err_msg);
			sqlite3_free(err_msg);

		}
		else {

			fprintf(stdout, "Table Friends created successfully\n");
		}
	}
	if (config.bLoadTextures) {
		rc = sqlite3_exec(db, query_create_texture_table, 0, 0, &err_msg);

		if (rc != SQLITE_OK) {

			fprintf(stderr, "Failed to create table\n");
			fprintf(stderr, "SQL error: %s\n", err_msg);
			sqlite3_free(err_msg);

		}
		else {

			fprintf(stdout, "Table Textures created successfully\n");
		}
	}
	if (config.bLoadMaterials) {
		rc = sqlite3_exec(db, query_create_material_table, 0, 0, &err_msg);

		if (rc != SQLITE_OK) {

			fprintf(stderr, "Failed to create table\n");
			fprintf(stderr, "SQL error: %s\n", err_msg);
			sqlite3_free(err_msg);

		}
		else {

			fprintf(stdout, "Table Materials created successfully\n");
		}
	}

	return rc;
}

std::string nodetype_to_string(SceneNodeType type) {
	switch (type) {
	case SceneNodeType::staticNode:
		return "STATIC_NODE";
		break;
	case SceneNodeType::dynamicNode:
		return "DYNAMIC_NODE";
		break;
	case SceneNodeType::pointLight:
		return "LIGHT_POINT";
		break;
	case SceneNodeType::staticMesh:
		return "STATIC_MESH";
		break;	
	}
	return "ERROR";
}

int insert_scene_node(sqlite3* db,DbNode* node)
{
	int id = node->id;
	std::string name = node->name;
	const Matrix& transform = node->transform;

	using nlohmann::json;

	sqlite3_stmt* res;
	int rc =sqlite3_prepare_v2(db, query_insert_scene_node, -1, &res, 0);

	if (rc == SQLITE_OK) {
		sqlite3_bind_int(res, 1, id);
		sqlite3_bind_text(res, 2, name.c_str() , name.size(), SQLITE_TRANSIENT);
		//sqlite3_bind_int(res, 3, type);
		//sqlite3_bind_blob(res, 4, &transform , sizeof(Matrix), SQLITE_TRANSIENT);

		std::array<float,16> matrix;
		
		for (int i = 0; i < 16; i++) {
			matrix[i] = transform.v[i];
		}

		json metadata;		
	
		metadata["object-type"] = nodetype_to_string(node->type);
		metadata["object-type-n"] = (uint8_t)node->type;
		metadata["world-matrix"] = matrix;

		switch (node->type) {
			case SceneNodeType::staticMesh:
				metadata["mesh-id"] = ((DbNodeMesh*)node)->meshID;
				break;
			case SceneNodeType::pointLight:
				metadata["light-color"] = ((DbNodeLight*)node)->color;
				break;
		}

		std::string mtstring = metadata.dump();

		sqlite3_bind_text(res, 3, mtstring.c_str(), mtstring.size(), SQLITE_TRANSIENT);
		rc = sqlite3_step(res);

		if (rc != SQLITE_DONE) {

			printf("execution failed: %s", sqlite3_errmsg(db));
		}

		sqlite3_finalize(res);

		return 0;
	}
	else {

		fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
		return 1;
	}
}

int insert_meshes(sqlite3* db, std::vector<DbMesh> &meshes)
{
	sqlite3_stmt* res;
	int rc = sqlite3_prepare_v2(db, query_insert_mesh, -1, &res, 0);

	if (rc == SQLITE_OK) {
		for (int i = 0; i < meshes.size(); i++) {
			const DbMesh &m = meshes[i];

			sqlite3_bind_int(res, 1, i);
			sqlite3_bind_text(res, 2, m.name.c_str(), m.name.size(), SQLITE_TRANSIENT);

			sqlite3_bind_int(res, 3, m.num_vertices);
			sqlite3_bind_int(res, 4, m.num_indices);
			sqlite3_bind_int(res, 5, m.material);

			sqlite3_bind_blob(res, 6,m.position_buffer, sizeof(float) * 3 * m.num_vertices, SQLITE_TRANSIENT);
			sqlite3_bind_blob(res, 7,m.normals_buffer, sizeof(float) * 3 * m.num_vertices, SQLITE_TRANSIENT);

			if (m.uv0_buffer != nullptr) {
				sqlite3_bind_blob(res, 8, m.uv0_buffer, sizeof(float) * 2 * m.num_vertices, SQLITE_TRANSIENT);
			}
			else {
				sqlite3_bind_blob(res, 8, nullptr, 0, SQLITE_TRANSIENT);
			}
			

			sqlite3_bind_blob(res, 9,m.index_buffer.data(), m.index_buffer.size() * sizeof(uint32_t), SQLITE_TRANSIENT);

			rc = sqlite3_step(res);

			if (rc != SQLITE_DONE) {

				printf("execution failed: %s", sqlite3_errmsg(db));
				return 1;
			}

			sqlite3_reset(res);
		}		

		sqlite3_finalize(res);

		return 0;
	}
	else {

		fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
		return 1;
	}
}

vk::Format image_format_from_stbi(int channels) {
	switch (channels) {
	case 1:
		return vk::Format::eR8Unorm;
		break;
	case 2:
		return vk::Format::eR8G8Unorm;
		break;
	case 3:
		return vk::Format::eR8G8B8Unorm;
		break;
	case 4:
		return vk::Format::eR8G8B8A8Unorm;
		break;
	}
	return vk::Format{};
};

int insert_textures(sqlite3* db, std::vector<DbTexture>& textures)
{
	using nlohmann::json;

	sqlite3_stmt* res;
	int rc = sqlite3_prepare_v2(db, query_insert_texture, -1, &res, 0);

	if (rc == SQLITE_OK) {
		for (int i = 0; i < textures.size(); i++) {
			const DbTexture& m = textures[i];

			sqlite3_bind_int(res, 1, i);
			sqlite3_bind_text(res, 2, m.name.c_str(), m.name.size(), SQLITE_TRANSIENT);

			sqlite3_bind_int(res, 3, m.size_x);
			sqlite3_bind_int(res, 4, m.size_y);
			sqlite3_bind_int(res, 5, m.channels);
			sqlite3_bind_blob(res, 6, m.data_raw, m.byte_size, SQLITE_TRANSIENT);

			json metadata;

			metadata["original-path"] = m.path;		
			metadata["format"] = to_string(vk::Format{ m.vk_format });
			metadata["format-n"] = m.vk_format;
			metadata["blob_size"] = m.byte_size;
			std::string mtstring = metadata.dump();

			sqlite3_bind_text(res, 7, mtstring.c_str(), mtstring.size(), SQLITE_TRANSIENT);

			rc = sqlite3_step(res);

			if (rc != SQLITE_DONE) {

				printf("execution failed: %s", sqlite3_errmsg(db));
				return 1;
			}

			sqlite3_reset(res);
		}

		sqlite3_finalize(res);

		return 0;
	}
	else {

		fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
		return 1;
	}
}


int insert_materials(sqlite3* db, std::vector<DbMaterial>& materials)
{
	using nlohmann::json;

	sqlite3_stmt* res;
	int rc = sqlite3_prepare_v2(db, query_insert_material, -1, &res, 0);

	if (rc == SQLITE_OK) {
		for (int i = 0; i < materials.size(); i++) {
			const DbMaterial& m = materials[i];

			sqlite3_bind_int(res, 1, i);
			sqlite3_bind_text(res, 2, m.name.c_str(), m.name.size(), SQLITE_TRANSIENT);

		
		
			json metadata;

			json texList;
			for (auto t : m.textures) {
				json tex;
				tex["name"] = t.texture_name;
				tex["slot"] = t.texture_slot;
				texList.push_back(tex);
			}

			metadata["texture_bindings"] = texList;
			
			std::string mtstring = metadata.dump();

			sqlite3_bind_text(res, 3, mtstring.c_str(), mtstring.size(), SQLITE_TRANSIENT);

			rc = sqlite3_step(res);

			if (rc != SQLITE_DONE) {

				printf("execution failed: %s", sqlite3_errmsg(db));
				return 1;
			}

			sqlite3_reset(res);
		}

		sqlite3_finalize(res);

		return 0;
	}
	else {

		fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
		return 1;
	}
}

void RealSceneLoader::add_request_from_assimp(std::unordered_map<std::string,DbTexture> &texturemap, const aiScene* scene, aiMaterial* material, aiTextureType textype,
	const std::string& scenepath) 
{
	aiString assimppath;
	if (material->GetTextureCount(textype))
	{
		material->GetTexture(textype, 0, &assimppath);

		const char* texname = &assimppath.data[0];
		char* ch = &assimppath.data[1];

		for (int i = 0; i < assimppath.length; i++)
		{
			if (assimppath.data[i] == '\\')
			{
				assimppath.data[i] = '/';
			}
		}
		std::filesystem::path texture_path{ texname };

		std::string texpath = scenepath + "/" + texture_path.string();
		
		auto load_entity = texturemap.find(texpath);
		if (load_entity == texturemap.end()) {
			std::cout << "loading texture" << texpath << std::endl;

			if (auto texture = scene->GetEmbeddedTexture(assimppath.C_Str())) {
				size_t tex_size = texture->mHeight * texture->mWidth;
				if (texture->mHeight == 0) {
					tex_size = texture->mWidth;
				}
				int x, y, c;
				
				DbTexture tex;
				tex.name = texname;
				tex.path = texpath;
				tex.data_raw = stbi_load_from_memory((stbi_uc*)texture->pcData, tex_size, &tex.size_x, &tex.size_y, &tex.channels, 0);
				//rgb format is not supported in almost any gpu, re-open as 4 channels
				if (tex.data_raw && tex.channels == STBI_rgb) {
					stbi_image_free(tex.data_raw);
					tex.data_raw = stbi_load_from_memory((stbi_uc*)texture->pcData, tex_size, &tex.size_x, &tex.size_y, &tex.channels, STBI_rgb_alpha);
					tex.channels = 4;
				}

				if (tex.data_raw) {
					tex.vk_format =(uint64_t) image_format_from_stbi(tex.channels);
					tex.byte_size = tex.size_x * tex.size_y * tex.channels;
					stb_delete_queue.push_back((stbi_uc*)tex.data_raw);
					tex.id = texturemap.size();
					texturemap[texpath] = tex;
				}

				
			}
			else {

				gli::texture *textu = new gli::texture();
				
				this->gli_delete_queue.push_back(textu);

				*textu = gli::load(texpath);
				gli::gl GL(gli::gl::PROFILE_GL33);
				gli::gl::format const Format = GL.translate(textu->format(), textu->swizzles());

				VkFormat format = vkGetFormatFromOpenGLInternalFormat(Format.Internal);

				DbTexture tex;
				tex.name = texname;
				tex.path = texpath;
				tex.data_raw = textu->data();
				tex.byte_size = textu->size();
				tex.channels = 4;	
				tex.vk_format = format;

				tex.id = texturemap.size();
				tex.size_x = textu->extent().x;
				tex.size_y = textu->extent().y;
				texturemap[texpath] = tex;
				



				//image_format = vk::Format(format);
				std::cout << "GLI loading texture: " << texpath  << std::endl;
			}
		}
	}
}


void RealSceneLoader::add_request_from_assimp(DbMaterial& mat, const aiScene* scene, aiMaterial* material, aiTextureType textype, const std::string& scenepath)
{
	aiString texpath;
	if (material->GetTextureCount(textype))
	{
		material->GetTexture(textype, 0, &texpath);

		const char* texname = &texpath.data[0];
		for (int i = 0; i < texpath.length; i++)
		{
			if (texpath.data[i] == '\\')
			{
				texpath.data[i] = '/';
			}
		}
		DbMaterial::TextureAssignement assignement;
		assignement.texture_name = texname;
		assignement.texture_slot = textype;
		mat.textures.push_back(assignement);
	}
}

int RealSceneLoader::transform_scene(const char* scene_path, const SceneProcessConfig& config)
{
	sqlite3* db;
	char* err_msg = 0;
	sqlite3_stmt* res;

	Matrix rootMatrix = config.rootMatrix;
	int rc = sqlite3_open(config.database_name.c_str(), &db);

	if (rc != SQLITE_OK) {

		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);

		return 1;
	}

	clear_db(db,config);


	auto start1 = std::chrono::system_clock::now();

	std::filesystem::path sc_path{ std::string(scene_path) };
	Assimp::Importer importer;
	const aiScene* scene;
	{
		//ZoneScopedNC("Assimp load", tracy::Color::Magenta);
		scene = importer.ReadFile(scene_path, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_GenBoundingBoxes); //aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_GenBoundingBoxes);
	}


	auto end = std::chrono::system_clock::now();
	auto elapsed = end - start1;
	std::cout << "Assimp load time " << elapsed.count() << '\n';
	start1 = std::chrono::system_clock::now();

	if (config.bLoadMeshes) {
		std::vector<DbMesh> loaded_meshes;
		for (int i = 0; i < scene->mNumMeshes; i++)
		{
			auto mesh = scene->mMeshes[i];

			DbMesh m;

			m.num_indices = mesh->mNumFaces * 3;
			m.num_vertices = mesh->mNumVertices;
			m.id = i;
			m.material = mesh->mMaterialIndex;
			m.name = mesh->mName.C_Str();

			m.position_buffer = (char*)mesh->mVertices;
			m.normals_buffer = (char*)mesh->mNormals;
			m.uv0_buffer = nullptr;
			if (mesh->HasTextureCoords(0)) {
				m.uv0_buffer = (char*)mesh->mTextureCoords[0];
			}
			m.index_buffer.reserve(m.num_indices);

			for (int face = 0; face < mesh->mNumFaces; face++) {
				m.index_buffer.push_back(mesh->mFaces[face].mIndices[0]);
				m.index_buffer.push_back(mesh->mFaces[face].mIndices[1]);
				m.index_buffer.push_back(mesh->mFaces[face].mIndices[2]);
			}

			loaded_meshes.push_back(m);
		}
		insert_meshes(db, loaded_meshes);
	}
	


	if (config.bLoadTextures)
	{
		std::string scenepath = sc_path.parent_path().string();
		std::unordered_map<std::string, DbTexture> texturemap;
		for (int i = 0; i < scene->mNumMaterials; i++) {
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_DIFFUSE, scenepath);
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_NORMALS, scenepath);
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_SPECULAR, scenepath);
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_METALNESS, scenepath);
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_EMISSIVE, scenepath);
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_OPACITY, scenepath);
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_DIFFUSE_ROUGHNESS, scenepath);
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_EMISSION_COLOR, scenepath);
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_BASE_COLOR, scenepath);
			add_request_from_assimp(texturemap, scene, scene->mMaterials[i], aiTextureType_UNKNOWN, scenepath);
		}
		std::vector<DbTexture> textures;
		textures.reserve(texturemap.size());
		for (auto [K, V] : texturemap) {
			textures.push_back(V);			
		}

		insert_textures(db, textures);

		for (auto p : this->gli_delete_queue) {
			delete p;
		}
		for (auto pix : this->stb_delete_queue) {
			stbi_image_free(pix);
		}
	}

	if (config.bLoadMaterials)
	{
		std::string scenepath = sc_path.parent_path().string();
		std::vector<DbMaterial> allMaterials;
		allMaterials.reserve(scene->mNumMaterials);
		for (int i = 0; i < scene->mNumMaterials; i++) {
			DbMaterial mat;
			mat.id = i;
			const char* matname = &scene->mMaterials[i]->GetName().data[0];
			mat.name = matname;

			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_DIFFUSE, scenepath);
			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_NORMALS, scenepath);
			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_SPECULAR, scenepath);
			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_METALNESS, scenepath);
			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_EMISSIVE, scenepath);
			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_OPACITY, scenepath);
			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_DIFFUSE_ROUGHNESS, scenepath);
			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_EMISSION_COLOR, scenepath);
			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_BASE_COLOR, scenepath);
			add_request_from_assimp(mat, scene, scene->mMaterials[i], aiTextureType_UNKNOWN, scenepath);

			allMaterials.push_back(mat);
		}

		insert_materials(db, allMaterials);
	}
	
	std::unordered_map<std::string, Matrix> node_matrices;
	int node_id = 0;
	std::function<void(aiNode * node, aiMatrix4x4 & parentmat)> process_node = [&](aiNode* node, aiMatrix4x4& parentmat) {

		aiMatrix4x4 node_mat = parentmat* node->mTransformation;

		glm::mat4 modelmat;
		for (int y = 0; y < 4; y++)
		{
			for (int x = 0; x < 4; x++)
			{
				modelmat[y][x] = node_mat[x][y];
			}
		}

		std::string nodename =node->mName.C_Str();

		DbNode dbNode;
		dbNode.id = node_id;
		dbNode.type = SceneNodeType::staticNode;
		dbNode.transform = Matrix(&modelmat[0][0]);
		dbNode.name = nodename;

		insert_scene_node(db, &dbNode);
		node_matrices[nodename] = dbNode.transform;
		node_id++;

		for (int msh = 0; msh < node->mNumMeshes; msh++) {
		
			int mesh_index = node->mMeshes[msh];
			std::string meshname = "Mesh:" + nodename + scene->mMeshes[mesh_index]->mName.C_Str();

			DbNodeMesh dbNodeMesh;
			dbNodeMesh.id = node_id;
			dbNodeMesh.type = SceneNodeType::staticMesh;
			dbNodeMesh.transform = Matrix(&modelmat[0][0]);
			dbNodeMesh.name = meshname;
			dbNodeMesh.meshID = mesh_index;

			insert_scene_node(db, &dbNodeMesh);
			node_id++;
		}
		
		for (int ch = 0; ch < node->mNumChildren; ch++)
		{
			process_node(node->mChildren[ch], node_mat);
		}
	};

	aiMatrix4x4 mat{};
	glm::mat4 rootmat;// (, rootMatrix.v);
	memcpy(&rootmat, rootMatrix.v, sizeof(Matrix));
	for (int y = 0; y < 4; y++)
	{
		for (int x = 0; x < 4; x++)
		{
			mat[x][y] = rootmat[y][x];
		}
	}
	if(config.bLoadNodes)
	{
		//ZoneScopedNC("Node transform Processing", tracy::Color::Blue);
		process_node(scene->mRootNode, mat);
		for (int l = 0; l < scene->mNumLights; l++) {

			aiLight* light = scene->mLights[l];

			std::string nodename = light->mName.C_Str();
			std::string lightname = "Light:" + nodename;

			DbNodeLight lightNode;
			lightNode.id = node_id;
			lightNode.type = SceneNodeType::pointLight;
			lightNode.transform = node_matrices[nodename];
			lightNode.name = lightname;
			lightNode.color[0] = light->mColorDiffuse.r;
			lightNode.color[1] = light->mColorDiffuse.g;
			lightNode.color[2] = light->mColorDiffuse.b;

			insert_scene_node(db, &lightNode);
			node_id++;
			
		}
		

		//std::cout << nodemeshes << "   " << scene->mNumMeshes;
	}
	//Matrix mat;
	//mat.v[0] = 6.f;
	//insert_scene_node(db, "test", 42, rootMatrix);


	sqlite3_close(db);
	end = std::chrono::system_clock::now();
	elapsed = end - start1;
	std::cout << "DB load time " << elapsed.count() << '\n';
	return 0;
}

int callback(void *, int, char **, char **);

int RealSceneLoader::load_meshes_from_db(const char* scene_path, std::vector < ManagedMesh >& out_meshes)
{
	using nlohmann::json;

	sqlite3_stmt* pStmt;
	char* err_msg = 0;


	char* sql = "SELECT size_x,size_y,channels,name,metadata FROM Textures";

	int error = sqlite3_prepare_v2(loaded_db, sql, -1, &pStmt, 0);
	if (error != SQLITE_OK) {

		fprintf(stderr, "Failed to prepare statement\n");
		return 1;
	}
	while (true) {
		int rc = sqlite3_step(pStmt);

		int bytes = 0;

		if (rc == SQLITE_ROW) {

			//DbTexture outTexture;
			//
			////outTexture.buffer_size = sqlite3_column_bytes(pStmt, 0);			
			//outTexture.size_x = sqlite3_column_int(pStmt, 0);
			//outTexture.size_y = sqlite3_column_int(pStmt, 1);
			//outTexture.channels = sqlite3_column_int(pStmt, 2);
			//auto tx = sqlite3_column_text(pStmt, 3);
			//auto meta = sqlite3_column_text(pStmt, 4);
			//
			//json metadata = json::parse(meta);
			//
			//outTexture.path = metadata["original-path"];
			//outTexture.vk_format = metadata["format-n"];
			//outTexture.byte_size = metadata["blob_size"];
			//
			//outTexture.name = reinterpret_cast<const char*>(tx);
			//outTexture.data_raw = nullptr;
			//
			//out_textures.push_back(outTexture);
		}
		else if (rc == SQLITE_DONE)
		{
			break;
		}
		else {
			sqlite3_finalize(pStmt);
			return 1;
		}

	}

	sqlite3_finalize(pStmt);

	return 0;
}
int RealSceneLoader::load_textures_from_db(const char* scene_path, std::vector<DbTexture>& out_textures)
{
	using nlohmann::json;

	sqlite3_stmt* pStmt;
	char* err_msg = 0;


	char* sql = "SELECT size_x,size_y,channels,name,metadata FROM Textures";

	int error = sqlite3_prepare_v2(loaded_db, sql, -1, &pStmt, 0);
	if (error != SQLITE_OK) {

		fprintf(stderr, "Failed to prepare statement\n");
		return 1;
	}
	while (true) {
		int rc = sqlite3_step(pStmt);

		int bytes = 0;

		if (rc == SQLITE_ROW) {

			DbTexture outTexture;

			//outTexture.buffer_size = sqlite3_column_bytes(pStmt, 0);			
			outTexture.size_x = sqlite3_column_int(pStmt, 0);
			outTexture.size_y = sqlite3_column_int(pStmt, 1);
			outTexture.channels = sqlite3_column_int(pStmt, 2);
			auto tx = sqlite3_column_text(pStmt, 3);
			auto meta = sqlite3_column_text(pStmt, 4);

			json metadata = json::parse( meta );

			outTexture.path = metadata["original-path"];
			outTexture.vk_format = metadata["format-n"];
			outTexture.byte_size = metadata["blob_size"];

			outTexture.name = reinterpret_cast<const char*>(tx);
			outTexture.data_raw = nullptr;

			out_textures.push_back(outTexture);
		}
		else if(rc == SQLITE_DONE)
		{ break; }
		else {
			sqlite3_finalize(pStmt);
			return 1;
		}

	}

	sqlite3_finalize(pStmt);

	return 0;
}

int RealSceneLoader::load_materials_from_db(const char* scene_path, std::vector<DbMaterial>& out_materials)
{
	using nlohmann::json;

	sqlite3_stmt* pStmt;
	char* err_msg = 0;


	char* sql = "SELECT name,metadata FROM Materials";

	int error = sqlite3_prepare_v2(loaded_db, sql, -1, &pStmt, 0);
	if (error != SQLITE_OK) {

		fprintf(stderr, "Failed to prepare statement\n");
		return 1;
	}
	while (true) {
		int rc = sqlite3_step(pStmt);

		int bytes = 0;

		if (rc == SQLITE_ROW) {

			DbMaterial outMaterial;
						
			
			auto tx = sqlite3_column_text(pStmt, 0);
			auto meta = sqlite3_column_text(pStmt, 1);

			json metadata = json::parse(meta);

			

			json texList = metadata["texture_bindings"];
			
			for (auto t : texList) {
				
				DbMaterial::TextureAssignement tex;
				tex.texture_name = t["name"] ;
				tex.texture_slot = t["slot"];
				outMaterial.textures.push_back(tex);
			}

			outMaterial.json_metadata = reinterpret_cast<const char*>(meta);
			outMaterial.name = reinterpret_cast<const char*>(tx);			

			out_materials.push_back(outMaterial);
		}
		else if (rc == SQLITE_DONE)
		{
			break;
		}
		else {
			sqlite3_finalize(pStmt);
			return 1;
		}

	}

	sqlite3_finalize(pStmt);

	return 0;
}


int callback(void* NotUsed, int argc, char** argv,
	char** azColName) {

	NotUsed = 0;

	for (int i = 0; i < argc; i++) {

		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}

	printf("\n");

	return 0;
}


int RealSceneLoader::open_db(const char* database_path)
{
	sqlite3* db;
	char* err_msg = 0;

	int rc = sqlite3_open_v2(database_path, &db,SQLITE_OPEN_READWRITE,nullptr);

	if (rc != SQLITE_OK) {

		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);

		return 1;
	}

	loaded_db = db;
	char* sql = "SELECT pixels,size_x,size_y FROM Textures WHERE name = ?";

	sqlite3_stmt* pStmt;
	rc = sqlite3_prepare_v2(db, sql, -1, &pStmt, 0);

	if (rc != SQLITE_OK) {

		fprintf(stderr, "Failed to prepare statement\n");
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));

		//sqlite3_close(db);

		return 1;
	}

	load_texture_query = pStmt;

	return 0;
}


int RealSceneLoader::load_db_texture(std::string texture_name, DbTexture& outTexture) 
{
	sqlite3_bind_text(load_texture_query, 1, texture_name.c_str(), texture_name.size(), SQLITE_TRANSIENT);

	int rc = sqlite3_step(load_texture_query);

	int bytes = 0;

	if (rc == SQLITE_ROW) {

		bytes = sqlite3_column_bytes(load_texture_query, 0);

		const void* ptr = sqlite3_column_blob(load_texture_query, 0);
		outTexture.size_x = sqlite3_column_int(load_texture_query, 1);
		outTexture.size_y = sqlite3_column_int(load_texture_query, 2);
		outTexture.data_raw = (stbi_uc*)malloc(bytes);

		memcpy(outTexture.data_raw, ptr, bytes);

		sqlite3_reset(load_texture_query);
		return 0;
	}
	sqlite3_reset(load_texture_query);
	return 1;
}


int RealSceneLoader::load_db_texture(std::string texture_name, void* outData)
{	
	sqlite3_bind_text(load_texture_query, 1, texture_name.c_str(), texture_name.size(), SQLITE_TRANSIENT);

	int rc = sqlite3_step(load_texture_query);

	int bytes = 0;

	if (rc == SQLITE_ROW) {

		bytes = sqlite3_column_bytes(load_texture_query, 0);
		const void* ptr = sqlite3_column_blob(load_texture_query, 0);

		memcpy(outData, ptr, bytes);

		sqlite3_reset(load_texture_query);
		return 0;
	}
	sqlite3_reset(load_texture_query);
	return 1;
}