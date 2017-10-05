/*
Copyright (c) 2015-2017 Alternative Games Ltd / Turo Lamminen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#ifdef RENDERER_OPENGL


#include <cassert>

#include <algorithm>
#include <vector>

#include <spirv_glsl.hpp>

#include "Renderer.h"
#include "Utils.h"
#include "RendererInternal.h"


namespace renderer {


void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /* length */, const GLchar *message, const void * /* userParam */);


static GLuint createShader(GLenum type, const std::string &name, const std::vector<char> &src) {
	assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);

	const char *sourcePointer = &src[0];
	GLint sourceLen = src.size();

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &sourcePointer, &sourceLen);
	glCompileShader(shader);

	// TODO: defer checking to enable multithreaded shader compile
	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	{
		GLint infoLogLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
		if (infoLogLen != 0) {
			std::vector<char> infoLog(infoLogLen + 1, '\0');
			// TODO: better logging
			glGetShaderInfoLog(shader, infoLogLen, NULL, &infoLog[0]);
			if (infoLog[0] != '\0') {
				LOG("shader \"%s\" info log:\n%s\ninfo log end\n", name.c_str(), &infoLog[0]); fflush(stdout);
			}
		}
	}

	if (status != GL_TRUE) {
		glDeleteShader(shader);
		throw std::runtime_error("shader compile failed");
	}

	return shader;
}


static GLenum glTexFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		__builtin_unreachable();

	case Format::R8:
		return GL_R8;

	case Format::RG8:
		return GL_RG8;

	case Format::RGB8:
		return GL_RGB8;

	case Format::RGBA8:
		return GL_RGBA8;

	case Format::Depth16:
		return GL_DEPTH_COMPONENT16;

	}

	__builtin_unreachable();
}


static GLenum glTexBaseFormat(Format format) {
	switch (format) {
	case Format::Invalid:
		__builtin_unreachable();

	case Format::R8:
		return GL_RED;

	case Format::RG8:
		return GL_RG;

	case Format::RGB8:
		return GL_RGB;

	case Format::RGBA8:
		return GL_RGBA;

	case Format::Depth16:
		// not supposed to use this format here
		assert(false);
		return GL_NONE;

	}

	__builtin_unreachable();
}


static const char *errorSource(GLenum source)
{
	switch (source)
	{
	case GL_DEBUG_SOURCE_API:
		return "API";
		break;

	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		return "window system";
		break;

	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		return "shader compiler";
		break;

	case GL_DEBUG_SOURCE_THIRD_PARTY:
		return "third party";
		break;

	case GL_DEBUG_SOURCE_APPLICATION:
		return "application";
		break;

	case GL_DEBUG_SOURCE_OTHER:
		return "other";
		break;

	default:
		break;
	}

	return "unknown source";
}


static const char *errorType(GLenum type)
{
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:
	case GL_DEBUG_CATEGORY_API_ERROR_AMD:
		return "error";
		break;

	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
	case GL_DEBUG_CATEGORY_DEPRECATION_AMD:
		return "deprecated behavior";
		break;

	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
	case GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD:
		return "undefined behavior";
		break;

	case GL_DEBUG_TYPE_PORTABILITY:
		return "portability";
		break;

	case GL_DEBUG_TYPE_PERFORMANCE:
	case GL_DEBUG_CATEGORY_PERFORMANCE_AMD:
		return "performance";
		break;

	case GL_DEBUG_TYPE_OTHER:
	case GL_DEBUG_CATEGORY_OTHER_AMD:
		return "other";
		break;

	case GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD:
		return "window system error";
		break;

	case GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD:
		return "shader compiler error";
		break;

	case GL_DEBUG_CATEGORY_APPLICATION_AMD:
		return "application error";
		break;

	default:
		break;

	}

	return "unknown type";
}


void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /* length */, const GLchar *message, const void * /* userParam */)
{
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH_ARB:
		LOG("GL error from %s type %s: (%d) %s\n", errorSource(source), errorType(type), id, message);
		break;

	case GL_DEBUG_SEVERITY_MEDIUM_ARB:
		LOG("GL warning from %s type %s: (%d) %s\n", errorSource(source), errorType(type), id, message);
		break;

	case GL_DEBUG_SEVERITY_LOW_ARB:
		LOG("GL debug from %s type %s: (%d) %s\n", errorSource(source), errorType(type), id, message);
		break;

	default:
		LOG("GL error of unknown severity %x from %s type %s: (%d) %s\n", severity, errorSource(source), errorType(type), id, message);
		break;
	}
}


RendererBase::RendererBase()
: ringBuffer(0)
, persistentMapInUse(false)
, persistentMapping(nullptr)
, window(nullptr)
, context(nullptr)
, debug(false)
, vao(0)
, idxBuf16Bit(false)
, indexBufByteOffset(0)
, uboAlign(0)
, ssboAlign(0)
{
}


RendererBase::~RendererBase()
{
}


