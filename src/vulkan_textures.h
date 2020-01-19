#include "vulkan_render.h"


struct aiScene;
struct aiMaterial;
enum aiTextureType;


class TextureLoader {
public:
	virtual void add_request_from_assimp( const aiScene* scene, aiMaterial* material, aiTextureType textype,
		const std::string& scenepath) = 0 {};

	static TextureLoader* create_new_loader(VulkanEngine* ownerEngine);

	virtual void flush_requests() = 0;
};