#include <autobind.h>
#include <vulkan_descriptors.h>
#include <vulkan_render.h>
#include <shader_processor.h>


const char* remove_prefix(const char* prefix, const char* string) {
	int i = 0;
	while (prefix[i]) {
		if (prefix[i] != string[i]) {
			return nullptr;
		}
		i++;
	}
	return string+i;
}
bool has_prefix(const char* prefix, std::string_view string) {
	int i = 0;
	while (prefix[i]) {
		if (prefix[i] != string[i]) {
			return false;
		}
		i++;
	}
	return true;
}


bool AutobindState::fill_descriptor(DescriptorSetBuilder* builder)
{
	//find rendergraph attachments
	auto refl = builder->effect->get_reflection();
	for (auto [name, val] : refl->DataBindings)
	{
		//only try to look up if it starts with underscore
		if (name[0] == '_')
		{
			const char* noprefix = remove_prefix("_RG_", name.c_str());
			if (noprefix) {

				auto image = Engine->render_graph.get_image_descriptor(noprefix);
				builder->bind_image(val.set, val.binding, image);
			}
			else {
				VkDescriptorImageInfo im_info;
				if (find_image(name.c_str(), im_info)) {
					builder->bind_image(val.set, val.binding, im_info);
				}
			}
		}
	}

	return true;
}

bool AutobindState::find_image(const char* name, VkDescriptorImageInfo& outInfo)
{
	auto it = image_infos.find(name);
	if (it != image_infos.end()) {
		outInfo = (*it).second;
		return true;
	}
	else {
		return false;
	}
}