RendererImpl::RendererImpl(const RendererDesc &desc)
: swapchainDesc(desc.swapchain)
, savePreprocessedShaders(false)
, frameNum(0)
, ringBufSize(0)
, ringBufPtr(0)
, inFrame(false)
, inRenderPass(false)
, validPipeline(false)
, pipelineDrawn(false)
, scissorSet(false)
{

	// TODO: check return value
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

	spirvCacheDir.reset(SDL_GetPrefPath(nullptr, "SMAADemo"));

	// TODO: fullscreen, resizable, highdpi etc. as necessary
	// TODO: check errors
	// TODO: other GL attributes as necessary
	// TODO: use core context (and maybe debug as necessary)

	unsigned int glMajor = 4;
	unsigned int glMinor = 5;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glMinor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	if (desc.debug) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	}

	SDL_DisplayMode mode;
	memset(&mode, 0, sizeof(mode));
	int numDisplays = SDL_GetNumVideoDisplays();
	LOG("Number of displays detected: %i\n", numDisplays);

	for (int i = 0; i < numDisplays; i++) {
		int numModes = SDL_GetNumDisplayModes(i);
		LOG("Number of display modes for display %i : %i\n", i, numModes);

		for (int j = 0; j < numModes; j++) {
			SDL_GetDisplayMode(i, j, &mode);
			LOG("Display mode %i : width %i, height %i, BPP %i, refresh %u Hz\n", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), mode.refresh_rate);
		}
	}

	int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	if (desc.swapchain.fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, desc.swapchain.width, desc.swapchain.height, flags);

	context = SDL_GL_CreateContext(window);

	if (desc.swapchain.vsync) {
		// enable vsync, using late swap tearing if possible
		int retval = SDL_GL_SetSwapInterval(-1);
		if (retval != 0) {
			// TODO: check return val
			SDL_GL_SetSwapInterval(1);
		}
		LOG("VSync is on\n");
	}

	// TODO: call SDL_GL_GetDrawableSize, log GL attributes etc.

	glewExperimental = true;
	glewInit();

	// TODO: check extensions
	// at least direct state access, texture storage

	if (!GLEW_ARB_direct_state_access) {
		LOG("ARB_direct_state_access not found\n");
		throw std::runtime_error("ARB_direct_state_access not found");
	}

	if (!GLEW_ARB_buffer_storage) {
		LOG("ARB_buffer_storage not found\n");
		throw std::runtime_error("ARB_buffer_storage not found");
	}

	if (!GLEW_ARB_clip_control) {
		LOG("ARB_clip_control not found\n");
		throw std::runtime_error("ARB_clip_control not found");
	}

	if (desc.debug) {
		if (GLEW_KHR_debug) {
			LOG("KHR_debug found\n");

			glDebugMessageCallback(glDebugCallback, NULL);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

			debug = true;
		} else {
			LOG("KHR_debug not found\n");
		}
	}

	LOG("GL vendor: \"%s\"\n", glGetString(GL_VENDOR));
	LOG("GL renderer: \"%s\"\n", glGetString(GL_RENDERER));
	LOG("GL version: \"%s\"\n", glGetString(GL_VERSION));
	LOG("GLSL version: \"%s\"\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	GLint temp = -1;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &temp);
	uboAlign = temp;
	LOG("UBO align: %d\n", uboAlign);

	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &temp);
	ssboAlign = temp;
	LOG("SSBO align: %d\n", ssboAlign);

	// TODO: use GL_UPPER_LEFT to match Vulkan
	glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// set up ring buffer
	glCreateBuffers(1, &ringBuffer);
	// TODO: proper error checking
	assert(ringBuffer != 0);
	assert(desc.ephemeralRingBufSize > 0);
	unsigned int bufferFlags = 0;
	// if debug is on, disable persistent buffer because apitrace can't trace it
	// TODO: should have separate toggles for debug messages and debug tracing
	persistentMapInUse = !debug;
	ringBufSize        = desc.ephemeralRingBufSize;

	if (!persistentMapInUse) {
		// need GL_DYNAMIC_STORAGE_BIT since we intend to glBufferSubData it
		bufferFlags |= GL_DYNAMIC_STORAGE_BIT;
	} else {
		// TODO: do we need GL_DYNAMIC_STORAGE_BIT?
		// spec seems to say only for glBufferSubData, not persistent mapping
		bufferFlags |= GL_MAP_WRITE_BIT;
		bufferFlags |= GL_MAP_PERSISTENT_BIT;
		bufferFlags |= GL_MAP_COHERENT_BIT;
	}

	glNamedBufferStorage(ringBuffer, ringBufSize, nullptr, bufferFlags);
	if (persistentMapInUse) {
		persistentMapping = reinterpret_cast<char *>(glMapNamedBufferRange(ringBuffer, 0, ringBufSize, bufferFlags));
	}

	// swap once to get better traces
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
}


RendererImpl::~RendererImpl() {
	assert(ringBuffer != 0);
	// TODO: need to wait until GPU finished with last frames?
	if (persistentMapInUse) {
		glUnmapNamedBuffer(ringBuffer);
		persistentMapping = nullptr;
	} else {
		assert(persistentMapping == nullptr);
	}

	glDeleteBuffers(1, &ringBuffer);
	ringBuffer = 0;

	framebuffers.clearWith([](Framebuffer &fb) {
		assert(fb.fbo != 0);
		glDeleteFramebuffers(1, &fb.fbo);
		fb.fbo = 0;
	} );

	renderPasses.clearWith([](RenderPass &) {
	} );

	renderTargets.clearWith([this](RenderTarget &rt) {
		assert(rt.texture);

		if (rt.readFBO != 0) {
			glDeleteFramebuffers(1, &rt.readFBO);
			rt.readFBO = 0;
		}

		{
			auto &tex = this->textures.get(rt.texture);
			assert(tex.renderTarget);
			tex.renderTarget = false;
			glDeleteTextures(1, &tex.tex);
			tex.tex = 0;
		}

		this->textures.remove(rt.texture);
		rt.texture = TextureHandle();
	} );


	pipelines.clearWith([](Pipeline &p) {
		assert(p.shader != 0);
		glDeleteProgram(p.shader);
		p.shader = 0;
	} );

	vertexShaders.clearWith([](VertexShader &v) {
		assert(v.shader != 0);
		glDeleteShader(v.shader);
		v.shader = 0;
	} );

	fragmentShaders.clearWith([](FragmentShader &f) {
		assert(f.shader != 0);
		glDeleteShader(f.shader);
		f.shader = 0;
	} );

	textures.clearWith([](Texture &tex) {
		assert(!tex.renderTarget);
		assert(tex.tex != 0);

		glDeleteTextures(1, &tex.tex);
		tex.tex = 0;
	} );

	samplers.clearWith([](Sampler &sampler) {
		assert(sampler.sampler != 0);

		glDeleteSamplers(1, &sampler.sampler);
		sampler.sampler = 0;
	} );

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);

	SDL_Quit();
}


