#include <vector>

enum class ShaderType : char{
	Vertex,
	TessControl,
	TessEvaluation,
	Geometry,
	Fragment,
	Compute,
	RayGenNV,
	IntersectNV,
	AnyHitNV,
	ClosestHitNV,
	MissNV,
	CallableNV,
	TaskNV,
	MeshNV,
	Count
};

struct ShaderModule {
	ShaderType type;
	std::vector<unsigned int> SpirV;
};

struct ShaderCompileUnit {
	const char* path;
	bool bSuccess;
	ShaderModule resultModule;
};

bool compile_shader(const char* path, ShaderModule* outModule);