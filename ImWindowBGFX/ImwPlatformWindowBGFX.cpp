#include "ImwPlatformWindowBGFX.h"

#include "ImwWindowManager.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/bx.h>
#include <bx/fpumath.h>
#include <bgfx/embedded_shader.h>

#include "imgui/vs_ocornut_imgui.bin.h"
#include "imgui/fs_ocornut_imgui.bin.h"

static const bgfx::EmbeddedShader s_embeddedShaders[] =
{
	BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
	BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),

	BGFX_EMBEDDED_SHADER_END()
};

using namespace ImWindow;

ImwPlatformWindowBGFX::ImwPlatformWindowBGFX(EPlatformWindowType eType, bool bCreateState, bgfx::RendererType::Enum eRenderer)
	: ImwPlatformWindowEasyWindow(eType, bCreateState)
{
	m_eRenderer = eRenderer;
}

ImwPlatformWindowBGFX::~ImwPlatformWindowBGFX()
{
	if (m_eType == E_PLATFORM_WINDOW_TYPE_MAIN)
	{
		bgfx::destroyProgram(m_hProgram);
		bgfx::destroyTexture(m_hTexture);
		bgfx::destroyUniform(m_hUniformTexture);
	}

	bgfx::destroyDynamicIndexBuffer(m_hIndexBuffer);
	bgfx::destroyDynamicVertexBuffer(m_hVertexBuffer);

	bgfx::destroyFrameBuffer(m_hFrameBufferHandle);
	bgfx::frame();
	bgfx::frame();
	ImwSafeDelete(m_pWindow);
}

inline bool checkAvailTransientBuffers(uint32_t _numVertices, const bgfx::VertexDecl& _decl, uint32_t _numIndices)
{
	return _numVertices == bgfx::getAvailTransientVertexBuffer(_numVertices, _decl)
		&& _numIndices == bgfx::getAvailTransientIndexBuffer(_numIndices)
		;
}

bool ImwPlatformWindowBGFX::Init(ImwPlatformWindow* pMain)
{
	if (ImwPlatformWindowEasyWindow::Init(pMain))
	{
		if (m_eType == E_PLATFORM_WINDOW_TYPE_MAIN)
		{
			bgfx::PlatformData pd;
			memset(&pd, 0, sizeof(pd));
			pd.nwh = m_pWindow->GetHandle();
			bgfx::setPlatformData(pd);

			bgfx::init(m_eRenderer);
			bgfx::reset(m_pWindow->GetClientWidth(), m_pWindow->GetClientHeight());
		}

		m_hFrameBufferHandle = bgfx::createFrameBuffer(m_pWindow->GetHandle(), uint16_t(m_pWindow->GetClientWidth()), uint16_t(m_pWindow->GetClientHeight()));
		bgfx::setViewFrameBuffer(255, m_hFrameBufferHandle);


		ImGuiIO& io = GetContext()->IO;

		if (pMain != NULL)
		{
			ImwPlatformWindowBGFX* pMainBGFX = ((ImwPlatformWindowBGFX*)pMain);
			m_hTexture = pMainBGFX->m_hTexture;
			m_hUniformTexture = pMainBGFX->m_hUniformTexture;
			m_hProgram = pMainBGFX->m_hProgram;
			m_oVertexDecl = pMainBGFX->m_oVertexDecl;
		}
		else
		{
			uint8_t* data;
			int32_t width;
			int32_t height;
			io.Fonts->AddFontDefault();
			io.Fonts->GetTexDataAsRGBA32(&data, &width, &height);

			m_hTexture = bgfx::createTexture2D(
				(uint16_t)width
				, (uint16_t)height
				, false
				, 1
				, bgfx::TextureFormat::BGRA8
				, 0
				, bgfx::copy(data, width*height * 4)
			);

			m_hUniformTexture = bgfx::createUniform("s_tex", bgfx::UniformType::Int1);

			m_hProgram = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, m_eRenderer, "vs_ocornut_imgui")
				, bgfx::createEmbeddedShader(s_embeddedShaders, m_eRenderer, "fs_ocornut_imgui")
				, true
			);

			m_oVertexDecl
				.begin()
				.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
				.end();
		}
		uint16_t iFlag = 0;
		if (sizeof(ImDrawIdx) == 4)
			iFlag |= BGFX_BUFFER_INDEX32;
		m_hVertexBuffer = bgfx::createDynamicVertexBuffer((uint32_t)1, m_oVertexDecl, BGFX_BUFFER_ALLOW_RESIZE);
		m_hIndexBuffer = bgfx::createDynamicIndexBuffer((uint32_t)1, BGFX_BUFFER_ALLOW_RESIZE | iFlag);

		return true;
	}

	return false;
}

void ImwPlatformWindowBGFX::OnSize(int iWidth, int iHeight)
{
	bgfx::frame();
	if (bgfx::isValid(m_hFrameBufferHandle))
		bgfx::destroyFrameBuffer(m_hFrameBufferHandle);
	m_hFrameBufferHandle = bgfx::createFrameBuffer(m_pWindow->GetHandle(), uint16_t(m_pWindow->GetClientWidth()), uint16_t(m_pWindow->GetClientHeight()));
	
	if (m_eType == E_PLATFORM_WINDOW_TYPE_MAIN)
	{
		bgfx::setViewFrameBuffer(255, m_hFrameBufferHandle);
		bgfx::reset(iWidth, iHeight);
	}
}