BufferHandle RendererImpl::createBuffer(uint32_t size, const void *contents) {
	assert(size != 0);

	auto result    = buffers.add();
	Buffer &buffer = result.first;
	glCreateBuffers(1, &buffer.buffer);
	glNamedBufferStorage(buffer.buffer, size, contents, 0);
	buffer.ringBufferAlloc = false;
	buffer.beginOffs       = 0;
	buffer.size            = size;

	return result.second;
}


BufferHandle RendererImpl::createEphemeralBuffer(uint32_t size, const void *contents) {
	assert(size != 0);
	assert(contents != nullptr);

	// TODO: use appropriate alignment
	// TODO: need buffer usage flags for that
	unsigned int beginPtr = ringBufferAllocate(size, std::max(uboAlign, ssboAlign));

	if (persistentMapInUse) {
		memcpy(persistentMapping + beginPtr, contents, size);
	} else {
		glNamedBufferSubData(ringBuffer, beginPtr, size, contents);
	}

	auto result    = buffers.add();
	Buffer &buffer = result.first;
	buffer.buffer          = ringBuffer;
	buffer.ringBufferAlloc = true;
	buffer.beginOffs       = beginPtr;
	buffer.size            = size;

	ephemeralBuffers.push_back(result.second);

	return result.second;
}


static std::vector<ShaderResource> processShaderResources(spirv_cross::CompilerGLSL &glsl) {
	auto spvResources = glsl.get_shader_resources();

	// TODO: map descriptor sets to opengl indices for textures/samplers
	// TODO: call build_combined_image_samplers() ?
	std::vector<ShaderResource> resources;

	for (const auto &ubo : spvResources.uniform_buffers) {
		ShaderResource r;
		r.set     = glsl.get_decoration(ubo.id, spv::DecorationDescriptorSet);
		r.binding = glsl.get_decoration(ubo.id, spv::DecorationBinding);
		r.type    = DescriptorType::UniformBuffer;
		resources.push_back(r);

		// opengl doesn't like set decorations, strip them
		// TODO: check that indices don't conflict
		glsl.unset_decoration(ubo.id, spv::DecorationDescriptorSet);
	}

	for (const auto &ssbo : spvResources.storage_buffers) {
		ShaderResource r;
		r.set     = glsl.get_decoration(ssbo.id, spv::DecorationDescriptorSet);
		r.binding = glsl.get_decoration(ssbo.id, spv::DecorationBinding);
		r.type    = DescriptorType::StorageBuffer;
		resources.push_back(r);

		// opengl doesn't like set decorations, strip them
		// TODO: check that indices don't conflict
		glsl.unset_decoration(ssbo.id, spv::DecorationDescriptorSet);
	}

	for (const auto &s : spvResources.separate_samplers) {
		ShaderResource r;
		r.set     = glsl.get_decoration(s.id, spv::DecorationDescriptorSet);
		r.binding = glsl.get_decoration(s.id, spv::DecorationBinding);
		r.type    = DescriptorType::Sampler;
		resources.push_back(r);

		// opengl doesn't like set decorations, strip them
		// TODO: check that indices don't conflict
		glsl.unset_decoration(s.id, spv::DecorationDescriptorSet);
	}

	for (const auto &tex : spvResources.separate_images) {
		ShaderResource r;
		r.set     = glsl.get_decoration(tex.id, spv::DecorationDescriptorSet);
		r.binding = glsl.get_decoration(tex.id, spv::DecorationBinding);
		r.type    = DescriptorType::Texture;
		resources.push_back(r);

		// opengl doesn't like set decorations, strip them
		// TODO: check that indices don't conflict
		glsl.unset_decoration(tex.id, spv::DecorationDescriptorSet);
	}

	for (const auto &s : spvResources.sampled_images) {
		ShaderResource r;
		r.set     = glsl.get_decoration(s.id, spv::DecorationDescriptorSet);
		r.binding = glsl.get_decoration(s.id, spv::DecorationBinding);
		r.type    = DescriptorType::CombinedSampler;
		resources.push_back(r);

		// opengl doesn't like set decorations, strip them
		// TODO: check that indices don't conflict
		glsl.unset_decoration(s.id, spv::DecorationDescriptorSet);
	}

	return resources;
}


static void pushString(std::vector<char> &v, const std::string &str) {
	v.insert(v.end(), str.begin(), str.end());
}


static std::vector<char> spirv2glsl(const std::string &name, const ShaderMacros &macros, spirv_cross::CompilerGLSL &glsl) {
	std::string src_ = glsl.compile();

	std::vector<char> result;
	{
		size_t size = src_.size() + 3 + name.size() + 1;
		std::vector<std::string> sorted;
		sorted.reserve(macros.size());
		for (const auto &macro : macros) {
			std::string str = macro.first;
			if (!macro.second.empty()) {
				str += "=";
				str += macro.second;
			}
			size += 3 + str.size() + 1;
			sorted.emplace_back(std::move(str));
		}

		std::sort(sorted.begin(), sorted.end());

		result.reserve(size);

		pushString(result, "// ");
		pushString(result, name);
		result.push_back('\n');

		for (const auto &s : sorted) {
			pushString(result, "// ");
			pushString(result, s);
			result.push_back('\n');
		}
	}

	result.insert(result.end(), src_.begin(), src_.end());
	return result;
}


