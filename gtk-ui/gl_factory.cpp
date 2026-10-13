#include "gl_factory.hpp"

#include "drape/framebuffer.hpp"
#include "drape/gl_functions.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

namespace omgtk
{
OffscreenFrame::OffscreenFrame(uint32_t w, uint32_t h) : m_width(w), m_height(h)
{
  glGenTextures(1, &m_texture);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenRenderbuffers(1, &m_depthStencil);
  glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencil);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glGenFramebuffers(1, &m_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencil);
  GLenum const status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE)
    LOG(LERROR, ("Offscreen framebuffer is incomplete:", status, w, h));
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

OffscreenFrame::~OffscreenFrame()
{
  glDeleteFramebuffers(1, &m_fbo);
  glDeleteRenderbuffers(1, &m_depthStencil);
  glDeleteTextures(1, &m_texture);
}

GtkRenderContext::GtkRenderContext(GdkGLContext * ctx, std::function<void()> onFrameReady)
  : m_ctx(GDK_GL_CONTEXT(g_object_ref(ctx)))
  , m_onFrameReady(std::move(onFrameReady))
{}

GtkRenderContext::~GtkRenderContext()
{
  // Frames must be released with the context current; the render thread does it in DoneCurrent
  // on shutdown, here we only drop what is left.
  gdk_gl_context_make_current(m_ctx);
  m_garbage.clear();
  m_back.reset();
  m_front.reset();
  m_acquired.reset();
  gdk_gl_context_clear_current();
  g_object_unref(m_ctx);
}

void GtkRenderContext::MakeCurrent()
{
  gdk_gl_context_make_current(m_ctx);
}

void GtkRenderContext::DoneCurrent()
{
  gdk_gl_context_clear_current();
}

void GtkRenderContext::Present()
{
  // Frames are consumed by another context, so the work must be finished before handing it over.
  GLFunctions::glFinish();

  {
    std::lock_guard lock(m_frameMutex);
    std::swap(m_front, m_back);
    m_frameUpdated = true;
  }
  if (m_onFrameReady)
    m_onFrameReady();
}

void GtkRenderContext::SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer)
{
  if (framebuffer)
    framebuffer->Bind();
  else if (m_back)
    glBindFramebuffer(GL_FRAMEBUFFER, m_back->m_fbo);
}

void GtkRenderContext::Resize(uint32_t w, uint32_t h)
{
  std::lock_guard lock(m_frameMutex);
  if (m_back && m_back->m_width == w && m_back->m_height == h)
    return;

  m_garbage.clear();
  if (m_back)
    m_garbage.push_back(std::move(m_back));
  if (m_front)
    m_garbage.push_back(std::move(m_front));
  if (m_acquired)
    m_garbage.push_back(std::move(m_acquired));

  m_back = std::make_unique<OffscreenFrame>(w, h);
  m_front = std::make_unique<OffscreenFrame>(w, h);
  m_acquired = std::make_unique<OffscreenFrame>(w, h);
  m_frameUpdated = false;
  glBindFramebuffer(GL_FRAMEBUFFER, m_back->m_fbo);
}

GLuint GtkRenderContext::AcquireFrame(uint32_t & w, uint32_t & h)
{
  std::lock_guard lock(m_frameMutex);
  if (m_frameUpdated)
  {
    std::swap(m_acquired, m_front);
    m_frameUpdated = false;
  }
  if (!m_acquired)
    return 0;
  w = m_acquired->m_width;
  h = m_acquired->m_height;
  return m_acquired->m_texture;
}

GtkUploadContext::GtkUploadContext(GdkGLContext * ctx) : m_ctx(GDK_GL_CONTEXT(g_object_ref(ctx))) {}

GtkUploadContext::~GtkUploadContext()
{
  g_object_unref(m_ctx);
}

void GtkUploadContext::MakeCurrent()
{
  gdk_gl_context_make_current(m_ctx);
}

void GtkUploadContext::DoneCurrent()
{
  gdk_gl_context_clear_current();
}

GtkContextFactory::GtkContextFactory(GdkDisplay * display, std::function<void()> onFrameReady)
{
  m_drawCtx = CreateContext(display);
  m_uploadCtx = CreateContext(display);
  if (m_drawCtx)
    m_draw = std::make_unique<GtkRenderContext>(m_drawCtx, std::move(onFrameReady));
  if (m_uploadCtx)
    m_upload = std::make_unique<GtkUploadContext>(m_uploadCtx);
}

GtkContextFactory::~GtkContextFactory()
{
  m_draw.reset();
  m_upload.reset();
  if (m_drawCtx)
    g_object_unref(m_drawCtx);
  if (m_uploadCtx)
    g_object_unref(m_uploadCtx);
}

GdkGLContext * GtkContextFactory::CreateContext(GdkDisplay * display)
{
  GError * error = nullptr;
  GdkGLContext * ctx = gdk_display_create_gl_context(display, &error);
  if (!ctx)
  {
    LOG(LERROR, ("gdk_display_create_gl_context failed:", error ? error->message : "?"));
    g_clear_error(&error);
    return nullptr;
  }
  gdk_gl_context_set_allowed_apis(ctx, GDK_GL_API_GLES);
  gdk_gl_context_set_required_version(ctx, 3, 0);
  if (!gdk_gl_context_realize(ctx, &error))
  {
    LOG(LERROR, ("gdk_gl_context_realize failed:", error ? error->message : "?"));
    g_clear_error(&error);
    g_object_unref(ctx);
    return nullptr;
  }
  return ctx;
}

dp::GraphicsContext * GtkContextFactory::GetDrawContext()
{
  return m_draw.get();
}

dp::GraphicsContext * GtkContextFactory::GetResourcesUploadContext()
{
  return m_upload.get();
}

GLuint GtkContextFactory::AcquireFrame(uint32_t & w, uint32_t & h)
{
  if (m_shutdown || !m_draw)
    return 0;
  return m_draw->AcquireFrame(w, h);
}
}  // namespace omgtk