#define IMGUI_FLAGS_NONE        UINT8_C(0x00)
#define IMGUI_FLAGS_ALPHA_BLEND UINT8_C(0x01)

void ImwPlatformWindowBGFX::RenderDrawLists(ImDrawData* pDrawData)
{
	bgfx::reset(uint16_t(m_pWindow->GetClientWidth()), uint16_t(m_pWindow->GetClientHeight()));
	bgfx::setViewFrameBuffer(255, m_hFrameBufferHandle);
	bgfx::setViewRect(255, 0, 0, uint16_t(m_pWindow->GetClientWidth()), uint16_t(m_pWindow->GetClientHeight()));
	bgfx::setViewMode(255, bgfx::ViewMode::Sequential);

	const ImGuiIO& io = ImGui::GetIO();
	const float width = io.DisplaySize.x;
	const float height = io.DisplaySize.y;

	{
		float ortho[16];
		bx::mtxOrtho(ortho, 0.0f, width, height, 0.0f, -1.0f, 1.0f, 0.f, false);
		bgfx::setViewTransform(255, NULL, ortho);
	}

	{ // Copy all vertices/indices to monolithic arrays
		const bgfx::Memory* pVertexBuffer = bgfx::alloc(pDrawData->TotalVtxCount * sizeof(ImDrawVert));
		const bgfx::Memory* pIndexBuffer = bgfx::alloc(pDrawData->TotalIdxCount * sizeof(ImDrawIdx));
		uint32_t iVertexOffset = 0;
		uint32_t iIndexOffset = 0;
		for (int32_t ii = 0, num = pDrawData->CmdListsCount; ii < num; ++ii)
		{
			const ImDrawList* drawList = pDrawData->CmdLists[ ii ];
			uint32_t numVertices = (uint32_t)drawList->VtxBuffer.size();
			uint32_t numIndices = (uint32_t)drawList->IdxBuffer.size();
			bx::memCopy(((ImDrawVert*)pVertexBuffer->data) + iVertexOffset, drawList->VtxBuffer.begin(), numVertices * sizeof(ImDrawVert));
			bx::memCopy(((ImDrawIdx*)pIndexBuffer->data) + iIndexOffset, drawList->IdxBuffer.begin(), numIndices * sizeof(ImDrawIdx));

			iVertexOffset += numVertices;
			iIndexOffset += numIndices;
		}

		bgfx::updateDynamicVertexBuffer(m_hVertexBuffer, 0, pVertexBuffer);
		bgfx::updateDynamicIndexBuffer(m_hIndexBuffer, 0, pIndexBuffer);
	}

	// Render command lists
	uint32_t iVertexOffset = 0;
	uint32_t iIndexOffset = 0;
	for (int32_t ii = 0, num = pDrawData->CmdListsCount; ii < num; ++ii)
	{
		const ImDrawList* drawList = pDrawData->CmdLists[ii];
		uint32_t numVertices = (uint32_t)drawList->VtxBuffer.size();
		for (const ImDrawCmd* cmd = drawList->CmdBuffer.begin(), *cmdEnd = drawList->CmdBuffer.end(); cmd != cmdEnd; ++cmd)
		{
			if (cmd->UserCallback)
			{
				cmd->UserCallback(drawList, cmd);
			}
			else if (0 != cmd->ElemCount)
			{
				uint64_t state = 0
					| BGFX_STATE_RGB_WRITE
					| BGFX_STATE_ALPHA_WRITE
					| BGFX_STATE_MSAA
					;

				bgfx::TextureHandle th = m_hTexture;
				bgfx::ProgramHandle program = m_hProgram;

				if (NULL != cmd->TextureId)
				{
					union { ImTextureID ptr; struct { bgfx::TextureHandle handle; uint8_t flags; uint8_t mip; } s; } texture = { cmd->TextureId };
					state |= 0 != (IMGUI_FLAGS_ALPHA_BLEND & texture.s.flags)
						? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
						: BGFX_STATE_NONE
						;
					th = texture.s.handle;
				}
				else
				{
					state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
				}

				const uint16_t xx = uint16_t(bx::fmax(cmd->ClipRect.x, 0.0f));
				const uint16_t yy = uint16_t(bx::fmax(cmd->ClipRect.y, 0.0f));
				bgfx::setScissor(xx, yy
					, uint16_t(bx::fmin(cmd->ClipRect.z, 65535.0f) - xx)
					, uint16_t(bx::fmin(cmd->ClipRect.w, 65535.0f) - yy)
				);

				bgfx::setState(state);
				bgfx::setTexture(0, m_hUniformTexture, th);

				bgfx::setVertexBuffer(0, m_hVertexBuffer, iVertexOffset, numVertices);
				bgfx::setIndexBuffer(m_hIndexBuffer, iIndexOffset, cmd->ElemCount);
				bgfx::submit(255, program);
			}

			iIndexOffset += cmd->ElemCount;
		}
		iVertexOffset += numVertices;
	}

	bgfx::frame();
}