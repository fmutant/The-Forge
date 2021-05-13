/*
*
* Copyright (c) 2018-2021 The Forge Interactive Inc.
*
* This file is part of The-Forge
* (see https://github.com/ConfettiFX/The-Forge).
*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
*/

// Unit Test for testing materials and pbr.

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/Atomics.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../../../Middleware_3/UI/AppUI.h"

//Input
#include "../../../../Common_3/OS/Core/ThreadSystem.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"

#define DEFERRED_RT_COUNT 4
//#define MAX_PLANES 4

// Have a uniform for camera data
struct UniformCamData
{
	mat4 mProjectView;
	mat4 mPrevProjectView;
	vec3 mCamPos;
	float mPad0;
};

// Have a uniform for extended camera data
struct UniformExtendedCamData
{
	mat4 mViewMat;
	mat4 mProjMat;
	mat4 mViewProjMat;
	mat4 mInvViewProjMat;

	vec4 mCameraWorldPos;
	vec4 mViewPortSize;

	mat4 mProjectViewInv;
	mat4 mPrevProjectViewInv;
};

// Have a uniform for object data
struct UniformObjData
{
	mat4  mWorldMat;
	mat4  mPrevWorldMat;
	float mRoughness = 0.04f;
	float mMetallic = 0.0f;
	int   pbrMaterials = -1;
};

struct UniformDataTerrain
{
	uint2 mViewportSize;
	uint mTerrainTilesCount;
	float mTerrainTilesCountInv;

	float mTerrainWidth;
	float mTerrainHeight;
	float mTerrainAlbedoMultiplier;
	float mPad0;
};

struct UniformDataAtmosphere
{
	float mRadiusBottom;
	float mRadiusTop;
	float mRadiusBottomSqr;
	float mRadiusTopSqr;

	float3 mScatteringRayleigh;
	float mDistanceBound;
	
	float3 mScatteringMie;
	float mPhaseFunctionGMie;
	
	float3 mExtinctionMie;
	float mSunAngularRadius;
	
	float3 mExtinctionAbsorption;
	float mTransmittanceHeightInv;

	float4 mDensityRayleigh[2];
	
	float2 mLayerWidthRayleigh;
	float2 mTransmittanceSizeInv;
	
	float4 mDensityMie[2];
	
	float2 mLayerWidthMie;
	float2 mIrradienceSizeInv;
	
	float4 mDensityAbsorption[2];
	
	float2 mLayerWidthAbsorption;
	float2 mPad2;

	float3 mSunDir{ 0.0f, 1.0f, 0.0f };
	float mPad3;
	
	float3 mViewDir{ 0.0f, 0.0f, 1.0f };
	float mPad4;

	float3 mSolarIrradience;
	float mPad5;
};

struct Light
{
	vec4  mPos;
	vec4  mCol;
	float mRadius;
	float mIntensity;
	float _pad0;
	float _pad1;
};

struct UniformLightData
{
	// Used to tell our shaders how many lights are currently present
	Light mLights[16];    // array of lights seem to be broken so just a single light for now
	int   mCurrAmountOfLights = 0;
};

struct DirectionalLight
{
	vec4 mPos;
	vec4 mCol;    //alpha is the intesity
	vec4 mDir;
};

struct UniformDirectionalLightData
{
	// Used to tell our shaders how many lights are currently present
	DirectionalLight mLights[16];    // array of lights seem to be broken so just a single light for now
	int              mCurrAmountOfDLights = 0;
};

#ifdef TARGET_IOS
static bool gUseTexturesFallback = false;
#else
const bool gUseTexturesFallback = false;    // cut off branching for other platforms
#endif

const char* pMaterialImageFileNames[] = {
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",

	//common
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/Dielectric_metallic",
	"SponzaPBR_Textures/Metallic_metallic",
	"SponzaPBR_Textures/gi_flag",

	//Background
	"SponzaPBR_Textures/Background/Background_Albedo",
	"SponzaPBR_Textures/Background/Background_Normal",
	"SponzaPBR_Textures/Background/Background_Roughness",

	//ChainTexture
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Albedo",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Metallic",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Normal",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Roughness",

	//Lion
	"SponzaPBR_Textures/Lion/Lion_Albedo",
	"SponzaPBR_Textures/Lion/Lion_Normal",
	"SponzaPBR_Textures/Lion/Lion_Roughness",

	//Sponza_Arch
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_diffuse",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_normal",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_roughness",

	//Sponza_Bricks
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Albedo",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Normal",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Roughness",

	//Sponza_Ceiling
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_diffuse",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_normal",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_roughness",

	//Sponza_Column
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_roughness",

	//Sponza_Curtain
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_metallic",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_roughness",

	//Sponza_Details
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_diffuse",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_metallic",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_normal",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_roughness",

	//Sponza_Fabric
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_metallic",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_roughness",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_normal",

	//Sponza_FlagPole
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_diffuse",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_normal",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_roughness",

	//Sponza_Floor
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_diffuse",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_normal",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_roughness",

	//Sponza_Roof
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_diffuse",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_normal",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_roughness",

	//Sponza_Thorn
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_diffuse",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_normal",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_roughness",

	//Vase
	"SponzaPBR_Textures/Vase/Vase_diffuse",
	"SponzaPBR_Textures/Vase/Vase_normal",
	"SponzaPBR_Textures/Vase/Vase_roughness",

	//VaseHanging
	"SponzaPBR_Textures/VaseHanging/VaseHanging_diffuse",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_normal",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_roughness",

	//VasePlant
	"SponzaPBR_Textures/VasePlant/VasePlant_diffuse",
	"SponzaPBR_Textures/VasePlant/VasePlant_normal",
	"SponzaPBR_Textures/VasePlant/VasePlant_roughness",

	//VaseRound
	"SponzaPBR_Textures/VaseRound/VaseRound_diffuse",
	"SponzaPBR_Textures/VaseRound/VaseRound_normal",
	"SponzaPBR_Textures/VaseRound/VaseRound_roughness",

	"lion/lion_albedo",
	"lion/lion_specular",
	"lion/lion_normal",

};

const uint32_t gImageCount = 3;
ProfileToken   gGpuProfileToken;
bool           gToggleVSync = false;

Renderer* pRenderer = NULL;
UIApp     gAppUI;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

SwapChain* pSwapChain = NULL;

RenderTarget* pRenderTargetDeferredPass[2][DEFERRED_RT_COUNT] = { { NULL }, { NULL } };

RenderTarget* pSceneBuffer = NULL;

RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*        pShaderBRDF = NULL;
Pipeline*      pPipelineBRDF = NULL;
RootSignature* pRootSigBRDF = NULL;
DescriptorSet* pDescriptorSetBRDF[2] = { NULL };

Buffer*        pSkyboxVertexBuffer = NULL;
Shader*        pSkyboxShader = NULL;
Pipeline*      pSkyboxPipeline = NULL;
RootSignature* pSkyboxRootSignature = NULL;
DescriptorSet* pDescriptorSetSkybox[2] = { NULL };

const static uint32_t gTerrainResolution = 512;
float gTerrainAlbedoMultiplier = 1.0f;
float gSunYaw = 0.0f;
float gSunPitch = 0.15f;
Shader* pTerrainShader = nullptr;
Pipeline* pTerrainPipeline = nullptr;
RootSignature* pTerrainRootSig = nullptr;
DescriptorSet* pTerrainDescriptorSet[2] = { nullptr };

Shader* pSkyShader = nullptr;
Pipeline* pSkyPipeline = nullptr;
RootSignature* pSkyRootSig = nullptr;
DescriptorSet* pSkyDescriptorSet[2] = { nullptr };

RenderTarget* pTransmittanceLUT = nullptr;
Shader* pTransmittanceShaderCompute = nullptr;
Pipeline* pTransmittancePipelineCompute = nullptr;
RootSignature* pTransmittanceRootSigCompute = nullptr;
DescriptorSet* pTransmittanceDescriptorSetCompute[2] = { nullptr };

RenderTarget* pIrradienceLUT = nullptr;
RenderTarget* pIrradienceDeltaLUT = nullptr;
Shader* pIrradienceShaderCompute = nullptr;
Pipeline* pIrradiencePipelineCompute = nullptr;
RootSignature* pIrradienceRootSigCompute = nullptr;
DescriptorSet* pIrradienceDescriptorSetCompute[2] = { nullptr };

Shader*        pPPR_HolePatchingShader = NULL;
RootSignature* pPPR_HolePatchingRootSignature = NULL;
Pipeline*      pPPR_HolePatchingPipeline = NULL;
DescriptorSet* pDescriptorSetPPR__HolePatching[1] = { NULL };

Buffer* pScreenQuadVertexBuffer = NULL;

Shader*        pShaderGbuffers = NULL;
Pipeline*      pPipelineGbuffers = NULL;
RootSignature* pRootSigGbuffers = NULL;
DescriptorSet* pDescriptorSetGbuffers[3] = { NULL };

Texture* pSkybox = NULL;
Texture* pTerrainTexture = nullptr;
Texture* pBRDFIntegrationMap = NULL;
Texture* pIrradianceMap = NULL;
Texture* pSpecularMap = NULL;

#define TOTAL_IMGS 84
Texture* pMaterialTextures[TOTAL_IMGS];

eastl::vector<int> gSponzaTextureIndexforMaterial;

//For clearing Intermediate Buffer
eastl::vector<uint32_t> gInitializeVal;

VirtualJoystickUI gVirtualJoystick;

UniformObjData gUniformDataMVP;

/************************************************************************/
// Vertex buffers for the model
/************************************************************************/

enum
{
	SPONZA_MODEL,
	LION_MODEL,
	MODEL_COUNT
};

Buffer* pSponzaBuffer;
Buffer* pLionBuffer;

Buffer*        pBufferUniformCamera[gImageCount] = { NULL };
UniformCamData gUniformDataCamera;

UniformCamData gUniformDataCameraSky;
UniformCamData gUniformDataCameraTerrain;
UniformDataTerrain gUniformDataTerrain;

UniformDataAtmosphere gUniformDataAtmosphere;
Buffer* pBufferUniformAtmosphere[gImageCount] = { nullptr };

Buffer*                pBufferUniformExtendedCamera[gImageCount] = { NULL };
UniformExtendedCamData gUniformDataExtendedCamera;

Buffer* pBufferUniformCameraSky[gImageCount] = { NULL };

Buffer* pBufferUniformCameraTerrain[gImageCount] = { nullptr };
Buffer* pBufferUniformTerrain[gImageCount] = { nullptr };

Buffer*          pBufferUniformLights = NULL;
UniformLightData gUniformDataLights;

Buffer*                     pBufferUniformDirectionalLights = NULL;
UniformDirectionalLightData gUniformDataDirectionalLights;

Shader*   pShaderPostProc = NULL;
Pipeline* pPipelinePostProc = NULL;

Sampler* pSamplerNearest = NULL;
Sampler* pSamplerBilinear = NULL;
Sampler* pSamplerLinearClamp = nullptr;

uint32_t gFrameIndex = 0;
uint32_t gFrameFlipFlop = 0;

eastl::vector<Buffer*> gSphereBuffers;

ICameraController* pCameraController = NULL;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

GuiComponent*    pGui = NULL;
GuiComponent*    pLoadingGui = NULL;

SyncToken gResourceSyncStartToken = {};
SyncToken gResourceSyncToken = {};

const char* pTextureName[] = { "albedoMap", "normalMap", "metallicMap", "roughnessMap", "aoMap" };

const char* gModelNames[2] = { "Sponza.gltf", "lion.gltf" };
Geometry*   gModels[2];
uint32_t    gMaterialIds[] = {
    0,  3,  1,  4,  5,  6,  7,  8,  6,  9,  7,  6, 10, 5, 7,  5, 6, 7,  6, 7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,
    5,  6,  5,  11, 5,  11, 5,  11, 5,  10, 5,  9, 8,  6, 12, 2, 5, 13, 0, 14, 15, 16, 14, 15, 14, 16, 15, 13, 17, 18, 19, 18, 19, 18, 17,
    19, 18, 17, 20, 21, 20, 21, 20, 21, 20, 21, 3, 1,  3, 1,  3, 1, 3,  1, 3,  1,  3,  1,  3,  1,  22, 23, 4,  23, 4,  5,  24, 5,
};

VertexLayout gVertexLayoutModel = {};

