# Reference wishlist — interop accessors that deserve a wrapped public API

These public methods return raw interop handles (sokol `sg_*`). They are real
symbols, so the reference now lists them — but a raw `sg_*` return is not a
user-facing API. Some are genuinely useful and *should* gain a properly-wrapped
(non-sokol) form. Track candidates here; add the wrapped method to core and
document it normally. The raw `sg_*` accessor can then be marked `TC_INTERNAL`
(see `tc/utils/tcAnnotations.h`) to drop it from the reference.

## Candidates for a wrapped public API

| symbol | returns | why it'd be useful | wrap idea |
|---|---|---|---|
| `Fbo::getColorImage()` | `sg_image` | read/share an FBO's rendered color attachment | expose as a `Texture` (already a wrapper type) — e.g. `Fbo::getTexture()` |
| `Mesh::getGpuVertexBuffer()` / `getGpuIndexBuffer()` | `sg_buffer` | hand a mesh's GPU buffers to custom shader/instancing code | a small `GpuBuffer` wrapper, or document only if a use-case lands |
| `Texture::getPixelFormat()` | `sg_pixel_format` | query a texture's format | return the public `TextureFormat` enum instead (`Texture::getFormat()`) |
| `toSokolFormat(TextureFormat)` | `sg_pixel_format` | TextureFormat → sokol; already public, used by apps | keep as-is for interop, or leave excluded (it's an interop bridge by definition) |

## Excluded as pure interop (no wrap planned)
`Texture::getView/getViewForMip/getAttachmentView/getAttachmentViewForMip/getCubemapFaceAttachmentView/getImage/getSampler`,
`Fbo::getTextureView/getSampler`, `Font::getSampler`, `IesProfile::getView/getSampler`
— these are sokol-binding plumbing for advanced/addon rendering; no user-facing need identified.