VertexShaderHandle RendererImpl::createVertexShader(const std::string &name, const ShaderMacros &macros) {
	std::string vertexShaderName   = name + ".vert";

    std::vector<uint32_t> spirv = compileSpirv(vertexShaderName, macros, shaderc_glsl_vertex_shader);

	spirv_cross::CompilerGLSL glsl(spirv);
	spirv_cross::CompilerGLSL::Options glslOptions;
	glslOptions.vertex.fixup_clipspace = false;
	glsl.set_options(glslOptions);

	auto resources = processShaderResources(glsl);
	std::vector<char> src = spirv2glsl(name, macros, glsl);

	if (savePreprocessedShaders) {
		// FIXME: name not really accurate
		writeFile(vertexShaderName + ".prep", &src[0], src.size());
	}

	auto result_ = vertexShaders.add();
	auto &v = result_.first;
	v.shader    = createShader(GL_VERTEX_SHADER, vertexShaderName, src);
	v.name      = vertexShaderName;
	v.resources = std::move(resources);

	return result_.second;
}


FragmentShaderHandle RendererImpl::createFragmentShader(const std::string &name, const ShaderMacros &macros) {
	std::string fragmentShaderName = name + ".frag";

	std::vector<uint32_t> spirv = compileSpirv(fragmentShaderName, macros, shaderc_glsl_fragment_shader);

	spirv_cross::CompilerGLSL glsl(spirv);
	spirv_cross::CompilerGLSL::Options glslOptions;
	glslOptions.vertex.fixup_clipspace = false;
	glsl.set_options(glslOptions);

	auto resources = processShaderResources(glsl);
	std::vector<char> src = spirv2glsl(name, macros, glsl);

	if (savePreprocessedShaders) {
		// FIXME: name not really accurate
		writeFile(fragmentShaderName + ".prep", &src[0], src.size());
	}

	auto result_ = fragmentShaders.add();
	auto &f = result_.first;
	f.shader = createShader(GL_FRAGMENT_SHADER, name, src);
	f.name      = fragmentShaderName;
	f.resources = std::move(resources);

	return result_.second;
}


static const char *descriptorTypeName(DescriptorType t) {
	switch (t) {
	case DescriptorType::End:
		return "End";

	case DescriptorType::UniformBuffer:
		return "UniformBuffer";

	case DescriptorType::StorageBuffer:
		return "StorageBuffer";

	case DescriptorType::Sampler:
		return "Sampler";

	case DescriptorType::Texture:
		return "Texture";

	case DescriptorType::CombinedSampler:
		return "CombinedSampler";

	case DescriptorType::Count:
		assert(false);  // shouldn't happen
		return "Count";

	}

	assert(false);
	return "ERROR!";
}


static void checkShaderResources(const std::string &name, const std::vector<ShaderResource> &resources, const std::vector<std::vector<DescriptorLayout> > &layouts) {
	for (const auto &r : resources) {
		assert(r.set < MAX_DESCRIPTOR_SETS);
		const auto &set = layouts[r.set];

		if (r.binding >= set.size()) {
			LOG("ERROR: set %u binding %u type %s in shader \"%s\" greater than set size (%u)\n", r.set, r.binding, descriptorTypeName(r.type), name.c_str(), static_cast<unsigned int>(set.size()));
			continue;
		}

		if (set[r.binding].type != r.type) {
			LOG("ERROR: set %u binding %u type %s in shader \"%s\" doesn't match ds layout (%s)\n", r.set, r.binding, descriptorTypeName(r.type), name.c_str(), descriptorTypeName(set[r.binding].type));
		}
	}
}


PipelineHandle RendererImpl::createPipeline(const PipelineDesc &desc) {
	assert(desc.vertexShader_);
	assert(desc.fragmentShader_);
	assert(desc.renderPass_);
	assert(desc.name_ != nullptr);

	const auto &v = vertexShaders.get(desc.vertexShader_);
    const auto &f = fragmentShaders.get(desc.fragmentShader_);

	// match shader resources against pipeline layouts
	{
		std::vector<std::vector<DescriptorLayout> > layouts;
		layouts.resize(MAX_DESCRIPTOR_SETS);
		for (unsigned int i = 0; i < MAX_DESCRIPTOR_SETS; i++) {
			if (desc.descriptorSetLayouts[i]) {
				layouts[i] = dsLayouts.get(desc.descriptorSetLayouts[i]).layout;
			}
		}
		checkShaderResources(v.name, v.resources, layouts);
		checkShaderResources(f.name, f.resources, layouts);
	}

	// TODO: cache shaders
	GLuint program = glCreateProgram();

	glAttachShader(program, v.shader);
	glAttachShader(program, f.shader);
	glLinkProgram(program);

	GLint status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &status);
		std::vector<char> infoLog(status + 1, '\0');
		// TODO: better logging
		glGetProgramInfoLog(program, status, NULL, &infoLog[0]);
		LOG("info log: %s\n", &infoLog[0]); fflush(stdout);
		throw std::runtime_error("shader link failed");
	}
	glUseProgram(program);

	auto result = pipelines.add();
	Pipeline &pipeline = result.first;
	pipeline.desc      = desc;
	pipeline.shader    = program;

	return result.second;
}