void assignSponzaTextures();

const char* gTestScripts[] = { "Test_RenderScene.lua" };
uint32_t gScriptIndexes[] = { 0 };
uint32_t gCurrentScriptIndex = 0;
void RunScript()
{
	gAppUI.RunTestScript(gTestScripts[gCurrentScriptIndex]);
}

class Sky : public IApp
{
	size_t mProgressBarValue = 0, mProgressBarValueMax = 1024;

	public:
	Sky()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}

	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		RendererDesc settings = { 0 };
		settings.mShaderTarget = ShaderTarget::shader_target_5_1;
		settings.mDxFeatureLevel = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_0;
		settings.mGpuMode = GpuMode::GPU_MODE_SINGLE;
		settings.mApi = RendererApi::RENDERER_API_D3D12;
		//settings.mEnableGPUBasedValidation = true;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

#ifdef TARGET_IOS
		gUseTexturesFallback = (pRenderer->pActiveGpuSettings->mArgumentBufferMaxTextures < TOTAL_IMGS);
#endif

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		// Create command pool and create a cmd buffer for each swapchain image
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		if (!gVirtualJoystick.Init(pRenderer, "circlepad"))
			return false;

		// Create UI
		if (!gAppUI.Init(pRenderer))
			return false;
		gAppUI.AddTestScripts(gTestScripts, sizeof(gTestScripts) / sizeof(gTestScripts[0]));

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");

		GuiDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.25f);
		pGui = gAppUI.AddGuiComponent("Clean Sponza", &guiDesc);

		initProfiler();
		initProfilerUI(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
		ComputePBRMaps();

		SamplerDesc samplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
									ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
		addSampler(pRenderer, &samplerDesc, &pSamplerBilinear);

		samplerDesc = { FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR,
									ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSamplerLinearClamp);

		SamplerDesc nearstSamplerDesc = { FILTER_NEAREST,      FILTER_NEAREST,      MIPMAP_MODE_NEAREST,
										  ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
		addSampler(pRenderer, &nearstSamplerDesc, &pSamplerNearest);

		// GBuffer
		char totalImagesShaderMacroBuffer[5] = {};
		sprintf(totalImagesShaderMacroBuffer, "%i", TOTAL_IMGS);
		ShaderMacro    totalImagesShaderMacro = { "TOTAL_IMGS", totalImagesShaderMacroBuffer };
		ShaderLoadDesc gBuffersShaderDesc = {};
		gBuffersShaderDesc.mStages[0] = { "fillGbuffers.vert", NULL, 0 };

		if (!gUseTexturesFallback)
		{
			gBuffersShaderDesc.mStages[1] = { "fillGbuffers.frag", &totalImagesShaderMacro, 1 };
		}
		else
		{
			gBuffersShaderDesc.mStages[1] = { "fillGbuffers_iOS.frag", NULL, 0 };
		}
		addShader(pRenderer, &gBuffersShaderDesc, &pShaderGbuffers);

		const char* pStaticSamplerNames[] = { "defaultSampler" };
		Sampler*    pStaticSamplers[] = { pSamplerBilinear };

		RootSignatureDesc gBuffersRootDesc = { &pShaderGbuffers, 1 };
		gBuffersRootDesc.mStaticSamplerCount = 1;
		gBuffersRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		gBuffersRootDesc.ppStaticSamplers = pStaticSamplers;

		if (!gUseTexturesFallback)
		{
			gBuffersRootDesc.mMaxBindlessTextures = TOTAL_IMGS;
		}
		addRootSignature(pRenderer, &gBuffersRootDesc, &pRootSigGbuffers);

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0 };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0 };
		addShader(pRenderer, &skyboxShaderDesc, &pSkyboxShader);

		const char*       pSkyboxamplerName = "skyboxSampler";
		RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = &pSkyboxamplerName;
		skyboxRootDesc.ppStaticSamplers = &pSamplerBilinear;
		addRootSignature(pRenderer, &skyboxRootDesc, &pSkyboxRootSignature);

		{
			ShaderLoadDesc shaderDescTerrain = {};
			shaderDescTerrain.mStages[0] = { "terrain.vert", nullptr, 0 };
			shaderDescTerrain.mStages[1] = { "terrain.frag", nullptr, 0 };
			addShader(pRenderer, &shaderDescTerrain, &pTerrainShader);
			
			const char* pSamplerNameTerrain = "samplerLinearClamp";
			RootSignatureDesc rootDescTerrain = { &pTerrainShader, 1 };
			rootDescTerrain.mStaticSamplerCount = 1;
			rootDescTerrain.ppStaticSamplerNames = &pSamplerNameTerrain;
			rootDescTerrain.ppStaticSamplers = &pSamplerLinearClamp;
			addRootSignature(pRenderer, &rootDescTerrain, &pTerrainRootSig);
		}

		{
			ShaderLoadDesc shaderDescSky = {};
			shaderDescSky.mStages[0] = { "fullscreenTriangle.vert", nullptr, 0 };
			shaderDescSky.mStages[1] = { "sky.frag", nullptr, 0 };
			addShader(pRenderer, &shaderDescSky, &pSkyShader);

			const char* pSamplerNameSky = "samplerLinearClamp";
			RootSignatureDesc rootDescSky = { &pSkyShader, 1 };
			rootDescSky.mStaticSamplerCount = 1;
			rootDescSky.ppStaticSamplerNames = &pSamplerNameSky;
			rootDescSky.ppStaticSamplers = &pSamplerLinearClamp;
			addRootSignature(pRenderer, &rootDescSky, &pSkyRootSig);
		}

		{
			ShaderLoadDesc shaderDescTransLUT = {};
			shaderDescTransLUT.mStages[0] = { "transmittanceLUT.comp", nullptr, 0 };
			addShader(pRenderer, &shaderDescTransLUT, &pTransmittanceShaderCompute);
			RootSignatureDesc rootDescTransLUT = { &pTransmittanceShaderCompute, 1 };
			rootDescTransLUT.mStaticSamplerCount = 0;
			addRootSignature(pRenderer, &rootDescTransLUT, &pTransmittanceRootSigCompute);
		}
		{
			ShaderLoadDesc shaderDescIrradLUT = {};
			shaderDescIrradLUT.mStages[0] = { "irradienceLUT.comp", nullptr, 0 };
			addShader(pRenderer, &shaderDescIrradLUT, &pIrradienceShaderCompute);
			RootSignatureDesc rootDescIrradLUT = { &pIrradienceShaderCompute, 1 };
			rootDescIrradLUT.mStaticSamplerCount = 0;
			addRootSignature(pRenderer, &rootDescIrradLUT, &pIrradienceRootSigCompute);
		}

		//BRDF
		ShaderLoadDesc brdfRenderSceneShaderDesc = {};
		brdfRenderSceneShaderDesc.mStages[0] = { "renderSceneBRDF.vert", NULL, 0 };
		brdfRenderSceneShaderDesc.mStages[1] = { "renderSceneBRDF.frag", NULL, 0 };
		addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);

		const char* pStaticSampler2Names[] = { "envSampler", "defaultSampler" };
		Sampler*    pStaticSamplers2[] = { pSamplerBilinear, pSamplerNearest };

		RootSignatureDesc brdfRootDesc = { &pShaderBRDF, 1 };
		brdfRootDesc.mStaticSamplerCount = 2;
		brdfRootDesc.ppStaticSamplerNames = pStaticSampler2Names;
		brdfRootDesc.ppStaticSamplers = pStaticSamplers2;
		addRootSignature(pRenderer, &brdfRootDesc, &pRootSigBRDF);

		// PPR_HolePatching
		ShaderLoadDesc PPR_HolePatchingShaderDesc = {};
		PPR_HolePatchingShaderDesc.mStages[0] = { "PPR_Holepatching.vert", NULL, 0 };
		PPR_HolePatchingShaderDesc.mStages[1] = { "PPR_Holepatching.frag", NULL, 0 };

		addShader(pRenderer, &PPR_HolePatchingShaderDesc, &pPPR_HolePatchingShader);

		const char* pStaticSamplerforHolePatchingNames[] = { "nearestSampler", "bilinearSampler" };
		Sampler*    pStaticSamplersforHolePatching[] = { pSamplerNearest, pSamplerBilinear };

		RootSignatureDesc PPR_HolePatchingRootDesc = { &pPPR_HolePatchingShader, 1 };
		PPR_HolePatchingRootDesc.mStaticSamplerCount = 2;
		PPR_HolePatchingRootDesc.ppStaticSamplerNames = pStaticSamplerforHolePatchingNames;
		PPR_HolePatchingRootDesc.ppStaticSamplers = pStaticSamplersforHolePatching;
		addRootSignature(pRenderer, &PPR_HolePatchingRootDesc, &pPPR_HolePatchingRootSignature);
		// Skybox
		DescriptorSetDesc setDesc = { pSkyboxRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
		setDesc = { pSkyboxRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[1]);
		{
			// terrain
			DescriptorSetDesc setDesc = { pTerrainRootSig, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &setDesc, &pTerrainDescriptorSet[0]);
			setDesc = { pTerrainRootSig, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
			addDescriptorSet(pRenderer, &setDesc, &pTerrainDescriptorSet[1]);
		}
		{
			// sky
			DescriptorSetDesc setDesc = { pSkyRootSig, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &setDesc, &pSkyDescriptorSet[0]);
			setDesc = { pSkyRootSig, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
			addDescriptorSet(pRenderer, &setDesc, &pSkyDescriptorSet[1]);
		}
		{
			// transmittance
			DescriptorSetDesc setDesc = { pTransmittanceRootSigCompute, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &setDesc, &pTransmittanceDescriptorSetCompute[0]);
			setDesc = { pTransmittanceRootSigCompute, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
			addDescriptorSet(pRenderer, &setDesc, &pTransmittanceDescriptorSetCompute[1]);
		}
		{
			// transmittance
			DescriptorSetDesc setDesc = { pIrradienceRootSigCompute, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &setDesc, &pIrradienceDescriptorSetCompute[0]);
			setDesc = { pIrradienceRootSigCompute, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
			addDescriptorSet(pRenderer, &setDesc, &pIrradienceDescriptorSetCompute[1]);
		}
		// GBuffer
		if (gUseTexturesFallback)
		{
			setDesc = { pRootSigGbuffers, DESCRIPTOR_UPDATE_FREQ_NONE, 512 };
		}
		else
		{
			setDesc = { pRootSigGbuffers, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		}
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGbuffers[0]);
		setDesc = { pRootSigGbuffers, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGbuffers[1]);
		setDesc = { pRootSigGbuffers, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGbuffers[2]);
		// BRDF
		setDesc = { pRootSigBRDF, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF[0]);
		setDesc = { pRootSigBRDF, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF[1]);
		// PPR Projection
		// PPR Hole Patching
		setDesc = { pPPR_HolePatchingRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPPR__HolePatching[0]);

		BufferLoadDesc sponza_buffDesc = {};
		sponza_buffDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		sponza_buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		sponza_buffDesc.mDesc.mSize = sizeof(UniformObjData);
		sponza_buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		sponza_buffDesc.pData = NULL;
		sponza_buffDesc.ppBuffer = &pSponzaBuffer;
		addResource(&sponza_buffDesc, NULL);

		BufferLoadDesc lion_buffDesc = {};
		lion_buffDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lion_buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lion_buffDesc.mDesc.mSize = sizeof(UniformObjData);
		lion_buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lion_buffDesc.pData = NULL;
		lion_buffDesc.ppBuffer = &pLionBuffer;
		addResource(&lion_buffDesc, NULL);

		//Generate sky box vertex buffer
		float skyBoxPoints[] = {
			0.5f,  -0.5f, -0.5f, 1.0f,    // -z
			-0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
			-0.5f, 1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

			-0.5f, -0.5f, 0.5f,  1.0f,    //-x
			-0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
			-0.5f, 1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

			0.5f,  -0.5f, -0.5f, 1.0f,    //+x
			0.5f,  -0.5f, 0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
			0.5f,  1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

			-0.5f, -0.5f, 0.5f,  1.0f,    // +z
			-0.5f, 0.5f,  0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
			0.5f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

			-0.5f, 0.5f,  -0.5f, 1.0f,    //+y
			0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
			0.5f,  1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,

			0.5f,  -0.5f, 0.5f,  1.0f,    //-y
			0.5f,  -0.5f, -0.5f, 1.0f,  -0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, -0.5f,
			-0.5f, 1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,
		};

		uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
		BufferLoadDesc skyboxVbDesc = {};
		skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		skyboxVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
		addResource(&skyboxVbDesc, NULL);

		float screenQuadPoints[] = {
			-1.0f, 3.0f, 0.5f, 0.0f, -1.0f, -1.0f, -1.0f, 0.5f, 0.0f, 1.0f, 3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
		};

		uint64_t       screenQuadDataSize = 5 * 3 * sizeof(float);
		BufferLoadDesc screenQuadVbDesc = {};
		screenQuadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		screenQuadVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		screenQuadVbDesc.mDesc.mSize = screenQuadDataSize;
		screenQuadVbDesc.pData = screenQuadPoints;
		screenQuadVbDesc.ppBuffer = &pScreenQuadVertexBuffer;
		addResource(&screenQuadVbDesc, NULL);

		// Uniform buffer for camera data
		BufferLoadDesc ubCamDesc = {};
		ubCamDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubCamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubCamDesc.mDesc.mSize = sizeof(UniformCamData);
		ubCamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubCamDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubCamDesc.ppBuffer = &pBufferUniformCamera[i];
			addResource(&ubCamDesc, NULL);
			ubCamDesc.ppBuffer = &pBufferUniformCameraSky[i];
			addResource(&ubCamDesc, NULL);
			ubCamDesc.ppBuffer = &pBufferUniformCameraTerrain[i];
			addResource(&ubCamDesc, NULL);
			ubCamDesc.ppBuffer = &pBufferUniformTerrain[i];
			addResource(&ubCamDesc, NULL);
		}

		{
			BufferLoadDesc ubAtmoDesc = {};
			ubAtmoDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ubAtmoDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			ubAtmoDesc.mDesc.mSize = sizeof(UniformDataAtmosphere);
			ubAtmoDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				ubAtmoDesc.ppBuffer = &pBufferUniformAtmosphere[i];
				addResource(&ubAtmoDesc, nullptr);
			}
		}

		// Uniform buffer for extended camera data
		BufferLoadDesc ubECamDesc = {};
		ubECamDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubECamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubECamDesc.mDesc.mSize = sizeof(UniformExtendedCamData);
		ubECamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubECamDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubECamDesc.ppBuffer = &pBufferUniformExtendedCamera[i];
			addResource(&ubECamDesc, NULL);
		}

		// Uniform buffer for light data
		BufferLoadDesc ubLightsDesc = {};
		ubLightsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ubLightsDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
		ubLightsDesc.mDesc.mSize = sizeof(UniformLightData);
		ubLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		ubLightsDesc.pData = NULL;
		ubLightsDesc.ppBuffer = &pBufferUniformLights;
		addResource(&ubLightsDesc, NULL);

		// Uniform buffer for DirectionalLight data
		BufferLoadDesc ubDLightsDesc = {};
		ubDLightsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ubDLightsDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
		ubDLightsDesc.mDesc.mSize = sizeof(UniformDirectionalLightData);
		ubDLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		ubDLightsDesc.pData = NULL;
		ubDLightsDesc.ppBuffer = &pBufferUniformDirectionalLights;
		addResource(&ubDLightsDesc, NULL);

		waitForAllResourceLoads();

		// prepare resources

		// Update the uniform buffer for the objects
		mat4 sponza_modelmat = mat4::translation(vec3(0.0f, -6.0f, 0.0f)) * mat4::scale(vec3(0.02f, 0.02f, 0.02f));
		gUniformDataMVP.mWorldMat = sponza_modelmat;
		gUniformDataMVP.mPrevWorldMat = gUniformDataMVP.mWorldMat;
		gUniformDataMVP.mMetallic = 0;
		gUniformDataMVP.mRoughness = 0.5f;
		gUniformDataMVP.pbrMaterials = 1;
		BufferUpdateDesc sponza_objBuffUpdateDesc = { pSponzaBuffer };
		beginUpdateResource(&sponza_objBuffUpdateDesc);
		*(UniformObjData*)sponza_objBuffUpdateDesc.pMappedData = gUniformDataMVP;
		endUpdateResource(&sponza_objBuffUpdateDesc, NULL);

		// Update the uniform buffer for the objects
		mat4 lion_modelmat = mat4::translation(vec3(0.0f, -6.0f, 1.0f)) * mat4::rotationY(-1.5708f) * mat4::scale(vec3(0.2f, 0.2f, -0.2f));
		gUniformDataMVP.mWorldMat = lion_modelmat;
		gUniformDataMVP.mPrevWorldMat = gUniformDataMVP.mWorldMat;
		gUniformDataMVP.mMetallic = 0;
		gUniformDataMVP.mRoughness = 0.5f;
		gUniformDataMVP.pbrMaterials = 1;
		BufferUpdateDesc lion_objBuffUpdateDesc = { pLionBuffer };
		beginUpdateResource(&lion_objBuffUpdateDesc);
		*(UniformObjData*)lion_objBuffUpdateDesc.pMappedData = gUniformDataMVP;
		endUpdateResource(&lion_objBuffUpdateDesc, NULL);

		// Add light to scene

		//Point light
		Light light;
		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(-12.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		gUniformDataLights.mLights[0] = light;

		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(-12.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		gUniformDataLights.mLights[1] = light;

		// Add light to scene
		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(9.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		gUniformDataLights.mLights[2] = light;

		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(9.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		gUniformDataLights.mLights[3] = light;

		gUniformDataLights.mCurrAmountOfLights = 4;
		BufferUpdateDesc lightBuffUpdateDesc = { pBufferUniformLights };
		beginUpdateResource(&lightBuffUpdateDesc);
		*(UniformLightData*)lightBuffUpdateDesc.pMappedData = gUniformDataLights;
		endUpdateResource(&lightBuffUpdateDesc, NULL);

		//Directional light
		DirectionalLight dLight;
		dLight.mCol = vec4(1.0f, 1.0f, 1.0f, 5.0f);
		dLight.mPos = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		dLight.mDir = vec4(-1.0f, -1.5f, 1.0f, 0.0f);

		gUniformDataDirectionalLights.mLights[0] = dLight;
		gUniformDataDirectionalLights.mCurrAmountOfDLights = 1;
		BufferUpdateDesc directionalLightBuffUpdateDesc = { pBufferUniformDirectionalLights };
		beginUpdateResource(&directionalLightBuffUpdateDesc);
		*(UniformDirectionalLightData*)directionalLightBuffUpdateDesc.pMappedData = gUniformDataDirectionalLights;
		endUpdateResource(&directionalLightBuffUpdateDesc, NULL);

#if !defined(TARGET_IOS)
		pGui->AddWidget(OneLineCheckboxWidget("Toggle VSync", &gToggleVSync, 0xFFFFFFFF));
#endif
		pGui->AddWidget(SliderFloatWidget("Sun pitch", &gSunPitch, -1.0f, 1.0f, 1.0f / 360.0f));
		pGui->AddWidget(SliderFloatWidget("Sun yaw", &gSunYaw, -1.0f, 1.0f, 1.0f / 360.0f));
		pGui->AddWidget(SliderFloat3Widget("SunDir", &gUniformDataAtmosphere.mSunDir, float3(-1.0f, -1.0f, -1.0f), float3(1.0f, 1.0f, 1.0f)));
		pGui->AddWidget(SliderFloat3Widget("ViewDir", &gUniformDataAtmosphere.mViewDir, float3(-1.0f, -1.0f, -1.0f), float3(1.0f, 1.0f, 1.0f)));

		GuiDesc guiDesc2 = {};
		guiDesc2.mStartPosition = vec2(mSettings.mWidth * 0.15f, mSettings.mHeight * 0.25f);
		pLoadingGui = gAppUI.AddGuiComponent("Loading", &guiDesc2);

		ProgressBarWidget ProgressBar("               [ProgressBar]               ", &mProgressBarValue, mProgressBarValueMax);
		pLoadingGui->AddWidget(ProgressBar);

		CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ 20.0f, 5.0f, 0.9f };
		vec3                   lookAt{ 20.0f, 5.0f, 1.0f };

		pCameraController = createFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(camParameters);

		// fill Gbuffers
		// Create vertex layout
		gVertexLayoutModel.mAttribCount = 3;

		gVertexLayoutModel.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gVertexLayoutModel.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutModel.mAttribs[0].mBinding = 0;
		gVertexLayoutModel.mAttribs[0].mLocation = 0;
		gVertexLayoutModel.mAttribs[0].mOffset = 0;

		//normals
		gVertexLayoutModel.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gVertexLayoutModel.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutModel.mAttribs[1].mLocation = 1;
		gVertexLayoutModel.mAttribs[1].mBinding = 0;
		gVertexLayoutModel.mAttribs[1].mOffset = 3 * sizeof(float);

		//texture
		gVertexLayoutModel.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gVertexLayoutModel.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gVertexLayoutModel.mAttribs[2].mLocation = 2;
		gVertexLayoutModel.mAttribs[2].mBinding = 0;
		gVertexLayoutModel.mAttribs[2].mOffset = 6 * sizeof(float);    // first attribute contains 3 floats

		for (size_t i = 0; i < TOTAL_IMGS; i += 1)
		{
			loadTexture(i);
		}

		for (size_t i = 0; i < 2; i += 1)
		{
			loadMesh(i);
		}
		//terrain heightmap
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.pFileName = "SponzaPBR_Textures/heightmap_zero";
			textureDesc.ppTexture = &pTerrainTexture;
			addResource(&textureDesc, &gResourceSyncToken);
		}
		gResourceSyncStartToken = getLastTokenCompleted();

		assignSponzaTextures();

		if (!initInputSystem(pWindow))
			return false;

		// App Actions
		InputActionDesc actionDesc = { InputBindings::BUTTON_FULLSCREEN,
									   [](InputActionContext* ctx) {
										   toggleFullscreen(((IApp*)ctx->pUserData)->pWindow);
										   return true;
									   },
									   this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) {
						  requestShutdown();
						  return true;
					  } };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_ANY,
					   [](InputActionContext* ctx) {
						   bool capture = gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition);
						   setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
						   return true;
					   },
					   this };
		addInputAction(&actionDesc);
		typedef bool (*CameraInputHandler)(InputActionContext * ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index) {
			if (!gAppUI.IsFocused() && *ctx->pCaptured)
			{
				gVirtualJoystick.OnMove(index, ctx->mPhase != INPUT_ACTION_PHASE_CANCELED, ctx->pPosition);
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = {
			InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 1.0f
		};
		addInputAction(&actionDesc);
		actionDesc = {
			InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f
		};
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) {
						  pCameraController->resetView();
						  return true;
					  } };
		addInputAction(&actionDesc);

		// Prepare descriptor sets
		DescriptorData skyParams[1] = {};
		skyParams[0].pName = "skyboxTex";
		skyParams[0].ppTextures = &pSkybox;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetSkybox[0], 1, skyParams);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			skyParams[0].pName = "uniformBlock";
			skyParams[0].ppBuffers = &pBufferUniformCameraSky[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetSkybox[1], 1, skyParams);
		}

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		// Remove streamer before removing any actual resources
		// otherwise we might delete a resource while uploading to it.
		waitForToken(&gResourceSyncToken);
		waitForAllResourceLoads();

		exitInputSystem();
		destroyCameraController(pCameraController);

		exitProfilerUI();

		exitProfiler();

		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
		removeDescriptorSet(pRenderer, pTerrainDescriptorSet[0]);
		removeDescriptorSet(pRenderer, pTerrainDescriptorSet[1]);
		removeDescriptorSet(pRenderer, pSkyDescriptorSet[0]);
		removeDescriptorSet(pRenderer, pSkyDescriptorSet[1]);
		removeDescriptorSet(pRenderer, pTransmittanceDescriptorSetCompute[0]);
		removeDescriptorSet(pRenderer, pTransmittanceDescriptorSetCompute[1]);
		removeDescriptorSet(pRenderer, pIrradienceDescriptorSetCompute[0]);
		removeDescriptorSet(pRenderer, pIrradienceDescriptorSetCompute[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetGbuffers[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetGbuffers[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetGbuffers[2]);
		removeDescriptorSet(pRenderer, pDescriptorSetBRDF[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetBRDF[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetPPR__HolePatching[0]);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeResource(pSpecularMap);
		removeResource(pIrradianceMap);
		removeResource(pSkybox);
		removeResource(pTerrainTexture);
		removeResource(pBRDFIntegrationMap);

		removeResource(pSponzaBuffer);
		removeResource(pLionBuffer);

		gVirtualJoystick.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pBufferUniformExtendedCamera[i]);
			removeResource(pBufferUniformCameraSky[i]);
			removeResource(pBufferUniformCameraTerrain[i]);
			removeResource(pBufferUniformTerrain[i]);
			removeResource(pBufferUniformCamera[i]);

			removeResource(pBufferUniformAtmosphere[i]);
		}

		removeResource(pBufferUniformLights);
		removeResource(pBufferUniformDirectionalLights);
		removeResource(pSkyboxVertexBuffer);
		removeResource(pScreenQuadVertexBuffer);

		for (Geometry*& model : gModels)
			removeResource(model);

		gAppUI.Exit();

		removeShader(pRenderer, pPPR_HolePatchingShader);
		removeShader(pRenderer, pShaderBRDF);
		removeShader(pRenderer, pSkyboxShader);
		removeShader(pRenderer, pTerrainShader);
		removeShader(pRenderer, pSkyShader);
		removeShader(pRenderer, pTransmittanceShaderCompute);
		removeShader(pRenderer, pIrradienceShaderCompute);
		removeShader(pRenderer, pShaderGbuffers);

		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerLinearClamp);
		removeSampler(pRenderer, pSamplerNearest);

		removeRootSignature(pRenderer, pPPR_HolePatchingRootSignature);
		removeRootSignature(pRenderer, pRootSigBRDF);
		removeRootSignature(pRenderer, pSkyboxRootSignature);
		removeRootSignature(pRenderer, pTerrainRootSig);
		removeRootSignature(pRenderer, pSkyRootSig);
		removeRootSignature(pRenderer, pTransmittanceRootSigCompute);
		removeRootSignature(pRenderer, pIrradienceRootSigCompute);
		removeRootSignature(pRenderer, pRootSigGbuffers);

		// Remove commands and command pool&
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}

		removeQueue(pRenderer, pGraphicsQueue);

		for (uint i = 0; i < TOTAL_IMGS; ++i)
		{
			if (pMaterialTextures[i])
			{
				removeResource(pMaterialTextures[i]);
			}
		}

		// Remove resource loader and renderer
		exitResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);

		gSponzaTextureIndexforMaterial.set_capacity(0);
		gInitializeVal.set_capacity(0);
	}

	void ComputePBRMaps()
	{
		Texture*       pPanoSkybox = NULL;
		Shader*        pPanoToCubeShader = NULL;
		RootSignature* pPanoToCubeRootSignature = NULL;
		Pipeline*      pPanoToCubePipeline = NULL;
		Shader*        pBRDFIntegrationShader = NULL;
		RootSignature* pBRDFIntegrationRootSignature = NULL;
		Pipeline*      pBRDFIntegrationPipeline = NULL;
		Shader*        pIrradianceShader = NULL;
		RootSignature* pIrradianceRootSignature = NULL;
		Pipeline*      pIrradiancePipeline = NULL;
		Shader*        pSpecularShader = NULL;
		RootSignature* pSpecularRootSignature = NULL;
		Pipeline*      pSpecularPipeline = NULL;
		Sampler*       pSkyboxSampler = NULL;
		DescriptorSet* pDescriptorSetBRDF = { NULL };
		DescriptorSet* pDescriptorSetPanoToCube[2] = { NULL };
		DescriptorSet* pDescriptorSetIrradiance = { NULL };
		DescriptorSet* pDescriptorSetSpecular[2] = { NULL };

		static const int skyboxIndex = 0;
		const char*      skyboxNames[] = {
            "LA_Helipad",
		};
		// PBR Texture values (these values are mirrored on the shaders).
		static const uint32_t gBRDFIntegrationSize = 512;
		static const uint32_t gSkyboxSize = 1024;
		static const uint32_t gSkyboxMips = 11;
		static const uint32_t gIrradianceSize = 32;
		static const uint32_t gSpecularSize = 128;
		static const uint32_t gSpecularMips = (uint)log2(gSpecularSize) + 1;

		SamplerDesc samplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		// Load the skybox panorama texture.
		SyncToken       token = {};
		TextureLoadDesc panoDesc = {};
		panoDesc.pFileName = skyboxNames[skyboxIndex];
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc, &token);

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pName = "skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pSkybox;
		addResource(&skyboxLoadDesc, NULL);

		TextureDesc irrImgDesc = {};
		irrImgDesc.mArraySize = 6;
		irrImgDesc.mDepth = 1;
		irrImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		irrImgDesc.mHeight = gIrradianceSize;
		irrImgDesc.mWidth = gIrradianceSize;
		irrImgDesc.mMipLevels = 1;
		irrImgDesc.mSampleCount = SAMPLE_COUNT_1;
		irrImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		irrImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		irrImgDesc.pName = "irrImgBuff";

		TextureLoadDesc irrLoadDesc = {};
		irrLoadDesc.pDesc = &irrImgDesc;
		irrLoadDesc.ppTexture = &pIrradianceMap;
		addResource(&irrLoadDesc, NULL);

		TextureDesc specImgDesc = {};
		specImgDesc.mArraySize = 6;
		specImgDesc.mDepth = 1;
		specImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		specImgDesc.mHeight = gSpecularSize;
		specImgDesc.mWidth = gSpecularSize;
		specImgDesc.mMipLevels = gSpecularMips;
		specImgDesc.mSampleCount = SAMPLE_COUNT_1;
		specImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		specImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		specImgDesc.pName = "specImgBuff";

		TextureLoadDesc specImgLoadDesc = {};
		specImgLoadDesc.pDesc = &specImgDesc;
		specImgLoadDesc.ppTexture = &pSpecularMap;
		addResource(&specImgLoadDesc, NULL);

		// Create empty texture for BRDF integration map.
		TextureLoadDesc brdfIntegrationLoadDesc = {};
		TextureDesc     brdfIntegrationDesc = {};
		brdfIntegrationDesc.mWidth = gBRDFIntegrationSize;
		brdfIntegrationDesc.mHeight = gBRDFIntegrationSize;
		brdfIntegrationDesc.mDepth = 1;
		brdfIntegrationDesc.mArraySize = 1;
		brdfIntegrationDesc.mMipLevels = 1;
		brdfIntegrationDesc.mFormat = TinyImageFormat_R32G32_SFLOAT;
		brdfIntegrationDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		brdfIntegrationDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
		brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
		brdfIntegrationLoadDesc.ppTexture = &pBRDFIntegrationMap;
		addResource(&brdfIntegrationLoadDesc, NULL);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0 };

		GPUPresetLevel presetLevel = pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel;
		uint32_t       importanceSampleCounts[GPUPresetLevel::GPU_PRESET_COUNT] = { 0, 0, 64, 128, 256, 1024 };
		uint32_t       importanceSampleCount = importanceSampleCounts[presetLevel];
		char           importanceSampleCountBuffer[5] = {};
		sprintf(importanceSampleCountBuffer, "%u", importanceSampleCount);
		ShaderMacro importanceSampleMacro = { "IMPORTANCE_SAMPLE_COUNT", importanceSampleCountBuffer };

		float irradianceSampleDeltas[GPUPresetLevel::GPU_PRESET_COUNT] = { 0, 0, 0.25f, 0.025f, 0.025f, 0.025f };
		float irradianceSampleDelta = irradianceSampleDeltas[presetLevel];
		char  irradianceSampleDeltaBuffer[10] = {};
		sprintf(irradianceSampleDeltaBuffer, "%f", irradianceSampleDelta);
		ShaderMacro irradianceSampleMacro = { "SAMPLE_DELTA", irradianceSampleDeltaBuffer };

		ShaderLoadDesc brdfIntegrationShaderDesc = {};
		brdfIntegrationShaderDesc.mStages[0] = { "BRDFIntegration.comp", &importanceSampleMacro, 1 };

		ShaderLoadDesc irradianceShaderDesc = {};
		irradianceShaderDesc.mStages[0] = { "computeIrradianceMap.comp", &irradianceSampleMacro, 1 };

		ShaderLoadDesc specularShaderDesc = {};
		specularShaderDesc.mStages[0] = { "computeSpecularMap.comp", &importanceSampleMacro, 1 };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);
		addShader(pRenderer, &irradianceShaderDesc, &pIrradianceShader);
		addShader(pRenderer, &specularShaderDesc, &pSpecularShader);
		addShader(pRenderer, &brdfIntegrationShaderDesc, &pBRDFIntegrationShader);

		const char*       pStaticSamplerNames[] = { "skyboxSampler" };
		RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
		panoRootDesc.mStaticSamplerCount = 1;
		panoRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		panoRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc brdfRootDesc = { &pBRDFIntegrationShader, 1 };
		brdfRootDesc.mStaticSamplerCount = 1;
		brdfRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		brdfRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc irradianceRootDesc = { &pIrradianceShader, 1 };
		irradianceRootDesc.mStaticSamplerCount = 1;
		irradianceRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		irradianceRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc specularRootDesc = { &pSpecularShader, 1 };
		specularRootDesc.mStaticSamplerCount = 1;
		specularRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		specularRootDesc.ppStaticSamplers = &pSkyboxSampler;
		addRootSignature(pRenderer, &panoRootDesc, &pPanoToCubeRootSignature);
		addRootSignature(pRenderer, &irradianceRootDesc, &pIrradianceRootSignature);
		addRootSignature(pRenderer, &specularRootDesc, &pSpecularRootSignature);
		addRootSignature(pRenderer, &brdfRootDesc, &pBRDFIntegrationRootSignature);

		DescriptorSetDesc setDesc = { pBRDFIntegrationRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF);
		setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPanoToCube[0]);
		setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gSkyboxMips };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPanoToCube[1]);
		setDesc = { pIrradianceRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetIrradiance);
		setDesc = { pSpecularRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSpecular[0]);
		setDesc = { pSpecularRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gSkyboxMips };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSpecular[1]);

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& pipelineSettings = desc.mComputeDesc;
		pipelineSettings.pShaderProgram = pPanoToCubeShader;
		pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
		addPipeline(pRenderer, &desc, &pPanoToCubePipeline);
		pipelineSettings.pShaderProgram = pIrradianceShader;
		pipelineSettings.pRootSignature = pIrradianceRootSignature;
		addPipeline(pRenderer, &desc, &pIrradiancePipeline);
		pipelineSettings.pShaderProgram = pSpecularShader;
		pipelineSettings.pRootSignature = pSpecularRootSignature;
		addPipeline(pRenderer, &desc, &pSpecularPipeline);
		pipelineSettings.pShaderProgram = pBRDFIntegrationShader;
		pipelineSettings.pRootSignature = pBRDFIntegrationRootSignature;
		addPipeline(pRenderer, &desc, &pBRDFIntegrationPipeline);

		waitForToken(&token);

		Cmd* pCmd = pCmds[0];

		// Compute the BRDF Integration map.
		resetCmdPool(pRenderer, pCmdPools[0]);
		beginCmd(pCmd);

		cmdBindPipeline(pCmd, pBRDFIntegrationPipeline);
		DescriptorData params[2] = {};
		params[0].pName = "dstTexture";
		params[0].ppTextures = &pBRDFIntegrationMap;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetBRDF, 1, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetBRDF);
		const uint32_t* pThreadGroupSize = pBRDFIntegrationShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(pCmd, gBRDFIntegrationSize / pThreadGroupSize[0], gBRDFIntegrationSize / pThreadGroupSize[1], pThreadGroupSize[2]);

		TextureBarrier srvBarrier[1] = { { pBRDFIntegrationMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };

		cmdResourceBarrier(pCmd, 0, NULL, 1, srvBarrier, 0, NULL);

		// Store the panorama texture inside a cubemap.
		cmdBindPipeline(pCmd, pPanoToCubePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pPanoSkybox;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetPanoToCube[0], 1, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPanoToCube[0]);

		struct
		{
			uint32_t mip;
			uint32_t textureSize;
		} rootConstantData = { 0, gSkyboxSize };

		for (uint32_t i = 0; i < gSkyboxMips; ++i)
		{
			rootConstantData.mip = i;
			cmdBindPushConstants(pCmd, pPanoToCubeRootSignature, "RootConstant", &rootConstantData);
			params[0].pName = "dstTexture";
			params[0].ppTextures = &pSkybox;
			params[0].mUAVMipSlice = i;
			updateDescriptorSet(pRenderer, i, pDescriptorSetPanoToCube[1], 1, params);
			cmdBindDescriptorSet(pCmd, i, pDescriptorSetPanoToCube[1]);

			pThreadGroupSize = pPanoToCubeShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				pCmd, max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] = { { pSkybox, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(pCmd, 0, NULL, 1, srvBarriers, 0, NULL);
		/************************************************************************/
		// Compute sky irradiance
		/************************************************************************/
		params[0] = {};
		params[1] = {};
		cmdBindPipeline(pCmd, pIrradiancePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pSkybox;
		params[1].pName = "dstTexture";
		params[1].ppTextures = &pIrradianceMap;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetIrradiance, 2, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetIrradiance);
		pThreadGroupSize = pIrradianceShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(pCmd, gIrradianceSize / pThreadGroupSize[0], gIrradianceSize / pThreadGroupSize[1], 6);

		/************************************************************************/
		// Compute specular sky
		/************************************************************************/
		cmdBindPipeline(pCmd, pSpecularPipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pSkybox;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetSpecular[0], 1, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetSpecular[0]);

		struct PrecomputeSkySpecularData
		{
			uint  mipSize;
			float roughness;
		};

		for (uint32_t i = 0; i < gSpecularMips; i++)
		{
			PrecomputeSkySpecularData data = {};
			data.roughness = (float)i / (float)(gSpecularMips - 1);
			data.mipSize = gSpecularSize >> i;
			cmdBindPushConstants(pCmd, pSpecularRootSignature, "RootConstant", &data);
			params[0].pName = "dstTexture";
			params[0].ppTextures = &pSpecularMap;
			params[0].mUAVMipSlice = i;
			updateDescriptorSet(pRenderer, i, pDescriptorSetSpecular[1], 1, params);
			cmdBindDescriptorSet(pCmd, i, pDescriptorSetSpecular[1]);
			pThreadGroupSize = pIrradianceShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(pCmd, max(1u, (gSpecularSize >> i) / pThreadGroupSize[0]), max(1u, (gSpecularSize >> i) / pThreadGroupSize[1]), 6);
		}
		/************************************************************************/
		/************************************************************************/
		TextureBarrier srvBarriers2[2] = { { pIrradianceMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
										   { pSpecularMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(pCmd, 0, NULL, 2, srvBarriers2, 0, NULL);

		endCmd(pCmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = &pCmd;
		submitDesc.pSignalFence = pRenderCompleteFences[0];
		submitDesc.mSubmitDone = true;
		queueSubmit(pGraphicsQueue, &submitDesc);
		waitForFences(pRenderer, 1, &pRenderCompleteFences[0]);

		removeDescriptorSet(pRenderer, pDescriptorSetBRDF);
		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetIrradiance);
		removeDescriptorSet(pRenderer, pDescriptorSetSpecular[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSpecular[1]);
		removePipeline(pRenderer, pSpecularPipeline);
		removeRootSignature(pRenderer, pSpecularRootSignature);
		removeShader(pRenderer, pSpecularShader);
		removePipeline(pRenderer, pIrradiancePipeline);
		removeRootSignature(pRenderer, pIrradianceRootSignature);
		removeShader(pRenderer, pIrradianceShader);
		removePipeline(pRenderer, pPanoToCubePipeline);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);

		removePipeline(pRenderer, pBRDFIntegrationPipeline);
		removeRootSignature(pRenderer, pBRDFIntegrationRootSignature);
		removeShader(pRenderer, pBRDFIntegrationShader);
		removeResource(pPanoSkybox);
		removeSampler(pRenderer, pSkyboxSampler);

		resetCmdPool(pRenderer, pCmdPools[0]);
	}

	void loadMesh(size_t index)
	{
		//Load Sponza
		GeometryLoadDesc loadDesc = {};
		loadDesc.pFileName = gModelNames[index];
		loadDesc.ppGeometry = &gModels[index];
		loadDesc.pVertexLayout = &gVertexLayoutModel;
		addResource(&loadDesc, &gResourceSyncToken);
	}

	void loadTexture(size_t index)
	{
		TextureLoadDesc textureDesc = {};
		textureDesc.pFileName = pMaterialImageFileNames[index];
		textureDesc.ppTexture = &pMaterialTextures[index];
		addResource(&textureDesc, &gResourceSyncToken);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addSceneBuffer())
			return false;

		if (!addGBuffers())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!addTransmittanceBuffer())
			return false;

		if (!addIrradienceBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppRenderTargets))
			return false;

		if (!gVirtualJoystick.Load(pSwapChain->ppRenderTargets[0]))
			return false;

		/************************************************************************/
		// Setup the resources needed for the Deferred Pass Pipeline
		/************************************************************************/
		TinyImageFormat deferredFormats[DEFERRED_RT_COUNT] = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			deferredFormats[i] = pRenderTargetDeferredPass[0][i]->mFormat;
		}

		// Create depth state and rasterizer state
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& deferredPassPipelineSettings = desc.mGraphicsDesc;
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		deferredPassPipelineSettings.pDepthState = &depthStateDesc;

		deferredPassPipelineSettings.pColorFormats = deferredFormats;

		deferredPassPipelineSettings.mSampleCount = pRenderTargetDeferredPass[0][0]->mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pRenderTargetDeferredPass[0][0]->mSampleQuality;

		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		deferredPassPipelineSettings.pRootSignature = pRootSigGbuffers;
		deferredPassPipelineSettings.pShaderProgram = pShaderGbuffers;
		deferredPassPipelineSettings.pVertexLayout = &gVertexLayoutModel;
		deferredPassPipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pPipelineGbuffers);

		//layout and pipeline for terrain draw
		{
			deferredPassPipelineSettings = {};
			deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
			deferredPassPipelineSettings.pDepthState = &depthStateDesc;

			deferredPassPipelineSettings.pColorFormats = deferredFormats;
			deferredPassPipelineSettings.mSampleCount = pRenderTargetDeferredPass[0][0]->mSampleCount;
			deferredPassPipelineSettings.mSampleQuality = pRenderTargetDeferredPass[0][0]->mSampleQuality;

			deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
			deferredPassPipelineSettings.pRootSignature = pTerrainRootSig;
			deferredPassPipelineSettings.pShaderProgram = pTerrainShader;
			deferredPassPipelineSettings.pVertexLayout = nullptr;
			deferredPassPipelineSettings.pRasterizerState = &rasterizerStateDesc;
			addPipeline(pRenderer, &desc, &pTerrainPipeline);
		}

		//layout and pipeline for sky draw
		{
			desc.mGraphicsDesc = {};
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = nullptr;

			pipelineSettings.pColorFormats = &pSceneBuffer->mFormat;
			pipelineSettings.mSampleCount = pSceneBuffer->mSampleCount;
			pipelineSettings.mSampleQuality = pSceneBuffer->mSampleQuality;

			pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
			pipelineSettings.pRootSignature = pSkyRootSig;
			pipelineSettings.pShaderProgram = pSkyShader;
			pipelineSettings.pVertexLayout = nullptr;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			addPipeline(pRenderer, &desc, &pSkyPipeline);
		}

		{
			// pipeline for transmittance compute
			PipelineDesc desc_compute = {};
			desc_compute.mType = PIPELINE_TYPE_COMPUTE;
			ComputePipelineDesc& pipelineSettings = desc_compute.mComputeDesc;
			pipelineSettings.pShaderProgram = pTransmittanceShaderCompute;
			pipelineSettings.pRootSignature = pTransmittanceRootSigCompute;
			addPipeline(pRenderer, &desc_compute, &pTransmittancePipelineCompute);
		}
		{
			// pipeline for irradience compute
			PipelineDesc desc_compute = {};
			desc_compute.mType = PIPELINE_TYPE_COMPUTE;
			ComputePipelineDesc& pipelineSettings = desc_compute.mComputeDesc;
			pipelineSettings.pShaderProgram = pIrradienceShaderCompute;
			pipelineSettings.pRootSignature = pIrradienceRootSigCompute;
			addPipeline(pRenderer, &desc_compute, &pIrradiencePipelineCompute);
		}

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayoutSkybox.mAttribs[0].mBinding = 0;
		vertexLayoutSkybox.mAttribs[0].mLocation = 0;
		vertexLayoutSkybox.mAttribs[0].mOffset = 0;

		deferredPassPipelineSettings = {};
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		deferredPassPipelineSettings.pDepthState = NULL;

		deferredPassPipelineSettings.mRenderTargetCount = 1;
		deferredPassPipelineSettings.pColorFormats = deferredFormats;
		deferredPassPipelineSettings.mSampleCount = pRenderTargetDeferredPass[0][0]->mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pRenderTargetDeferredPass[0][0]->mSampleQuality;

		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		deferredPassPipelineSettings.pRootSignature = pSkyboxRootSignature;
		deferredPassPipelineSettings.pShaderProgram = pSkyboxShader;
		deferredPassPipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		deferredPassPipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pSkyboxPipeline);

		// BRDF
		//Position
		VertexLayout vertexLayoutScreenQuad = {};
		vertexLayoutScreenQuad.mAttribCount = 2;

		vertexLayoutScreenQuad.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutScreenQuad.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutScreenQuad.mAttribs[0].mBinding = 0;
		vertexLayoutScreenQuad.mAttribs[0].mLocation = 0;
		vertexLayoutScreenQuad.mAttribs[0].mOffset = 0;

		//Uv
		vertexLayoutScreenQuad.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutScreenQuad.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayoutScreenQuad.mAttribs[1].mLocation = 1;
		vertexLayoutScreenQuad.mAttribs[1].mBinding = 0;
		vertexLayoutScreenQuad.mAttribs[1].mOffset = 3 * sizeof(float);    // first attribute contains 3 floats

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pSceneBuffer->mFormat;
		pipelineSettings.mSampleCount = pSceneBuffer->mSampleCount;
		pipelineSettings.mSampleQuality = pSceneBuffer->mSampleQuality;

		// pipelineSettings.pDepthState is NULL, pipelineSettings.mDepthStencilFormat should be NONE
		pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
		pipelineSettings.pRootSignature = pRootSigBRDF;
		pipelineSettings.pShaderProgram = pShaderBRDF;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pPipelineBRDF);

		////PPR_HolePatching -> Present
		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

		pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
		pipelineSettings.pRootSignature = pPPR_HolePatchingRootSignature;
		pipelineSettings.pShaderProgram = pPPR_HolePatchingShader;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pPPR_HolePatchingPipeline);

		PrepareDescriptorSets(false);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);
		waitForAllResourceLoads();

		waitForAllResourceLoads();

		gAppUI.Unload();

		gVirtualJoystick.Unload();

		removePipeline(pRenderer, pPipelineBRDF);
		removePipeline(pRenderer, pSkyboxPipeline);
		removePipeline(pRenderer, pTerrainPipeline);
		removePipeline(pRenderer, pSkyPipeline);
		removePipeline(pRenderer, pTransmittancePipelineCompute);
		removePipeline(pRenderer, pIrradiencePipelineCompute);
		removePipeline(pRenderer, pPPR_HolePatchingPipeline);
		removePipeline(pRenderer, pPipelineGbuffers);

		removeRenderTarget(pRenderer, pTransmittanceLUT);
		removeRenderTarget(pRenderer, pIrradienceLUT);
		removeRenderTarget(pRenderer, pIrradienceDeltaLUT);
		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pSceneBuffer);

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
			removeRenderTarget(pRenderer, pRenderTargetDeferredPass[0][i]);

		removeRenderTarget(pRenderer, pRenderTargetDeferredPass[1][1]);
		removeRenderTarget(pRenderer, pRenderTargetDeferredPass[1][2]);

		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

