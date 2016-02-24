#include "Specter/View.hpp"
#include "Specter/ViewResources.hpp"
#include "Specter/RootView.hpp"

namespace Specter
{
static LogVisor::LogModule Log("Specter::View");

static const char* GLSLSolidVS =
"#version 330\n"
BOO_GLSL_BINDING_HEAD
"layout(location=0) in vec3 posIn;\n"
"layout(location=1) in vec4 colorIn;\n"
SPECTER_GLSL_VIEW_VERT_BLOCK
"struct VertToFrag\n"
"{\n"
"    vec4 color;\n"
"};\n"
"out VertToFrag vtf;\n"
"void main()\n"
"{\n"
"    vtf.color = colorIn * mulColor;\n"
"    gl_Position = mv * vec4(posIn, 1.0);\n"
"}\n";

static const char* GLSLSolidFS =
"#version 330\n"
"struct VertToFrag\n"
"{\n"
"    vec4 color;\n"
"};\n"
"in VertToFrag vtf;\n"
"layout(location=0) out vec4 colorOut;\n"
"void main()\n"
"{\n"
"    colorOut = vtf.color;\n"
"}\n";

static const char* GLSLTexVS =
"#version 330\n"
BOO_GLSL_BINDING_HEAD
"layout(location=0) in vec3 posIn;\n"
"layout(location=1) in vec2 uvIn;\n"
SPECTER_GLSL_VIEW_VERT_BLOCK
"struct VertToFrag\n"
"{\n"
"    vec4 color;\n"
"    vec2 uv;\n"
"};\n"
"out VertToFrag vtf;\n"
"void main()\n"
"{\n"
"    vtf.uv = uvIn;\n"
"    vtf.color = mulColor;\n"
"    gl_Position = mv * vec4(posIn, 1.0);\n"
"}\n";

static const char* GLSLTexFS =
"#version 330\n"
BOO_GLSL_BINDING_HEAD
"struct VertToFrag\n"
"{\n"
"    vec4 color;\n"
"    vec2 uv;\n"
"};\n"
"in VertToFrag vtf;\n"
"TBINDING0 uniform sampler2D tex;\n"
"layout(location=0) out vec4 colorOut;\n"
"void main()\n"
"{\n"
"    colorOut = texture(tex, vtf.uv) * vtf.color;\n"
"}\n";

void View::Resources::init(boo::GLDataFactory* factory, const IThemeData& theme)
{
    static const char* BlockNames[] = {"SpecterViewBlock"};

    m_solidShader = factory->newShaderPipeline(GLSLSolidVS, GLSLSolidFS, 0, nullptr, 1, BlockNames,
                                               boo::BlendFactor::SrcAlpha, boo::BlendFactor::InvSrcAlpha,
                                               false, false, false);

    m_texShader = factory->newShaderPipeline(GLSLTexVS, GLSLTexFS, 1, "tex", 1, BlockNames,
                                             boo::BlendFactor::SrcAlpha, boo::BlendFactor::InvSrcAlpha,
                                             false, false, false);
}

#if _WIN32

void View::Resources::init(boo::ID3DDataFactory* factory, const IThemeData& theme)
{
    static const char* SolidVS =
    "struct VertData\n"
    "{\n"
    "    float3 posIn : POSITION;\n"
    "    float4 colorIn : COLOR;\n"
    "};\n"
    SPECTER_HLSL_VIEW_VERT_BLOCK
    "struct VertToFrag\n"
    "{\n"
    "    float4 position : SV_Position;\n"
    "    float4 color : COLOR;\n"
    "};\n"
    "VertToFrag main(in VertData v)\n"
    "{\n"
    "    VertToFrag vtf;\n"
    "    vtf.color = v.colorIn * mulColor;\n"
    "    vtf.position = mul(mv, float4(v.posIn, 1.0));\n"
    "    return vtf;\n"
    "}\n";

    static const char* SolidFS =
    "struct VertToFrag\n"
    "{\n"
    "    float4 position : SV_Position;\n"
    "    float4 color : COLOR;\n"
    "};\n"
    "float4 main(in VertToFrag vtf) : SV_Target0\n"
    "{\n"
    "    return vtf.color;\n"
    "}\n";

    static const char* TexVS =
    "struct VertData\n"
    "{\n"
    "    float3 posIn : POSITION;\n"
    "    float2 uvIn : UV;\n"
    "};\n"
    SPECTER_HLSL_VIEW_VERT_BLOCK
    "struct VertToFrag\n"
    "{\n"
    "    float4 position : SV_Position;\n"
    "    float4 color : COLOR;\n"
    "    float2 uv : UV;\n"
    "};\n"
    "VertToFrag main(in VertData v)\n"
    "{\n"
    "    VertToFrag vtf;\n"
    "    vtf.uv = v.uvIn;\n"
    "    vtf.color = mulColor;\n"
    "    vtf.position = mul(mv, float4(v.posIn, 1.0));\n"
    "    return vtf;\n"
    "}\n";

    static const char* TexFS =
    "struct VertToFrag\n"
    "{\n"
    "    float4 position : SV_Position;\n"
    "    float4 color : COLOR;\n"
    "    float2 uv : UV;\n"
    "};\n"
    "Texture2D tex : register(t0);\n"
    "SamplerState samp : register(s0);\n"
    "float4 main(in VertToFrag vtf) : SV_Target0\n"
    "{\n"
    "    return tex.Sample(samp, vtf.uv) * vtf.color;\n"
    "}\n";

    boo::VertexElementDescriptor solidvdescs[] =
    {
        {nullptr, nullptr, boo::VertexSemantic::Position4},
        {nullptr, nullptr, boo::VertexSemantic::Color}
    };
    m_solidVtxFmt = factory->newVertexFormat(2, solidvdescs);

    ComPtr<ID3DBlob> vertBlob;
    ComPtr<ID3DBlob> fragBlob;
    ComPtr<ID3DBlob> pipeBlob;
    m_solidShader = factory->newShaderPipeline(SolidVS, SolidFS, vertBlob, fragBlob, pipeBlob, m_solidVtxFmt,
                                               boo::BlendFactor::SrcAlpha, boo::BlendFactor::InvSrcAlpha,
                                               false, false, false);

    boo::VertexElementDescriptor texvdescs[] =
    {
        {nullptr, nullptr, boo::VertexSemantic::Position4},
        {nullptr, nullptr, boo::VertexSemantic::UV4}
    };
    m_texVtxFmt = factory->newVertexFormat(2, texvdescs);

    vertBlob.Reset();
    fragBlob.Reset();
    pipeBlob.Reset();
    m_texShader = factory->newShaderPipeline(TexVS, TexFS, vertBlob, fragBlob, pipeBlob, m_texVtxFmt,
                                             boo::BlendFactor::SrcAlpha, boo::BlendFactor::InvSrcAlpha,
                                             false, false, false);
}

#endif
#if BOO_HAS_METAL

void View::Resources::init(boo::MetalDataFactory* factory, const IThemeData& theme)
{
    static const char* SolidVS =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct VertData\n"
    "{\n"
    "    float3 posIn [[ attribute(0) ]];\n"
    "    float4 colorIn [[ attribute(1) ]];\n"
    "};\n"
    SPECTER_VIEW_VERT_BLOCK_METAL
    "struct VertToFrag\n"
    "{\n"
    "    float4 position [[ position ]];\n"
    "    float4 color;\n"
    "};\n"
    "vertex VertToFrag vmain(VertData v [[ stage_in ]], constant SpecterViewBlock& view [[ buffer(2) ]])\n"
    "{\n"
    "    VertToFrag vtf;\n"
    "    vtf.color = v.colorIn * view.mulColor;\n"
    "    vtf.position = view.mv * float4(v.posIn, 1.0);\n"
    "    return vtf;\n"
    "}\n";

    static const char* SolidFS =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct VertToFrag\n"
    "{\n"
    "    float4 position [[ position ]];\n"
    "    float4 color;\n"
    "};\n"
    "fragment float4 fmain(VertToFrag vtf [[ stage_in ]])\n"
    "{\n"
    "    return vtf.color;\n"
    "}\n";

    static const char* TexVS =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct VertData\n"
    "{\n"
    "    float3 posIn [[ attribute(0) ]];\n"
    "    float2 uvIn [[ attribute(1) ]];\n"
    "};\n"
    SPECTER_VIEW_VERT_BLOCK_METAL
    "struct VertToFrag\n"
    "{\n"
    "    float4 position [[ position ]];\n"
    "    float4 color;\n"
    "    float2 uv;\n"
    "};\n"
    "vertex VertToFrag vmain(VertData v [[ stage_in ]], constant SpecterViewBlock& view [[ buffer(2) ]])\n"
    "{\n"
    "    VertToFrag vtf;\n"
    "    vtf.uv = v.uvIn;\n"
    "    vtf.color = view.mulColor;\n"
    "    vtf.position = view.mv * float4(v.posIn, 1.0);\n"
    "    return vtf;\n"
    "}\n";

    static const char* TexFS =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "constexpr sampler samp(address::repeat);\n"
    "struct VertToFrag\n"
    "{\n"
    "    float4 position [[ position ]];\n"
    "    float4 color;\n"
    "    float2 uv;\n"
    "};\n"
    "fragment float4 fmain(VertToFrag vtf [[ stage_in ]], texture2d<float> tex [[ texture(0) ]])\n"
    "{\n"
    "    return tex.sample(samp, vtf.uv) * vtf.color;\n"
    "}\n";

    boo::VertexElementDescriptor solidvdescs[] =
    {
        {nullptr, nullptr, boo::VertexSemantic::Position4},
        {nullptr, nullptr, boo::VertexSemantic::Color}
    };
    m_solidVtxFmt = factory->newVertexFormat(2, solidvdescs);

    m_solidShader = factory->newShaderPipeline(SolidVS, SolidFS, m_solidVtxFmt, 1,
                                               boo::BlendFactor::SrcAlpha, boo::BlendFactor::InvSrcAlpha,
                                               false, false, false);

    boo::VertexElementDescriptor texvdescs[] =
    {
        {nullptr, nullptr, boo::VertexSemantic::Position4},
        {nullptr, nullptr, boo::VertexSemantic::UV4}
    };
    m_texVtxFmt = factory->newVertexFormat(2, texvdescs);

    m_texShader = factory->newShaderPipeline(TexVS, TexFS, m_texVtxFmt, 1,
                                             boo::BlendFactor::SrcAlpha, boo::BlendFactor::InvSrcAlpha,
                                             false, false, false);
}

#endif
#if BOO_HAS_VULKAN

void View::Resources::init(boo::VulkanDataFactory* factory, const IThemeData& theme)
{
    boo::VertexElementDescriptor solidvdescs[] =
    {
        {nullptr, nullptr, boo::VertexSemantic::Position4},
        {nullptr, nullptr, boo::VertexSemantic::Color}
    };
    m_solidVtxFmt = factory->newVertexFormat(2, solidvdescs);

    m_solidShader = factory->newShaderPipeline(GLSLSolidVS, GLSLSolidFS, m_solidVtxFmt,
                                               boo::BlendFactor::SrcAlpha, boo::BlendFactor::InvSrcAlpha,
                                               false, false, false);

    boo::VertexElementDescriptor texvdescs[] =
    {
        {nullptr, nullptr, boo::VertexSemantic::Position4},
        {nullptr, nullptr, boo::VertexSemantic::UV4}
    };
    m_texVtxFmt = factory->newVertexFormat(2, texvdescs);

    m_texShader = factory->newShaderPipeline(GLSLTexVS, GLSLTexFS, m_texVtxFmt,
                                             boo::BlendFactor::SrcAlpha, boo::BlendFactor::InvSrcAlpha,
                                             false, false, false);
}

#endif

void View::buildResources(ViewResources& res)
{
    m_viewVertBlockBuf =
    res.m_factory->newDynamicBuffer(boo::BufferUse::Uniform,
                                    sizeof(ViewBlock), 1);
    m_bgVertsBinding.initSolid(res, 4, m_viewVertBlockBuf);
}

View::View(ViewResources& res)
: m_rootView(*static_cast<RootView*>(this)),
  m_parentView(*static_cast<RootView*>(this))
{
    buildResources(res);
}

View::View(ViewResources& res, View& parentView)
: m_rootView(parentView.rootView()),
  m_parentView(parentView)
{
    buildResources(res);
}

void View::updateSize()
{
    resized(m_rootView.rootRect(), m_subRect);
}

void View::resized(const boo::SWindowRect& root, const boo::SWindowRect& sub)
{
    m_subRect = sub;
    m_viewVertBlock.setViewRect(root, sub);
    m_bgRect[0].m_pos.assign(0.f, sub.size[1], 0.f);
    m_bgRect[1].m_pos.assign(0.f, 0.f, 0.f);
    m_bgRect[2].m_pos.assign(sub.size[0], sub.size[1], 0.f);
    m_bgRect[3].m_pos.assign(sub.size[0], 0.f, 0.f);
    m_viewVertBlockBuf->load(&m_viewVertBlock, sizeof(ViewBlock));
    m_bgVertsBinding.load(m_bgRect, sizeof(m_bgRect));
}

void View::resized(const ViewBlock& vb, const boo::SWindowRect& sub)
{
    m_subRect = sub;
    m_bgRect[0].m_pos.assign(0.f, sub.size[1], 0.f);
    m_bgRect[1].m_pos.assign(0.f, 0.f, 0.f);
    m_bgRect[2].m_pos.assign(sub.size[0], sub.size[1], 0.f);
    m_bgRect[3].m_pos.assign(sub.size[0], 0.f, 0.f);
    m_viewVertBlockBuf->load(&vb, sizeof(ViewBlock));
    m_bgVertsBinding.load(m_bgRect, sizeof(m_bgRect));
}

void View::draw(boo::IGraphicsCommandQueue* gfxQ)
{
    gfxQ->setShaderDataBinding(m_bgVertsBinding);
    gfxQ->draw(0, 4);
}

void View::commitResources(ViewResources& res)
{
    if (m_gfxData)
        Log.report(LogVisor::FatalError, "multiple resource commits not allowed");
    m_gfxData = res.m_factory->commit();
}

void View::VertexBufferBinding::initSolid(ViewResources& res, size_t count, boo::IGraphicsBuffer* viewBlockBuf)
{
    m_vertsBuf = res.m_factory->newDynamicBuffer(boo::BufferUse::Vertex, sizeof(SolidShaderVert), count);

    if (!res.m_viewRes.m_solidVtxFmt)
    {
        boo::VertexElementDescriptor vdescs[] =
        {
            {m_vertsBuf, nullptr, boo::VertexSemantic::Position4},
            {m_vertsBuf, nullptr, boo::VertexSemantic::Color}
        };
        m_vtxFmt = res.m_factory->newVertexFormat(2, vdescs);
        boo::IGraphicsBuffer* bufs[] = {viewBlockBuf};
        m_shaderBinding = res.m_factory->newShaderDataBinding(res.m_viewRes.m_solidShader,
                                                              m_vtxFmt, m_vertsBuf, nullptr,
                                                              nullptr, 1, bufs, 0, nullptr);
    }
    else
    {
        boo::IGraphicsBuffer* bufs[] = {viewBlockBuf};
        m_shaderBinding = res.m_factory->newShaderDataBinding(res.m_viewRes.m_solidShader,
                                                              res.m_viewRes.m_solidVtxFmt,
                                                              m_vertsBuf, nullptr,
                                                              nullptr, 1, bufs, 0, nullptr);
    }
}

void View::VertexBufferBinding::initTex(ViewResources& res, size_t count, boo::IGraphicsBuffer* viewBlockBuf, boo::ITexture* texture)
{
    m_vertsBuf = res.m_factory->newDynamicBuffer(boo::BufferUse::Vertex, sizeof(TexShaderVert), count);

    if (!res.m_viewRes.m_texVtxFmt)
    {
        boo::VertexElementDescriptor vdescs[] =
        {
            {m_vertsBuf, nullptr, boo::VertexSemantic::Position4},
            {m_vertsBuf, nullptr, boo::VertexSemantic::UV4}
        };
        m_vtxFmt = res.m_factory->newVertexFormat(2, vdescs);
        boo::IGraphicsBuffer* bufs[] = {viewBlockBuf};
        boo::ITexture* tex[] = {texture};
        m_shaderBinding = res.m_factory->newShaderDataBinding(res.m_viewRes.m_texShader,
                                                              m_vtxFmt, m_vertsBuf, nullptr,
                                                              nullptr, 1, bufs, 1, tex);
    }
    else
    {
        boo::IGraphicsBuffer* bufs[] = {viewBlockBuf};
        boo::ITexture* tex[] = {texture};
        m_shaderBinding = res.m_factory->newShaderDataBinding(res.m_viewRes.m_texShader,
                                                              res.m_viewRes.m_texVtxFmt,
                                                              m_vertsBuf, nullptr,
                                                              nullptr, 1, bufs, 1, tex);
    }
}


}
