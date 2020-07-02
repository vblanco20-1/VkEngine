#include "vulkan_render.h"
#include "slotbuffer.h"


struct aiScene;
struct aiMaterial;
enum aiTextureType;

namespace sp {
	struct SceneLoader;
}

class TextureBindlessCache {
public:
	std::vector< vk::DescriptorImageInfo> all_images;
	std::vector< EntityID> image_Ids;

	void TextureBindlessCache::AddToCache(TextureResource& resource, EntityID id);
	void Refresh(TextureResource& resource, int index);
};

class TextureLoader {
public:

	virtual void add_request_from_assimp( const aiScene* scene, aiMaterial* material, aiTextureType textype,
		const std::string& scenepath) = 0 ;

	virtual void add_request_from_assimp_db(sp::SceneLoader* loader, aiMaterial* material, aiTextureType textype,
		const std::string& scenepath) = 0;

	static TextureLoader* create_new_loader(VulkanEngine* ownerEngine);

	virtual void flush_requests() = 0;
	virtual bool should_flush() = 0;

	virtual void request_texture_load(guid::BinaryGUID textureGUID) = 0;
	virtual void update_background_loads() = 0;

	virtual void preload_textures(sp::SceneLoader* loader) = 0;
	virtual void load_all_textures(sp::SceneLoader* loader, const std::string& scenepath) = 0;
};