#if !defined(TARGET_IOS)
		if (pSwapChain->mEnableVsync != gToggleVSync)
		{
			// To remove Validation errors related to deletion of imageView before commands are executed
			// when toggling Vsync
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				Fence* pRenderCompleteFence = pRenderCompleteFences[i];

				// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
				FenceStatus fenceStatus;
				getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
				if (fenceStatus == FENCE_STATUS_INCOMPLETE)
					waitForFences(pRenderer, 1, &pRenderCompleteFence);

				resetCmdPool(pRenderer, pCmdPools[i]);
			}

			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif

		pCameraController->update(deltaTime);

		// Update camera
		mat4        viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		mat4 ViewProjMat = projMat * viewMat;

		gUniformDataCamera.mPrevProjectView = gUniformDataCamera.mProjectView;
		gUniformDataCamera.mProjectView = ViewProjMat;
		gUniformDataCamera.mCamPos = pCameraController->getViewPosition();

		viewMat.setTranslation(vec3(0));
		gUniformDataCameraSky = gUniformDataCamera;
		gUniformDataCameraSky.mProjectView = projMat * viewMat;

		//terrain
		{
			gUniformDataCameraTerrain = gUniformDataCamera;
			gUniformDataCameraTerrain.mProjectView = projMat * viewMat;

			gUniformDataTerrain.mViewportSize = { static_cast<uint>(mSettings.mWidth), static_cast<uint>(mSettings.mHeight) };
			gUniformDataTerrain.mTerrainTilesCount = gTerrainResolution;
			gUniformDataTerrain.mTerrainTilesCountInv = 1.0f / gTerrainResolution;
			gUniformDataTerrain.mTerrainWidth = 100.0f; //km
			gUniformDataTerrain.mTerrainHeight = 100.0f; //km
			gUniformDataTerrain.mTerrainAlbedoMultiplier = gTerrainAlbedoMultiplier;
		}
		//atmosphere
		{
			constexpr float EarthBottomRadius = 6360.0f; //km
			constexpr float AtmosphereRadius = 100.0f; //km
			constexpr float EarthTopRadius = EarthBottomRadius + AtmosphereRadius;
			//Karman line
			constexpr float EarthRayleighScaleHeight = 8.0f; //km
			constexpr float EarthMieScaleHeight = 1.2f; //km

			gUniformDataAtmosphere.mRadiusBottom = EarthBottomRadius; //km
			gUniformDataAtmosphere.mRadiusTop = EarthTopRadius; //km
			gUniformDataAtmosphere.mRadiusBottomSqr = EarthBottomRadius * EarthBottomRadius;
			gUniformDataAtmosphere.mRadiusTopSqr = EarthTopRadius * EarthTopRadius;

			gUniformDataAtmosphere.mScatteringRayleigh = { 0.0058f, 0.01356f, 0.0331f };
			gUniformDataAtmosphere.mDistanceBound = sqrtf(gUniformDataAtmosphere.mRadiusTopSqr - gUniformDataAtmosphere.mRadiusBottomSqr); //km
			
			gUniformDataAtmosphere.mScatteringMie = { 0.003996f, 0.003996f, 0.003996f };
			gUniformDataAtmosphere.mPhaseFunctionGMie = 0.8f;
			
			gUniformDataAtmosphere.mExtinctionMie = { 0.00444f, 0.00444f, 0.00444f };
			gUniformDataAtmosphere.mSunAngularRadius = 0.00467f; //rad

			gUniformDataAtmosphere.mExtinctionAbsorption = { 0.00065f, 0.00188f, 0.00008f };
			gUniformDataAtmosphere.mTransmittanceHeightInv = 1.0f / float(pTransmittanceLUT->mHeight);

			//rayleigh
			gUniformDataAtmosphere.mDensityRayleigh[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
			gUniformDataAtmosphere.mDensityRayleigh[1] = { 1.0f, -1.0f / EarthRayleighScaleHeight, 0.0f, 0.0f };
			gUniformDataAtmosphere.mLayerWidthRayleigh = { 0.0f, 0.0f };
			gUniformDataAtmosphere.mTransmittanceSizeInv = { 1.0f / float(pTransmittanceLUT->mWidth), 1.0f / float(pTransmittanceLUT->mHeight) };
			
			//mie
			gUniformDataAtmosphere.mDensityMie[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
			gUniformDataAtmosphere.mDensityMie[1] = { 1.0f, -1.0f / EarthMieScaleHeight, 0.0f, 0.0f };
			gUniformDataAtmosphere.mLayerWidthMie = { 0.0f, 0.0f };
			gUniformDataAtmosphere.mIrradienceSizeInv = { 1.0f / float(pIrradienceLUT->mWidth), 1.0f / float(pIrradienceLUT->mHeight) };

			//ozone
			gUniformDataAtmosphere.mDensityAbsorption[0] = { 0.0f, 0.0f, 1.0f / 15.0f, -2.0f / 3.0f };
			gUniformDataAtmosphere.mDensityAbsorption[1] = { 0.0f, 0.0f, -1.0f / 15.0f, 8.0f / 3.0f };
			gUniformDataAtmosphere.mLayerWidthAbsorption = { 25.0f, 0.0f };
			gUniformDataAtmosphere.mPad2 = { 0.0f, 0.0f };

			//sun
			float sunyaw = gSunYaw * PI;
			float sunpitch = gSunPitch * PI;
			Matrix4 marSunRot = Matrix4::rotationXY(sunpitch, -sunyaw);
			Vector3 sundir = normalize(marSunRot.getRow(2).getXYZ());
			gUniformDataAtmosphere.mSunDir = { sundir.getX(), sundir.getY(), sundir.getZ() };
			gUniformDataAtmosphere.mPad3 = 0.0f;

			Vector4 viewdir = pCameraController->getViewMatrix() * vec4(0.0f, 0.0f, 1.0f, 0.0f);
			gUniformDataAtmosphere.mViewDir = { viewdir.getX(), viewdir.getY(), viewdir.getZ() };
			gUniformDataAtmosphere.mPad4 = 0.0f;

			gUniformDataAtmosphere.mSolarIrradience = { 1.0f, 1.0f, 1.0f };
			gUniformDataAtmosphere.mPad5 = 0.0f;
		}

		//data uniforms
		gUniformDataExtendedCamera.mCameraWorldPos = vec4(pCameraController->getViewPosition(), 1.0);
		gUniformDataExtendedCamera.mViewMat = pCameraController->getViewMatrix();
		gUniformDataExtendedCamera.mProjMat = projMat;
		gUniformDataExtendedCamera.mViewProjMat = ViewProjMat;
		gUniformDataExtendedCamera.mInvViewProjMat = inverse(ViewProjMat);
		gUniformDataExtendedCamera.mViewPortSize = vec4(static_cast<float>(mSettings.mWidth), static_cast<float>(mSettings.mHeight), 1.0f / static_cast<float>(mSettings.mWidth), 1.0f / static_cast<float>(mSettings.mHeight));

		gUniformDataExtendedCamera.mProjectViewInv = inverse(gUniformDataCamera.mProjectView);
		gUniformDataExtendedCamera.mPrevProjectViewInv = inverse(gUniformDataCamera.mPrevProjectView);

		mat4 basicMat;
		basicMat[0] = vec4(1.0, 0.0, 0.0, 0.0);     //tan
		basicMat[1] = vec4(0.0, 0.0, -1.0, 0.0);    //bitan
		basicMat[2] = vec4(0.0, 1.0, 0.0, 0.0);     //normal
		basicMat[3] = vec4(0.0, 0.0, 0.0, 1.0);


		SyncToken currentProgress = getLastTokenCompleted();
		double    progress = (double)(currentProgress - gResourceSyncStartToken) / (double)(gResourceSyncToken - gResourceSyncStartToken);
		mProgressBarValue = (size_t)(mProgressBarValueMax * progress);

		/************************************************************************/
		// Update GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		// This will acquire the next swapchain image
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		BufferUpdateDesc camBuffUpdateDesc = { pBufferUniformCamera[gFrameIndex] };
		beginUpdateResource(&camBuffUpdateDesc);
		*(UniformCamData*)camBuffUpdateDesc.pMappedData = gUniformDataCamera;
		endUpdateResource(&camBuffUpdateDesc, NULL);

		BufferUpdateDesc skyboxViewProjCbv = { pBufferUniformCameraSky[gFrameIndex] };
		beginUpdateResource(&skyboxViewProjCbv);
		*(UniformCamData*)skyboxViewProjCbv.pMappedData = gUniformDataCameraSky;
		endUpdateResource(&skyboxViewProjCbv, NULL);

		{
			BufferUpdateDesc terrainViewProjCbv = { pBufferUniformCameraTerrain[gFrameIndex] };
			beginUpdateResource(&terrainViewProjCbv);
			*(UniformCamData*)terrainViewProjCbv.pMappedData = gUniformDataCameraTerrain;
			endUpdateResource(&terrainViewProjCbv, nullptr);

			BufferUpdateDesc terrainCbv = { pBufferUniformTerrain[gFrameIndex] };
			beginUpdateResource(&terrainCbv);
			*(UniformDataTerrain*)terrainCbv.pMappedData = gUniformDataTerrain;
			endUpdateResource(&terrainCbv, nullptr);
		}

		{
			BufferUpdateDesc atmosphereCbv = { pBufferUniformAtmosphere[gFrameIndex] };
			beginUpdateResource(&atmosphereCbv);
			*(UniformDataAtmosphere*)atmosphereCbv.pMappedData = gUniformDataAtmosphere;
			endUpdateResource(&atmosphereCbv, nullptr);
		}

		BufferUpdateDesc CbvExtendedCamera = { pBufferUniformExtendedCamera[gFrameIndex] };
		beginUpdateResource(&CbvExtendedCamera);
		*(UniformExtendedCamData*)CbvExtendedCamera.pMappedData = gUniformDataExtendedCamera;
		endUpdateResource(&CbvExtendedCamera, NULL);

		// Draw G-buffers
		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);
		{
			TextureBarrier uavBarriers[3] = {};
			uavBarriers[0] = { pTransmittanceLUT->pTexture, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 0, nullptr, 1, uavBarriers, 0, nullptr);
			// eval transmittance lut
			{
				cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "TransmittanceLUT");

				cmdBindPipeline(cmd, pTransmittancePipelineCompute);
				cmdBindDescriptorSet(cmd, 0, pTransmittanceDescriptorSetCompute[0]);
				cmdBindDescriptorSet(cmd, gFrameIndex, pTransmittanceDescriptorSetCompute[1]);
				cmdDispatch(cmd, pTransmittanceLUT->mWidth >> 5u, pTransmittanceLUT->mHeight >> 5u, 1);

				cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			}
			uavBarriers[0] = { pTransmittanceLUT->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
			uavBarriers[1] = { pIrradienceLUT->pTexture, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			uavBarriers[2] = { pIrradienceDeltaLUT->pTexture, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 0, nullptr, 3, uavBarriers, 0, nullptr);
			// eval irradience lut
			{
				cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "IrradienceLUT");

				cmdBindPipeline(cmd, pIrradiencePipelineCompute);
				cmdBindDescriptorSet(cmd, 0, pIrradienceDescriptorSetCompute[0]);
				cmdBindDescriptorSet(cmd, gFrameIndex, pIrradienceDescriptorSetCompute[1]);
				cmdDispatch(cmd, 1, 1, 1);

				cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			}
			uavBarriers[0] = { pIrradienceLUT->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
			uavBarriers[1] = { pIrradienceDeltaLUT->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
			cmdResourceBarrier(cmd, 0, nullptr, 2, uavBarriers, 0, nullptr);

		}
		//Clear G-buffers and Depth buffer
		LoadActionsDesc loadActions = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRenderTargetDeferredPass[0][i]->mClearValue;
		}

		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { { 1.0f, 0.0f } };    // Clear depth to the far plane and stencil to 0

		// Transfer G-buffers to render target state for each buffer
		RenderTargetBarrier rtBarriers[DEFERRED_RT_COUNT + 2] = {};
		rtBarriers[0] = { pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE };
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			rtBarriers[1 + i] = { pRenderTargetDeferredPass[gFrameFlipFlop][i], RESOURCE_STATE_SHADER_RESOURCE,
								  RESOURCE_STATE_RENDER_TARGET };
		}

		// Transfer DepthBuffer to a DephtWrite State
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, DEFERRED_RT_COUNT + 1, rtBarriers);

		cmdBindRenderTargets(cmd, 1, pRenderTargetDeferredPass[gFrameFlipFlop], pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0][0]->mWidth, (float)pRenderTargetDeferredPass[0][0]->mHeight, 1.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0][0]->mWidth, pRenderTargetDeferredPass[0][0]->mHeight);

		// Draw the skybox.
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Render SkyBox");

		const uint32_t skyboxStride = sizeof(float) * 4;
		cmdBindPipeline(cmd, pSkyboxPipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkybox[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSkybox[1]);
		cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, &skyboxStride, NULL);
		cmdDraw(cmd, 36, 0);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0][0]->mWidth, (float)pRenderTargetDeferredPass[0][0]->mHeight, 0.0f, 1.0f);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(
			cmd, DEFERRED_RT_COUNT, pRenderTargetDeferredPass[gFrameFlipFlop], pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0][0]->mWidth, (float)pRenderTargetDeferredPass[0][0]->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0][0]->mWidth, pRenderTargetDeferredPass[0][0]->mHeight);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		// Draw Sponza
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Fill GBuffers");
		//The default code path we have if not iOS uses an array of texture of size 81
		//iOS only supports 31 max texture units in a fragment shader for most devices.
		//so fallback to binding every texture for every draw call (max of 5 textures)

		static bool prevDataLoaded = false;

		//SyncToken lastCompletedToken = getLastTokenCompleted();
		bool dataLoaded = isTokenCompleted(&gResourceSyncToken);
		if (dataLoaded)
		{
			if (prevDataLoaded != dataLoaded)
			{
				prevDataLoaded = dataLoaded;
				waitQueueIdle(pGraphicsQueue);
				PrepareDescriptorSets(true);
			}

			// Draw the terrain
			{
				cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Render terrain");

				cmdBindPipeline(cmd, pTerrainPipeline);
				cmdBindDescriptorSet(cmd, 0, pTerrainDescriptorSet[0]);
				cmdBindDescriptorSet(cmd, gFrameIndex, pTerrainDescriptorSet[1]);
				cmdDrawInstanced(cmd, 6, 0, gTerrainResolution * gTerrainResolution, 0);

				cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			}

			/*
			Geometry& sponzaMesh = *gModels[SPONZA_MODEL];

			cmdBindIndexBuffer(cmd, sponzaMesh.pIndexBuffer, sponzaMesh.mIndexType, 0);
			Buffer* pSponzaVertexBuffers[] = { sponzaMesh.pVertexBuffers[0] };
			cmdBindVertexBuffer(cmd, 1, pSponzaVertexBuffers, sponzaMesh.mVertexStrides, NULL);

			struct MaterialMaps
			{
				uint textureMaps;
			} data;

			const uint32_t drawCount = (uint32_t)sponzaMesh.mDrawArgCount;

			if (gUseTexturesFallback)
			{
				cmdBindPipeline(cmd, pPipelineGbuffers);
				cmdBindVertexBuffer(cmd, 1, pSponzaVertexBuffers, sponzaMesh.mVertexStrides, NULL);
				cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetGbuffers[1]);
				cmdBindDescriptorSet(cmd, 0, pDescriptorSetGbuffers[2]);
				DescriptorData params[8] = {};

				for (uint32_t i = 0; i < drawCount; ++i)
				{
					int materialID = gMaterialIds[i];
					materialID *= 5;    //because it uses 5 basic textures for redering BRDF

					for (int j = 0; j < 5; ++j)
					{
						params[j].pName = pTextureName[j];
						params[j].ppTextures = &pMaterialTextures[gSponzaTextureIndexforMaterial[materialID + j]];
					}
					updateDescriptorSet(pRenderer, i, pDescriptorSetGbuffers[0], 5, params);
					cmdBindDescriptorSet(cmd, i, pDescriptorSetGbuffers[0]);

					IndirectDrawIndexArguments& cmdData = sponzaMesh.pDrawArgs[i];
					cmdDrawIndexed(cmd, cmdData.mIndexCount, cmdData.mStartIndex, cmdData.mVertexOffset);
				}
			}
			else
			{
				cmdBindPipeline(cmd, pPipelineGbuffers);
				cmdBindVertexBuffer(cmd, 1, pSponzaVertexBuffers, sponzaMesh.mVertexStrides, NULL);
				cmdBindDescriptorSet(cmd, 0, pDescriptorSetGbuffers[0]);
				cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetGbuffers[1]);
				cmdBindDescriptorSet(cmd, 0, pDescriptorSetGbuffers[2]);

				for (uint32_t i = 0; i < drawCount; ++i)
				{
					int materialID = gMaterialIds[i];
					materialID *= 5;    //because it uses 5 basic textures for redering BRDF

					data.textureMaps = ((gSponzaTextureIndexforMaterial[materialID + 0] & 0xFF) << 0) |
									   ((gSponzaTextureIndexforMaterial[materialID + 1] & 0xFF) << 8) |
									   ((gSponzaTextureIndexforMaterial[materialID + 2] & 0xFF) << 16) |
									   ((gSponzaTextureIndexforMaterial[materialID + 3] & 0xFF) << 24);

					cmdBindPushConstants(cmd, pRootSigGbuffers, "cbTextureRootConstants", &data);
					IndirectDrawIndexArguments& cmdData = sponzaMesh.pDrawArgs[i];
					cmdDrawIndexed(cmd, cmdData.mIndexCount, cmdData.mStartIndex, cmdData.mVertexOffset);
				}
			}

			Geometry& lionMesh = *gModels[LION_MODEL];

			//Draw Lion
			if (gUseTexturesFallback)
			{
				DescriptorData params[5] = {};

				params[0].pName = pTextureName[0];
				params[0].ppTextures = &pMaterialTextures[81];

				params[1].pName = pTextureName[1];
				params[1].ppTextures = &pMaterialTextures[83];

				params[2].pName = pTextureName[2];
				params[2].ppTextures = &pMaterialTextures[6];

				params[3].pName = pTextureName[3];
				params[3].ppTextures = &pMaterialTextures[6];

				params[4].pName = pTextureName[4];
				params[4].ppTextures = &pMaterialTextures[0];

				updateDescriptorSet(pRenderer, drawCount + 1, pDescriptorSetGbuffers[0], 5, params);
				cmdBindDescriptorSet(cmd, drawCount + 1, pDescriptorSetGbuffers[0]);
			}
			else
			{
				data.textureMaps = ((81 & 0xFF) << 0) | ((83 & 0xFF) << 8) | ((6 & 0xFF) << 16) | ((6 & 0xFF) << 24);

				cmdBindPushConstants(cmd, pRootSigGbuffers, "cbTextureRootConstants", &data);
			}

			cmdBindDescriptorSet(cmd, 1, pDescriptorSetGbuffers[2]);

			Buffer* pLionVertexBuffers[] = { lionMesh.pVertexBuffers[0] };
			cmdBindVertexBuffer(cmd, 1, pLionVertexBuffers, lionMesh.mVertexStrides, NULL);
			cmdBindIndexBuffer(cmd, lionMesh.pIndexBuffer, lionMesh.mIndexType, 0);

			for (uint32_t i = 0; i < (uint32_t)lionMesh.mDrawArgCount; ++i)
			{
				IndirectDrawIndexArguments& cmdData = lionMesh.pDrawArgs[i];
				cmdDrawIndexed(cmd, cmdData.mIndexCount, cmdData.mStartIndex, cmdData.mVertexOffset);
			}
			*/
		}

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		// Transfer DepthBuffer to a Shader resource state
		rtBarriers[0] = { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE };
		// Transfer current render target to a render target state
		rtBarriers[1] = { pSceneBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
		// Transfer G-buffers to a Shader resource state
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			rtBarriers[2 + i] = { pRenderTargetDeferredPass[gFrameFlipFlop][i], RESOURCE_STATE_RENDER_TARGET,
								  RESOURCE_STATE_SHADER_RESOURCE };
		}

		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, DEFERRED_RT_COUNT + 2, rtBarriers);

		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pSceneBuffer->mClearValue;

		cmdBindRenderTargets(cmd, 1, &pSceneBuffer, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneBuffer->mWidth, (float)pSceneBuffer->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSceneBuffer->mWidth, pSceneBuffer->mHeight);

		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Render BRDF");

		const uint32_t quadStride = sizeof(float) * 5;
		cmdBindPipeline(cmd, pPipelineBRDF);
		cmdBindDescriptorSet(cmd, gFrameFlipFlop, pDescriptorSetBRDF[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetBRDF[1]);
		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, &quadStride, NULL);
		cmdDraw(cmd, 3, 0);
		//#endif
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		// Draw the sky
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Render sky");

			cmdBindPipeline(cmd, pSkyPipeline);
			cmdBindDescriptorSet(cmd, 0, pSkyDescriptorSet[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pSkyDescriptorSet[1]);
			cmdDraw(cmd, 3, 0);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Apply Reflections");

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

		rtBarriers[0] = { pSceneBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
		rtBarriers[1] = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);

		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		cmdBindPipeline(cmd, pPPR_HolePatchingPipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetPPR__HolePatching[0]);
		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, &quadStride, NULL);
		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		// End Reflections

		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

		float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
		cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 30.f), gGpuProfileToken);

		if (!dataLoaded)
			gAppUI.Gui(pLoadingGui);