FramebufferHandle RendererImpl::createFramebuffer(const FramebufferDesc &desc) {
	assert(desc.name_ != nullptr);
	assert(desc.renderPass_);

	auto &renderPass = renderPasses.get(desc.renderPass_);

	auto result = framebuffers.add();
	Framebuffer &fb = result.first;
	glCreateFramebuffers(1, &fb.fbo);

	const auto &colorRT = renderTargets.get(desc.colors_[0]);

	assert(colorRT.width  > 0);
	assert(colorRT.height > 0);
	assert(colorRT.texture);
	assert(colorRT.format == renderPass.desc.colorFormats_[0]);
	fb.renderPass = desc.renderPass_;
	fb.colors[0]  = desc.colors_[0];
	fb.width      = colorRT.width;
	fb.height     = colorRT.height;

	const auto &colorRTtex = textures.get(colorRT.texture);
	assert(colorRTtex.renderTarget);
	assert(colorRTtex.tex != 0);

	glNamedFramebufferTexture(fb.fbo, GL_COLOR_ATTACHMENT0, colorRTtex.tex, 0);
	assert(!desc.colors_[1]);

	if (desc.depthStencil_) {
		const auto &depthRT = renderTargets.get(desc.depthStencil_);
		assert(depthRT.format == renderPass.desc.depthStencilFormat_);
		assert(depthRT.width  == colorRT.width);
		assert(depthRT.height == colorRT.height);
		assert(depthRT.texture);
		const auto &depthRTtex = textures.get(depthRT.texture);
		assert(depthRTtex.renderTarget);
		assert(depthRTtex.tex != 0);
		fb.depthStencil = desc.depthStencil_;
		glNamedFramebufferTexture(fb.fbo, GL_DEPTH_ATTACHMENT, depthRTtex.tex, 0);
	} else {
		assert(renderPass.desc.depthStencilFormat_ == Format::Invalid);
	}

	if (debug) {
		glObjectLabel(GL_FRAMEBUFFER, fb.fbo, -1, desc.name_);
	}

	return result.second;
}


RenderPassHandle RendererImpl::createRenderPass(const RenderPassDesc &desc) {
	assert(desc.name_ != nullptr);

	auto result = renderPasses.add();
	RenderPass &pass = result.first;
	pass.desc = desc;
	return result.second;
}


RenderTargetHandle RendererImpl::createRenderTarget(const RenderTargetDesc &desc) {
	assert(desc.width_  > 0);
	assert(desc.height_ > 0);
	assert(desc.format_ != Format::Invalid);
	assert(desc.name_   != nullptr);

	GLuint id = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &id);
	glTextureStorage2D(id, 1, glTexFormat(desc.format_), desc.width_, desc.height_);
	glTextureParameteri(id, GL_TEXTURE_MAX_LEVEL, 0);
	if (debug) {
		glObjectLabel(GL_TEXTURE, id, -1, desc.name_);
	}

	auto textureResult = textures.add();
	Texture &tex = textureResult.first;
	tex.tex           = id;
	tex.width         = desc.width_;
	tex.height        = desc.height_;
	tex.renderTarget  = true;

	auto result = renderTargets.add();
	RenderTarget &rt = result.first;
	rt.width  = desc.width_;
	rt.height = desc.height_;
	rt.format = desc.format_;
	// TODO: std::move?
	rt.texture = textureResult.second;

	return result.second;
}


