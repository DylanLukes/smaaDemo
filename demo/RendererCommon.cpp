#include "RendererInternal.h"
#include "Utils.h"


std::vector<char> RendererImpl::loadSource(const std::string &name) {
	auto it = shaderSources.find(name);
	if (it != shaderSources.end()) {
		return it->second;
	} else {
		auto source = readFile(name);
		shaderSources.emplace(name, source);
		return source;
	}
}


Renderer Renderer::createRenderer(const RendererDesc &desc) {
	return Renderer(new RendererImpl(desc));
}


Renderer &Renderer::operator=(Renderer &&other) {
	assert(impl == nullptr);
	impl = other.impl;
	other.impl = nullptr;

	return *this;
}


Renderer::Renderer(Renderer &&other)
: impl(other.impl)
{
	other.impl = nullptr;
}


Renderer::Renderer()
: impl(nullptr)
{
}


Renderer::Renderer(RendererImpl *impl_)
: impl(impl_)
{
	assert(impl != nullptr);
}


Renderer::~Renderer() {
	if (impl) {
		delete impl;
		impl = nullptr;
	}
}


BufferHandle Renderer::createBuffer(uint32_t size, const void *contents) {
	return impl->createBuffer(size, contents);
}


BufferHandle Renderer::createEphemeralBuffer(uint32_t size, const void *contents) {
	return impl->createEphemeralBuffer(size, contents);
}


FramebufferHandle Renderer::createFramebuffer(const FramebufferDesc &desc) {
	return impl->createFramebuffer(desc);
}


PipelineHandle Renderer::createPipeline(const PipelineDesc &desc) {
	return impl->createPipeline(desc);
}


RenderTargetHandle Renderer::createRenderTarget(const RenderTargetDesc &desc) {
	return impl->createRenderTarget(desc);
}


SamplerHandle Renderer::createSampler(const SamplerDesc &desc) {
	return impl->createSampler(desc);
}


VertexShaderHandle Renderer::createVertexShader(const std::string &name, const ShaderMacros &macros) {
	return impl->createVertexShader(name, macros);
}


FragmentShaderHandle Renderer::createFragmentShader(const std::string &name, const ShaderMacros &macros) {
	return impl->createFragmentShader(name, macros);
}


TextureHandle Renderer::createTexture(const TextureDesc &desc) {
	return impl->createTexture(desc);
}


void Renderer::deleteBuffer(BufferHandle handle) {
	impl->deleteBuffer(handle);
}


void Renderer::deleteFramebuffer(FramebufferHandle fbo) {
	impl->deleteFramebuffer(fbo);
}


void Renderer::deleteRenderTarget(RenderTargetHandle &rt) {
	impl->deleteRenderTarget(rt);
}


void Renderer::deleteSampler(SamplerHandle handle) {
	impl->deleteSampler(handle);
}


void Renderer::deleteTexture(TextureHandle handle) {
	impl->deleteTexture(handle);
}


void Renderer::recreateSwapchain(const SwapchainDesc &desc) {
	impl->recreateSwapchain(desc);
}


void Renderer::blitFBO(FramebufferHandle src, FramebufferHandle dest) {
	impl->blitFBO(src, dest);
}


void Renderer::beginFrame() {
	impl->beginFrame();
}


void Renderer::presentFrame(FramebufferHandle fbo) {
	impl->presentFrame(fbo);
}


void Renderer::beginRenderPass(FramebufferHandle fbo) {
	impl->beginRenderPass(fbo);
}


void Renderer::endRenderPass() {
	impl->endRenderPass();
}


void Renderer::bindFramebuffer(FramebufferHandle fbo) {
	impl->bindFramebuffer(fbo);
}


void Renderer::bindPipeline(PipelineHandle pipeline) {
	impl->bindPipeline(pipeline);
}


void Renderer::bindIndexBuffer(BufferHandle buffer, bool bit16) {
	impl->bindIndexBuffer(buffer, bit16);
}


void Renderer::bindVertexBuffer(unsigned int binding, BufferHandle buffer, unsigned int stride) {
	impl->bindVertexBuffer(binding, buffer, stride);
}


void Renderer::bindTexture(unsigned int unit, TextureHandle tex, SamplerHandle sampler) {
	impl->bindTexture(unit, tex, sampler);
}


void Renderer::bindUniformBuffer(unsigned int index, BufferHandle buffer) {
	impl->bindUniformBuffer(index, buffer);
}


void Renderer::bindStorageBuffer(unsigned int index, BufferHandle buffer) {
	impl->bindStorageBuffer(index, buffer);
}


void Renderer::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	impl->setViewport(x, y, width, height);
}


void Renderer::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
	impl->setScissorRect(x, y, width, height);
}


void Renderer::draw(unsigned int firstVertex, unsigned int vertexCount) {
	impl->draw(firstVertex, vertexCount);
}


void Renderer::drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount) {
	impl->drawIndexedInstanced(vertexCount, instanceCount);
}