#ifndef TARGET_IOS
		else
			gAppUI.Gui(pGui);
#endif

		cmdDrawProfilerUI();

		gAppUI.Draw(cmd);
		cmdEndDebugMarker(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		// Transition our texture to present state
		rtBarriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, rtBarriers);
		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
		endCmd(cmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = &cmd;
		submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
		submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
		submitDesc.pSignalFence = pRenderCompleteFence;
		queueSubmit(pGraphicsQueue, &submitDesc);
		QueuePresentDesc presentDesc = {};
		presentDesc.mIndex = swapchainImageIndex;
		presentDesc.mWaitSemaphoreCount = 1;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.mSubmitDone = true;
		queuePresent(pGraphicsQueue, &presentDesc);
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
		gFrameFlipFlop ^= 1;
	}

	const char* GetName() { return "104_Sky_Hillaire_2020"; }

	void PrepareDescriptorSets(bool dataLoaded)
	{
		// GBuffer
		{
			DescriptorData params[8] = {};

			if (!gUseTexturesFallback && dataLoaded)
			{
				params[0].pName = "textureMaps";
				params[0].ppTextures = pMaterialTextures;
				params[0].mCount = TOTAL_IMGS;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetGbuffers[0], 1, params);
			}

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0] = {};
				params[0].pName = "cbCamera";
				params[0].ppBuffers = &pBufferUniformCamera[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetGbuffers[1], 1, params);
			}
			params[0].pName = "cbObject";
			params[0].ppBuffers = &pSponzaBuffer;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetGbuffers[2], 1, params);
			params[0].ppBuffers = &pLionBuffer;
			updateDescriptorSet(pRenderer, 1, pDescriptorSetGbuffers[2], 1, params);
		}
		// Shading
		{
			DescriptorData BRDFParams[12] = {};
			BRDFParams[0].pName = "cbLights";
			BRDFParams[0].ppBuffers = &pBufferUniformLights;
			BRDFParams[1].pName = "cbDLights";
			BRDFParams[1].ppBuffers = &pBufferUniformDirectionalLights;
			BRDFParams[2].pName = "brdfIntegrationMap";
			BRDFParams[2].ppTextures = &pBRDFIntegrationMap;
			BRDFParams[3].pName = "irradianceMap";
			BRDFParams[3].ppTextures = &pIrradianceMap;
			BRDFParams[4].pName = "specularMap";
			BRDFParams[4].ppTextures = &pSpecularMap;
			BRDFParams[5].pName = "DepthTexture";
			BRDFParams[5].ppTextures = &pDepthBuffer->pTexture;
			for (uint32_t i = 0; i < 2; ++i)
			{
				BRDFParams[6].pName = "AlbedoTexture";
				BRDFParams[6].ppTextures = &pRenderTargetDeferredPass[i][0]->pTexture;
				BRDFParams[7].pName = "NormalTexture";
				BRDFParams[7].ppTextures = &pRenderTargetDeferredPass[i][1]->pTexture;
				BRDFParams[8].pName = "RoughnessTexture";
				BRDFParams[8].ppTextures = &pRenderTargetDeferredPass[i][2]->pTexture;
				updateDescriptorSet(pRenderer, i, pDescriptorSetBRDF[0], 9, BRDFParams);
			}
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				BRDFParams[0].pName = "cbExtendCamera";
				BRDFParams[0].ppBuffers = &pBufferUniformExtendedCamera[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetBRDF[1], 1, BRDFParams);
			}
		}
		// PPR Hole Patching
		{
			DescriptorData PPR_HolePatchingParams[1] = {};
			PPR_HolePatchingParams[0].pName = "SceneTexture";
			PPR_HolePatchingParams[0].ppTextures = &pSceneBuffer->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetPPR__HolePatching[0], 1, PPR_HolePatchingParams);
		}

		if (dataLoaded)
		{
			// terrain descriptor sets
			DescriptorData terrainParams[2] = {};
			terrainParams[0].pName = "terrainHeightTex";
			terrainParams[0].ppTextures = &pTerrainTexture;
			updateDescriptorSet(pRenderer, 0, pTerrainDescriptorSet[0], 1, terrainParams);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				terrainParams[0].pName = "cbCamera";
				terrainParams[0].ppBuffers = &pBufferUniformCameraTerrain[i];
				terrainParams[1].pName = "cbTerrain";
				terrainParams[1].ppBuffers = &pBufferUniformTerrain[i];
				updateDescriptorSet(pRenderer, i, pTerrainDescriptorSet[1], 2, terrainParams);
			}
		}
		else
		{
			//transmittance
			{
				DescriptorData pTransLUTParams[1] = {};
				pTransLUTParams[0].pName = "TransmittanceLUT";
				pTransLUTParams[0].ppTextures = &pTransmittanceLUT->pTexture;
				updateDescriptorSet(pRenderer, 0, pTransmittanceDescriptorSetCompute[0], 1, pTransLUTParams);
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					pTransLUTParams[0].pName = "cbAtmosphere";
					pTransLUTParams[0].ppBuffers = &pBufferUniformAtmosphere[i];
					updateDescriptorSet(pRenderer, i, pTransmittanceDescriptorSetCompute[1], 1, pTransLUTParams);
				}
			}
			//irradience
			{
				DescriptorData pIrradLUTParams[3] = {};
				pIrradLUTParams[0].pName = "TransmittanceLUT";
				pIrradLUTParams[0].ppTextures = &pTransmittanceLUT->pTexture;
				pIrradLUTParams[1].pName = "IrradienceLUT";
				pIrradLUTParams[1].ppTextures = &pIrradienceLUT->pTexture;
				pIrradLUTParams[2].pName = "IrradienceDeltaLUT";
				pIrradLUTParams[2].ppTextures = &pIrradienceDeltaLUT->pTexture;
				updateDescriptorSet(pRenderer, 0, pIrradienceDescriptorSetCompute[0], 3, pIrradLUTParams);
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					pIrradLUTParams[0].pName = "cbAtmosphere";
					pIrradLUTParams[0].ppBuffers = &pBufferUniformAtmosphere[i];
					updateDescriptorSet(pRenderer, i, pIrradienceDescriptorSetCompute[1], 1, pIrradLUTParams);
				}
			}
			// sky
			{
				DescriptorData pSkyParams[2] = {};
				pSkyParams[0].pName = "TransmittanceLUT";
				pSkyParams[0].ppTextures = &pTransmittanceLUT->pTexture;
				updateDescriptorSet(pRenderer, 0, pSkyDescriptorSet[0], 1, pSkyParams);
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					pSkyParams[0].pName = "cbAtmosphere";
					pSkyParams[0].ppBuffers = &pBufferUniformAtmosphere[i];
					
					pSkyParams[1].pName = "cbCameraExtended";
					pSkyParams[1].ppBuffers = &pBufferUniformExtendedCamera[i];

					updateDescriptorSet(pRenderer, i, pSkyDescriptorSet[1], 2, pSkyParams);
				}
			}
		}
	}

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = mSettings.mDefaultVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addSceneBuffer()
	{
		RenderTargetDesc sceneRT = {};
		sceneRT.mArraySize = 1;
		sceneRT.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		sceneRT.mDepth = 1;
		sceneRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		sceneRT.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		sceneRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;

		sceneRT.mHeight = mSettings.mHeight;
		sceneRT.mWidth = mSettings.mWidth;

		sceneRT.mSampleCount = SAMPLE_COUNT_1;
		sceneRT.mSampleQuality = 0;
		sceneRT.pName = "Scene Buffer";

		addRenderTarget(pRenderer, &sceneRT, &pSceneBuffer);

		return pSceneBuffer != NULL;
	}

	bool addGBuffers()
	{
		ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };

		/************************************************************************/
		// Deferred pass render targets
		/************************************************************************/
		RenderTargetDesc deferredRTDesc = {};
		deferredRTDesc.mArraySize = 1;
		deferredRTDesc.mClearValue = optimizedColorClearBlack;
		deferredRTDesc.mDepth = 1;
		deferredRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		deferredRTDesc.mWidth = mSettings.mWidth;
		deferredRTDesc.mHeight = mSettings.mHeight;
		deferredRTDesc.mSampleCount = SAMPLE_COUNT_1;
		deferredRTDesc.mSampleQuality = 0;
		deferredRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		deferredRTDesc.pName = "G-Buffer RTs";

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			if (i == 1 || i == 2)
				deferredRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
			else if (i == 3)
				deferredRTDesc.mFormat = TinyImageFormat_R16G16_SFLOAT;
			else
				deferredRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;

			if (i == 2)
				deferredRTDesc.mClearValue = { { 1.0f, 0.0f, 0.0f, 0.0f } };
			else if (i == 3)
				deferredRTDesc.mClearValue = optimizedColorClearBlack;

			addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[0][i]);
			pRenderTargetDeferredPass[1][i] = pRenderTargetDeferredPass[0][i];
		}

		deferredRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[1][1]);

		deferredRTDesc.mClearValue = { { 1.0f, 0.0f, 0.0f, 0.0f } };
		addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[1][2]);