SamplerHandle RendererImpl::createSampler(const SamplerDesc &desc) {
	auto result = samplers.add();
	Sampler &sampler = result.first;
	glCreateSamplers(1, &sampler.sampler);

	glSamplerParameteri(sampler.sampler, GL_TEXTURE_MIN_FILTER, (desc.min == FilterMode::Nearest) ? GL_NEAREST: GL_LINEAR);
	glSamplerParameteri(sampler.sampler, GL_TEXTURE_MAG_FILTER, (desc.mag == FilterMode::Nearest) ? GL_NEAREST: GL_LINEAR);
	glSamplerParameteri(sampler.sampler, GL_TEXTURE_WRAP_S,     (desc.wrapMode == WrapMode::Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	glSamplerParameteri(sampler.sampler, GL_TEXTURE_WRAP_T,     (desc.wrapMode == WrapMode::Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT);

	return result.second;
}


TextureHandle RendererImpl::createTexture(const TextureDesc &desc) {
	assert(desc.width_   > 0);
	assert(desc.height_  > 0);
	assert(desc.numMips_ > 0);

	GLuint texture = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glTextureStorage2D(texture, 1, glTexFormat(desc.format_), desc.width_, desc.height_);
	glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, desc.numMips_ - 1);
	unsigned int w = desc.width_, h = desc.height_;

	for (unsigned int i = 0; i < desc.numMips_; i++) {
		assert(desc.mipData_[i].data != nullptr);
		assert(desc.mipData_[i].size != 0);
		glTextureSubImage2D(texture, i, 0, 0, w, h, glTexBaseFormat(desc.format_), GL_UNSIGNED_BYTE, desc.mipData_[i].data);

		w = std::max(w / 2, 1u);
		h = std::max(h / 2, 1u);
	}

	auto result  = textures.add();
	Texture &tex = result.first;
	tex.tex    = texture;
	tex.width  = desc.width_;
	tex.height = desc.height_;
	assert(!tex.renderTarget);

	return result.second;
}


DSLayoutHandle RendererImpl::createDescriptorSetLayout(const DescriptorLayout *layout) {
	auto result = dsLayouts.add();
	DescriptorSetLayout &dsLayout = result.first;

	while (layout->type != DescriptorType::End) {
		dsLayout.layout.push_back(*layout);
		layout++;
	}
	assert(layout->offset == 0);

	return result.second;
}


TextureHandle RendererImpl::getRenderTargetTexture(RenderTargetHandle handle) {
	const auto &rt = renderTargets.get(handle);

	const auto &tex = textures.get(rt.texture);
	assert(tex.renderTarget);

	return rt.texture;
}


void RendererImpl::deleteBuffer(BufferHandle handle) {
	buffers.removeWith(handle, [](struct Buffer &b) {
		assert(b.buffer != 0);
		glDeleteBuffers(1, &b.buffer);
		b.buffer = 0;

		assert(b.size != 0);
		b.size   = 0;

		assert(!b.ringBufferAlloc);
	} );
}


void RendererImpl::deleteFramebuffer(FramebufferHandle handle) {
	framebuffers.removeWith(handle, [](Framebuffer &fb) {
		assert(fb.fbo != 0);
		glDeleteFramebuffers(1, &fb.fbo);
		fb.fbo = 0;
	} );
}


void RendererImpl::deleteRenderPass(RenderPassHandle handle) {
	renderPasses.removeWith(handle, [](RenderPass &) {
	} );
}


void RendererImpl::deleteRenderTarget(RenderTargetHandle &handle) {
	renderTargets.removeWith(handle, [this](RenderTarget &rt) {
		assert(rt.texture);

		if (rt.readFBO != 0) {
			glDeleteFramebuffers(1, &rt.readFBO);
			rt.readFBO = 0;
		}

		{
			auto &tex = this->textures.get(rt.texture);
			assert(tex.renderTarget);
			tex.renderTarget = false;
			assert(tex.tex != 0);
			glDeleteTextures(1, &tex.tex);
			tex.tex = 0;
		}
		this->textures.remove(rt.texture);
		rt.texture = TextureHandle();
	} );
}


void RendererImpl::deleteSampler(SamplerHandle handle) {
	samplers.removeWith(handle, [](Sampler &sampler) {
		assert(sampler.sampler != 0);

		glDeleteSamplers(1, &sampler.sampler);
		sampler.sampler = 0;
	} );
}


void RendererImpl::deleteTexture(TextureHandle handle) {
	textures.removeWith(handle, [](Texture &tex) {
		assert(!tex.renderTarget);
		assert(tex.tex != 0);

		glDeleteTextures(1, &tex.tex);
		tex.tex = 0;
	} );
}


void RendererImpl::recreateSwapchain(const SwapchainDesc &desc) {
	if (swapchainDesc.fullscreen != desc.fullscreen) {
		if (desc.fullscreen) {
			// TODO: check return val?
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			LOG("Fullscreen\n");
		} else {
			SDL_SetWindowFullscreen(window, 0);
			LOG("Windowed\n");
		}
	}

	if (swapchainDesc.vsync != desc.vsync) {
		if (desc.vsync) {
			// enable vsync, using late swap tearing if possible
			int retval = SDL_GL_SetSwapInterval(-1);
			if (retval != 0) {
				// TODO: check return val
				SDL_GL_SetSwapInterval(1);
			}
			LOG("VSync is on\n");
		} else {
			// TODO: check return val
			SDL_GL_SetSwapInterval(0);
			LOG("VSync is off\n");
		}
	}
	swapchainDesc = desc;

	int w = -1, h = -1;
	SDL_GL_GetDrawableSize(window, &w, &h);
	if (w <= 0 || h <= 0) {
		throw std::runtime_error("drawable size is negative");
	}
	swapchainDesc.width  = w;
	swapchainDesc.height = h;
}


void RendererImpl::beginFrame() {
	assert(!inFrame);
	inFrame       = true;
	inRenderPass  = false;
	validPipeline = false;
	pipelineDrawn = true;

	// TODO: reset all relevant state in case some 3rd-party program fucked them up
	glDepthMask(GL_TRUE);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	// TODO: only clear depth/stencil if we have it
	// TODO: set color/etc write masks if necessary
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void RendererImpl::presentFrame(RenderTargetHandle image) {
	assert(inFrame);
	inFrame = false;

	auto &rt = renderTargets.get(image);
	assert(rt.currentLayout == Layout::TransferSrc);

	unsigned int width  = rt.width;
	unsigned int height = rt.height;

	// TODO: only if enabled
	glDisable(GL_SCISSOR_TEST);

	// TODO: necessary? should do linear blit?
	assert(width  == swapchainDesc.width);
	assert(height == swapchainDesc.height);

	assert(width > 0);
	assert(height > 0);

	if (rt.readFBO == 0) {
		glCreateFramebuffers(1, &rt.readFBO);
		const auto &colorTex = textures.get(rt.texture);
		assert(colorTex.renderTarget);
		assert(colorTex.tex != 0);
		glNamedFramebufferTexture(rt.readFBO, GL_COLOR_ATTACHMENT0, colorTex.tex, 0);
	}
	glBindFramebuffer(GL_READ_FRAMEBUFFER, rt.readFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	SDL_GL_SwapWindow(window);

	// TODO: multiple frames, only delete after no longer in use by GPU
	// TODO: use persistent coherent buffer
	for (auto handle : ephemeralBuffers) {
		Buffer &buffer = buffers.get(handle);
		assert(buffer.buffer == ringBuffer);
		buffer.buffer = 0;

		assert(buffer.ringBufferAlloc);
		buffer.ringBufferAlloc = false;

		assert(buffer.size   >  0);
		buffer.size = 0;

		buffers.remove(handle);
	}
	ephemeralBuffers.clear();
}


void RendererImpl::beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle) {
	assert(inFrame);
	assert(!inRenderPass);
	inRenderPass  = true;
	validPipeline = false;

	assert(fbHandle);
	const auto &fb = framebuffers.get(fbHandle);
	assert(fb.fbo != 0);

	// make sure renderpass and framebuffer match
	// OpenGL doesn't care but Vulkan does
	assert(fb.renderPass == rpHandle);

	// TODO: should get clear bits from RenderPass object
	GLbitfield mask = GL_COLOR_BUFFER_BIT;
	if (fb.depthStencil) {
		mask |= GL_DEPTH_BUFFER_BIT;
	}

	assert(fb.fbo != 0);
	assert(fb.width > 0);
	assert(fb.height > 0);

	glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
	glClear(mask);

	currentRenderPass  = rpHandle;
	currentFramebuffer = fbHandle;
}


void RendererImpl::endRenderPass() {
	assert(inFrame);
	assert(inRenderPass);
	inRenderPass = false;

	const auto &pass = renderPasses.get(currentRenderPass);
	const auto &fb = framebuffers.get(currentFramebuffer);

	// TODO: track depthstencil layout too
	auto &rt = renderTargets.get(fb.colors[0]);
	rt.currentLayout = pass.desc.colorFinalLayout_;

	currentRenderPass = RenderPassHandle();
	currentFramebuffer = FramebufferHandle();
}


void RendererImpl::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	assert(inFrame);
	glViewport(x, y, width, height);
}


void RendererImpl::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	assert(validPipeline);
	assert(currentPipeline.scissorTest_);
	scissorSet = true;

	// flip y from Vulkan convention to OpenGL convention
	// TODO: should use current FB height
	glScissor(x, swapchainDesc.height - (y + height), width, height);
}


void RendererImpl::bindPipeline(PipelineHandle pipeline) {
	assert(inFrame);
	assert(pipeline);
	assert(inRenderPass);
	assert(pipelineDrawn);
	pipelineDrawn = false;
	validPipeline = true;
	scissorSet = false;

	const auto &p = pipelines.get(pipeline);
	assert(p.desc.renderPass_ == currentRenderPass);

	// TODO: shadow state, set only necessary
	glUseProgram(p.shader);
	if (p.desc.depthWrite_) {
		glDepthMask(GL_TRUE);
	} else {
		glDepthMask(GL_FALSE);
	}

	if (p.desc.depthTest_) {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}

	if (p.desc.cullFaces_) {
		glEnable(GL_CULL_FACE);
	} else {
		glDisable(GL_CULL_FACE);
	}

	if (p.desc.scissorTest_) {
		glEnable(GL_SCISSOR_TEST);
	} else {
		glDisable(GL_SCISSOR_TEST);
	}

	if (p.desc.blending_) {
		glEnable(GL_BLEND);
		// TODO: get from Pipeline
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glDisable(GL_BLEND);
	}

	uint32_t oldMask = currentPipeline.vertexAttribMask;
	uint32_t newMask = p.desc.vertexAttribMask;

	// enable/disable changed attributes
	uint32_t vattrChanged = oldMask ^ newMask;
#ifdef __GNUC__
	while (vattrChanged != 0) {
		int bit = __builtin_ctz(vattrChanged);
		uint32_t mask = 1 << bit;

		if (newMask & mask) {
			glEnableVertexAttribArray(bit);
		} else {
			glDisableVertexAttribArray(bit);
		}

		vattrChanged &= ~mask;
	}
#else
  while (true) {
    unsigned long bit = 0;
    if (!_BitScanForward(&bit, vattrChanged)) {
      break;
    }
    uint32_t mask = 1 << bit;

    if (newMask & mask) {
      glEnableVertexAttribArray(bit);
    }
    else {
      glDisableVertexAttribArray(bit);
    }

    vattrChanged &= ~mask;
  }
#endif

	// set format on new attributes
	const auto &attribs = p.desc.vertexAttribs;
#ifdef __GNUC__
	while (newMask) {
		int bit = __builtin_ctz(newMask);
		uint32_t mask = 1 << bit;

		const auto &attr = attribs[bit];
		bool normalized = false;
		GLenum format = GL_NONE;
		switch (attr.format) {
		case VtxFormat::Float:
			format = GL_FLOAT;
			break;

		case VtxFormat::UNorm8:
			format = GL_UNSIGNED_BYTE;
			normalized = true;
			break;
		}

		glVertexAttribFormat(bit, attr.count, format, normalized ? GL_TRUE : GL_FALSE, attr.offset);
		glVertexAttribBinding(bit, attr.bufBinding);
		newMask &= ~mask;
	}
#else
  while (true) {
    unsigned long bit = 0;
    if (!_BitScanForward(&bit, vattrChanged)) {
      break;
    }
    uint32_t mask = 1 << bit;

    const auto &attr = attribs[bit];
    bool normalized = false;
    GLenum format = GL_NONE;
    switch (attr.format) {
    case VtxFormat::Float:
      format = GL_FLOAT;
      break;

    case VtxFormat::UNorm8:
      format = GL_UNSIGNED_BYTE;
      normalized = true;
      break;
    }

    glVertexAttribFormat(bit, attr.count, format, normalized ? GL_TRUE : GL_FALSE, attr.offset);
    glVertexAttribBinding(bit, attr.bufBinding);
    newMask &= ~mask;
  }
#endif

	currentPipeline = p.desc;
}


void RendererImpl::bindIndexBuffer(BufferHandle handle, bool bit16) {
	assert(inFrame);
	assert(validPipeline);

	const Buffer &buffer = buffers.get(handle);
	assert(buffer.size > 0);
	if (buffer.ringBufferAlloc) {
		assert(buffer.buffer == ringBuffer);
		assert(buffer.beginOffs + buffer.size < ringBufSize);
	} else {
		assert(buffer.buffer    != 0);
		assert(buffer.beginOffs == 0);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.buffer);
	indexBufByteOffset = buffer.beginOffs;
	idxBuf16Bit = bit16;
}


void RendererImpl::bindVertexBuffer(unsigned int binding, BufferHandle handle) {
	assert(inFrame);
	assert(validPipeline);

	const Buffer &buffer = buffers.get(handle);
	assert(buffer.size >  0);
	if (buffer.ringBufferAlloc) {
		assert(buffer.buffer == ringBuffer);
		assert(buffer.beginOffs + buffer.size < ringBufSize);
	} else {
		assert(buffer.buffer    != 0);
		assert(buffer.beginOffs == 0);
	}
	glBindVertexBuffer(binding, buffer.buffer, buffer.beginOffs, currentPipeline.vertexBuffers[binding].stride);
}


void RendererImpl::bindDescriptorSet(unsigned int index, DSLayoutHandle layoutHandle, const void *data_) {
	assert(validPipeline);
	assert(currentPipeline.descriptorSetLayouts[index] == layoutHandle);

	// TODO: get shader bindings from current pipeline, use index
	const DescriptorSetLayout &layout = dsLayouts.get(layoutHandle);

	const char *data = reinterpret_cast<const char *>(data_);
	unsigned int descIndex = 0;
	for (const auto &l : layout.layout) {
		switch (l.type) {
		case DescriptorType::End:
			// can't happen because createDesciptorSetLayout doesn't let it
			assert(false);
			break;

		case DescriptorType::UniformBuffer: {
			// this is part of the struct, we know it's correctly aligned and right type
			BufferHandle handle = *reinterpret_cast<const BufferHandle *>(data + l.offset);
			const Buffer &buffer = buffers.get(handle);
			assert(buffer.size > 0);
			if (buffer.ringBufferAlloc) {
				assert(buffer.buffer == ringBuffer);
				assert(buffer.beginOffs + buffer.size < ringBufSize);
			} else {
				assert(buffer.buffer    != 0);
				assert(buffer.beginOffs == 0);
			}
			// FIXME: descIndex is not right here
			glBindBufferRange(GL_UNIFORM_BUFFER, descIndex, buffer.buffer, buffer.beginOffs, buffer.size);
		} break;

		case DescriptorType::StorageBuffer: {
			BufferHandle handle = *reinterpret_cast<const BufferHandle *>(data + l.offset);
			const Buffer &buffer = buffers.get(handle);
			assert(buffer.size  > 0);
			if (buffer.ringBufferAlloc) {
				assert(buffer.buffer == ringBuffer);
				assert(buffer.beginOffs + buffer.size < ringBufSize);
			} else {
				assert(buffer.buffer    != 0);
				assert(buffer.beginOffs == 0);
			}
			// FIXME: descIndex is not right here
			glBindBufferRange(GL_SHADER_STORAGE_BUFFER, descIndex, buffer.buffer, buffer.beginOffs, buffer.size);
		} break;

		case DescriptorType::Sampler: {
			const auto &sampler = samplers.get(*reinterpret_cast<const SamplerHandle *>(data + l.offset));
			assert(sampler.sampler);
			glBindSampler(descIndex, sampler.sampler);
		} break;

		case DescriptorType::Texture: {
			TextureHandle texHandle = *reinterpret_cast<const TextureHandle *>(data + l.offset);
			const auto &tex = textures.get(texHandle);
			// FIXME: descIndex is not right here
			glBindTextureUnit(descIndex, tex.tex);
		} break;

		case DescriptorType::CombinedSampler: {
			const CSampler &combined = *reinterpret_cast<const CSampler *>(data + l.offset);

			const Texture &tex = textures.get(combined.tex);
			assert(tex.tex);

			const auto &sampler = samplers.get(combined.sampler);
			assert(sampler.sampler);

			// FIXME: descIndex is not right here
			glBindTextureUnit(descIndex, tex.tex);
			glBindSampler(descIndex, sampler.sampler);
		} break;

		case DescriptorType::Count:
			assert(false); // shouldn't happen
			break;

		}

		descIndex++;
	}
}


void RendererImpl::draw(unsigned int firstVertex, unsigned int vertexCount) {
	assert(inRenderPass);
	assert(validPipeline);
	assert(vertexCount > 0);
	assert(!currentPipeline.scissorTest_ || scissorSet);
	assert(currentPipeline.renderPass_ == currentRenderPass);
	pipelineDrawn = true;

	// TODO: get primitive from current pipeline
	glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);
}


