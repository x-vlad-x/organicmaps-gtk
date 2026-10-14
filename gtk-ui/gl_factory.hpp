#pragma once

#include "drape/gl_includes.hpp"
#include "drape/graphics_context_factory.hpp"
#include "drape/oglcontext.hpp"

#include <gdk/gdk.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace lichen
{
// Offscreen colour+depth/stencil target living in the render thread's context.
// Only its texture is shared with the GTK context that composes the frame.
struct OffscreenFrame
{
  GLuint m_fbo = 0;
  GLuint m_texture = 0;
  GLuint m_depthStencil = 0;
  uint32_t m_width = 0;
  uint32_t m_height = 0;

  OffscreenFrame(uint32_t w, uint32_t h);
  ~OffscreenFrame();
};

// Drape renders on its own thread into GdkGLContexts created from the display (surfaceless,
// shared with every GTK context). Frames are triple buffered, like the Qt desktop app does.
class GtkRenderContext : public dp::OGLContext
{
public:
  GtkRenderContext(GdkGLContext * ctx, std::function<void()> onFrameReady);
  ~GtkRenderContext() override;

  void Present() override;
  void MakeCurrent() override;
  void DoneCurrent() override;
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override;
  void Resize(uint32_t w, uint32_t h) override;

  // Main thread: picks up the newest finished frame. Returns texture id (0 if none).
  GLuint AcquireFrame(uint32_t & w, uint32_t & h);

private:
  GdkGLContext * m_ctx;
  std::function<void()> m_onFrameReady;
  std::unique_ptr<OffscreenFrame> m_back;
  std::unique_ptr<OffscreenFrame> m_front;
  std::unique_ptr<OffscreenFrame> m_acquired;
  // Frames replaced on resize; deleted later on the render thread (FBOs aren't shared).
  std::vector<std::unique_ptr<OffscreenFrame>> m_garbage;
  bool m_frameUpdated = false;
  std::mutex m_frameMutex;
};

class GtkUploadContext : public dp::OGLContext
{
public:
  explicit GtkUploadContext(GdkGLContext * ctx);
  ~GtkUploadContext() override;

  void Present() override {}
  void MakeCurrent() override;
  void DoneCurrent() override;
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer>) override {}

private:
  GdkGLContext * m_ctx;
};

class GtkContextFactory : public dp::GraphicsContextFactory
{
public:
  // Contexts must be created and realized on the main thread.
  GtkContextFactory(GdkDisplay * display, std::function<void()> onFrameReady);
  ~GtkContextFactory() override;

  bool IsValid() const { return m_drawCtx && m_uploadCtx; }

  dp::GraphicsContext * GetDrawContext() override;
  dp::GraphicsContext * GetResourcesUploadContext() override;
  bool IsDrawContextCreated() const override { return true; }
  bool IsUploadContextCreated() const override { return true; }

  GLuint AcquireFrame(uint32_t & w, uint32_t & h);
  void PrepareToShutdown() { m_shutdown = true; }

private:
  static GdkGLContext * CreateContext(GdkDisplay * display);

  GdkGLContext * m_drawCtx = nullptr;
  GdkGLContext * m_uploadCtx = nullptr;
  std::unique_ptr<GtkRenderContext> m_draw;
  std::unique_ptr<GtkUploadContext> m_upload;
  std::atomic<bool> m_shutdown = false;
};
}  // namespace lichen