#ifdef VULKAN
		beginCmd(pCmds[0]);

		RenderTargetBarrier rtBarriers[2] = {};
		rtBarriers[0] = { pRenderTargetDeferredPass[1][1], RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_SHADER_RESOURCE };
		rtBarriers[1] = { pRenderTargetDeferredPass[1][2], RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(pCmds[0], 0, NULL, 0, NULL, 2, rtBarriers);

		endCmd(pCmds[0]);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = pCmds;
		submitDesc.pSignalFence = pRenderCompleteFences[0];
		submitDesc.mSubmitDone = true;
		queueSubmit(pGraphicsQueue, &submitDesc);
		waitForFences(pRenderer, 1, &pRenderCompleteFences[0]);
#endif
		return pRenderTargetDeferredPass[0] != NULL;
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = { { 1.0f, 0 } };
		depthRT.mDepth = 1;
		depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.pName = "Depth Buffer";
		//fixes flickering issues related to depth buffer being recycled.
#ifdef METAL
		depthRT.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	bool addTransmittanceBuffer()
	{
		RenderTargetDesc transDesc = {};
		transDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		transDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		transDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		transDesc.mWidth = 256;
		transDesc.mHeight = 64;
		transDesc.mDepth = 1;
		transDesc.mArraySize = 1;
		transDesc.mSampleCount = SAMPLE_COUNT_1;
		transDesc.mSampleQuality = 0;
		transDesc.pName = "TransmittanceLUT";
		addRenderTarget(pRenderer, &transDesc, &pTransmittanceLUT);

		return nullptr != pTransmittanceLUT;
	}

	bool addIrradienceBuffer()
	{
		RenderTargetDesc irradDesc = {};
		irradDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		irradDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		irradDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		irradDesc.mWidth = 64;
		irradDesc.mHeight = 16;
		irradDesc.mDepth = 1;
		irradDesc.mArraySize = 1;
		irradDesc.mSampleCount = SAMPLE_COUNT_1;
		irradDesc.mSampleQuality = 0;
		irradDesc.pName = "IrradienceLUT";
		addRenderTarget(pRenderer, &irradDesc, &pIrradienceLUT);

		irradDesc.pName = "IrradienceDeltaLUT";
		addRenderTarget(pRenderer, &irradDesc, &pIrradienceDeltaLUT);

		return nullptr != pTransmittanceLUT && nullptr != pIrradienceDeltaLUT;;
	}
};