void RendererImpl::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
	assert(inRenderPass);
	assert(validPipeline);
	assert(instanceCount > 0);
	assert(vertexCount > 0);
	assert(!currentPipeline.scissorTest_ || scissorSet);
	assert(currentPipeline.renderPass_ == currentRenderPass);
	pipelineDrawn = true;

	// TODO: get primitive from current pipeline
	GLenum format = idxBuf16Bit ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT ;
	auto ptr = reinterpret_cast<const void *>(indexBufByteOffset);
	if (instanceCount == 1) {
		glDrawElements(GL_TRIANGLES, vertexCount, format, ptr);
	} else {
		glDrawElementsInstanced(GL_TRIANGLES, vertexCount, format, ptr, instanceCount);
	}
}


void RendererImpl::drawIndexedOffset(unsigned int vertexCount, unsigned int firstIndex) {
	assert(inRenderPass);
	assert(validPipeline);
	assert(vertexCount > 0);
	assert(!currentPipeline.scissorTest_ || scissorSet);
	assert(currentPipeline.renderPass_ == currentRenderPass);
	pipelineDrawn = true;

	GLenum format        = idxBuf16Bit ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	unsigned int idxSize = idxBuf16Bit ? 2                 : 4 ;
	auto ptr = reinterpret_cast<const char *>(firstIndex * idxSize + indexBufByteOffset);
	// TODO: get primitive from current pipeline
	glDrawElements(GL_TRIANGLES, vertexCount, format, ptr);
}


} // namespace renderer


#endif //  RENDERER_OPENGL