void assignSponzaTextures()
{
	int AO = 5;
	int NoMetallic = 6;

	//00 : leaf
	gSponzaTextureIndexforMaterial.push_back(66);
	gSponzaTextureIndexforMaterial.push_back(67);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(68);
	gSponzaTextureIndexforMaterial.push_back(AO);

	//01 : vase_round
	gSponzaTextureIndexforMaterial.push_back(78);
	gSponzaTextureIndexforMaterial.push_back(79);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(80);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 02 : 16___Default (gi_flag)
	gSponzaTextureIndexforMaterial.push_back(8);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!!
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!
	gSponzaTextureIndexforMaterial.push_back(AO);

	//03 : Material__57 (Plant)
	gSponzaTextureIndexforMaterial.push_back(75);
	gSponzaTextureIndexforMaterial.push_back(76);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(77);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 04 : Material__298
	gSponzaTextureIndexforMaterial.push_back(9);
	gSponzaTextureIndexforMaterial.push_back(10);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(11);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 05 : bricks
	gSponzaTextureIndexforMaterial.push_back(22);
	gSponzaTextureIndexforMaterial.push_back(23);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(24);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 06 :  arch
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 07 : ceiling
	gSponzaTextureIndexforMaterial.push_back(25);
	gSponzaTextureIndexforMaterial.push_back(26);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(27);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 08 : column_a
	gSponzaTextureIndexforMaterial.push_back(28);
	gSponzaTextureIndexforMaterial.push_back(29);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(30);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 09 : Floor
	gSponzaTextureIndexforMaterial.push_back(60);
	gSponzaTextureIndexforMaterial.push_back(61);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(6);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 10 : column_c
	gSponzaTextureIndexforMaterial.push_back(34);
	gSponzaTextureIndexforMaterial.push_back(35);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(36);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 11 : details
	gSponzaTextureIndexforMaterial.push_back(45);
	gSponzaTextureIndexforMaterial.push_back(47);
	gSponzaTextureIndexforMaterial.push_back(46);
	gSponzaTextureIndexforMaterial.push_back(48);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 12 : column_b
	gSponzaTextureIndexforMaterial.push_back(31);
	gSponzaTextureIndexforMaterial.push_back(32);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(33);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 13 : flagpole
	gSponzaTextureIndexforMaterial.push_back(57);
	gSponzaTextureIndexforMaterial.push_back(58);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(59);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 14 : fabric_e (green)
	gSponzaTextureIndexforMaterial.push_back(51);
	gSponzaTextureIndexforMaterial.push_back(52);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 15 : fabric_d (blue)
	gSponzaTextureIndexforMaterial.push_back(49);
	gSponzaTextureIndexforMaterial.push_back(50);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 16 : fabric_a (red)
	gSponzaTextureIndexforMaterial.push_back(55);
	gSponzaTextureIndexforMaterial.push_back(56);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 17 : fabric_g (curtain_blue)
	gSponzaTextureIndexforMaterial.push_back(37);
	gSponzaTextureIndexforMaterial.push_back(38);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 18 : fabric_c (curtain_red)
	gSponzaTextureIndexforMaterial.push_back(41);
	gSponzaTextureIndexforMaterial.push_back(42);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 19 : fabric_f (curtain_green)
	gSponzaTextureIndexforMaterial.push_back(39);
	gSponzaTextureIndexforMaterial.push_back(40);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 20 : chain
	gSponzaTextureIndexforMaterial.push_back(12);
	gSponzaTextureIndexforMaterial.push_back(14);
	gSponzaTextureIndexforMaterial.push_back(13);
	gSponzaTextureIndexforMaterial.push_back(15);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 21 : vase_hanging
	gSponzaTextureIndexforMaterial.push_back(72);
	gSponzaTextureIndexforMaterial.push_back(73);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(74);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 22 : vase
	gSponzaTextureIndexforMaterial.push_back(69);
	gSponzaTextureIndexforMaterial.push_back(70);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(71);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 23 : Material__25 (lion)
	gSponzaTextureIndexforMaterial.push_back(16);
	gSponzaTextureIndexforMaterial.push_back(17);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(18);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 24 : roof
	gSponzaTextureIndexforMaterial.push_back(63);
	gSponzaTextureIndexforMaterial.push_back(64);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(65);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 25 : Material__47 - it seems missing
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);
}

DEFINE_APPLICATION_MAIN(Sky)
