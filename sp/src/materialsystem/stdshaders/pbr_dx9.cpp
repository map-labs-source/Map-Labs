//==================================================================================================
//
// Physically Based Rendering shader for brushes and models
//
//==================================================================================================

// Includes for all shaders
#include "BaseVSShader.h"
#include "cpp_shader_constant_register_map.h"

// Includes for PS30
#include "pbr_vs30.inc"
#include "pbr_ps30.inc"

// Includes for PS20b
#include "pbr_vs20b.inc"
#include "pbr_ps20b.inc"

// Defining samplers
const Sampler_t SAMPLER_BASETEXTURE = SHADER_SAMPLER0;
const Sampler_t SAMPLER_BASETEXTURE2 = SHADER_SAMPLER3;
const Sampler_t SAMPLER_NORMAL = SHADER_SAMPLER1;
const Sampler_t SAMPLER_NORMAL2 = SHADER_SAMPLER12;
const Sampler_t SAMPLER_ENVMAP = SHADER_SAMPLER2;
const Sampler_t SAMPLER_SHADOWDEPTH = SHADER_SAMPLER4;
const Sampler_t SAMPLER_RANDOMROTATION = SHADER_SAMPLER5;
const Sampler_t SAMPLER_FLASHLIGHT = SHADER_SAMPLER6;
const Sampler_t SAMPLER_LIGHTMAP = SHADER_SAMPLER7;
const Sampler_t SAMPLER_MRAO = SHADER_SAMPLER10;
const Sampler_t SAMPLER_MRAO2 = SHADER_SAMPLER13;
const Sampler_t SAMPLER_EMISSIVE = SHADER_SAMPLER11;
const Sampler_t SAMPLER_EMISSIVE2 = SHADER_SAMPLER14;
const Sampler_t SAMPLER_DETAIL = SHADER_SAMPLER15;

// Convars
static ConVar mat_fullbright("mat_fullbright", "0", FCVAR_CHEAT);
static ConVar mat_specular("mat_specular", "1", FCVAR_CHEAT);
static ConVar mat_pbr_force_20b("mat_pbr_force_20b", "0", FCVAR_CHEAT);
static ConVar mat_pbr_parallaxmap("mat_pbr_parallaxmap", "1");

// Variables for this shader
struct PBR_Vars_t
{
    PBR_Vars_t()
    {
        memset(this, 0xFF, sizeof(*this));
    }

    int BaseTexture;
    int BaseTexture2;
    int BaseColor;
    int NormalTexture;
    int BumpMap;
    int BumpMap2;
    int EnvMap;

    int BaseTextureTransform;
    int UseParallax;
    int ParallaxDepth;
    int ParallaxCenter;
    int AlphaTestReference;
    int FlashlightTexture;
    int FlashlightTextureFrame;
    int EmissionTexture;
    int EmissionTexture2;
	int LightmapTexture;
    int MRAOTexture;
    int MRAOTexture2;
    int EmissionScale;
    int EmissionScale2;
	int HSV;
	int HSV_blend;

	// PCC Implementation
	int EnvmapParallax;
	int EnvMapParallaxOBB1;
	int EnvMapParallaxOBB2;
	int EnvMapParallaxOBB3;
	int EnvmapOrigin;

	// Tint, Contrast and Saturation Parameters
	int EnvMapContrast;
	int EnvMapSaturation;
	int	EnvMapTint;

	// Frames
	int BaseTextureFrame;
	int BaseTextureFrame2;
	int DetailFrame;
	int BumpFrame;

	// Detailtexture implementation
	//int DetailTexture;
	//int DetailBlendMode;
	//int DetailBlendFactor;
	//int DetailTransform;
	//int DetailScale;
	//int DetailTint;

};

// Beginning the shader
BEGIN_VS_SHADER(PBR, "PBR shader")

    // Setting up vmt parameters
    BEGIN_SHADER_PARAMS;
        SHADER_PARAM(BASETEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "shadertest/lightmappedtexture", "Blended texture");
        SHADER_PARAM(FRAME2, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $basetexture2");
        SHADER_PARAM(ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0", "");
        SHADER_PARAM(ENVMAP, SHADER_PARAM_TYPE_ENVMAP, "", "Set the cubemap for this material.");
        SHADER_PARAM(MRAOTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Texture with metalness in R, roughness in G, ambient occlusion in B, HSV blend in A.");
        SHADER_PARAM(MRAOTEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "", "Texture with metalness in R, roughness in G, ambient occlusion in B, HSV blend in A.");
        SHADER_PARAM(EMISSIONTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Emission texture");
        SHADER_PARAM(EMISSIONTEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "", "Emission texture");
        SHADER_PARAM(NORMALTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Normal texture (deprecated, use $bumpmap)");
        SHADER_PARAM(BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "Normal texture");
		SHADER_PARAM(BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "", "frame number for $bumpmap/$normaltexture");
        SHADER_PARAM(BUMPMAP2, SHADER_PARAM_TYPE_TEXTURE, "", "Normal texture");
		SHADER_PARAM(LIGHTMAP, SHADER_PARAM_TYPE_TEXTURE, "", "In MP this gets set automatically if your model has a lightmap, in SP you have to do it yourself.");
        SHADER_PARAM(PARALLAX, SHADER_PARAM_TYPE_BOOL, "0", "Use Parallax Occlusion Mapping.");
        SHADER_PARAM(PARALLAXDEPTH, SHADER_PARAM_TYPE_FLOAT, "0.0030", "Depth of the Parallax Map");
        SHADER_PARAM(PARALLAXCENTER, SHADER_PARAM_TYPE_FLOAT, "0.5", "Center depth of the Parallax Map");
        SHADER_PARAM(EMISSIONSCALE, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Color to multiply emission texture with");
        SHADER_PARAM(EMISSIONSCALE2, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Color to multiply emission texture with");
        SHADER_PARAM(HSV, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "HSV color to transform $basetexture texture with");
        SHADER_PARAM(HSV_BLEND, SHADER_PARAM_TYPE_BOOL, "0", "Blend untransformed color and HSV transformed color");

		// Detail textures
		//SHADER_PARAM(DETAIL, SHADER_PARAM_TYPE_TEXTURE, "", "Texture");
		//SHADER_PARAM(DETAILTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$detail & $detail2 texcoord transform");
		//SHADER_PARAM(DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $detail");
		//SHADER_PARAM(DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "4", "scale of the detail texture");
		//SHADER_PARAM(DETAILTINT, SHADER_PARAM_TYPE_COLOR, "", "Tints $Detail");
		//SHADER_PARAM(DETAILBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "Check VDC for more information");
		//SHADER_PARAM(DETAILBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "", "How much detail is it that you want?");

		// Envmapping parameters
		SHADER_PARAM(ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "", "Tints the $envmap");
		SHADER_PARAM(ENVMAPCONTRAST, SHADER_PARAM_TYPE_FLOAT, "", "0-1 where 1 is full contrast and 0 is pure cubemap");
		SHADER_PARAM(ENVMAPSATURATION, SHADER_PARAM_TYPE_FLOAT, "", "0-1 where 1 is default pure cubemap and 0 is b/w.");

		// Parallax Corrected Cubemaps
		SHADER_PARAM(ENVMAPPARALLAX, SHADER_PARAM_TYPE_INTEGER, "0", "Enables parallax correction code for env_cubemaps");
		SHADER_PARAM(ENVMAPPARALLAXOBB1, SHADER_PARAM_TYPE_VEC4, "[1 0 0 0]", "The first line of the parallax correction OBB matrix");
		SHADER_PARAM(ENVMAPPARALLAXOBB2, SHADER_PARAM_TYPE_VEC4, "[0 1 0 0]", "The second line of the parallax correction OBB matrix");
		SHADER_PARAM(ENVMAPPARALLAXOBB3, SHADER_PARAM_TYPE_VEC4, "[0 0 1 0]", "The third line of the parallax correction OBB matrix");
		SHADER_PARAM(ENVMAPORIGIN, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "The world space position of the env_cubemap being corrected");

    END_SHADER_PARAMS;

    // Setting up variables for this shader
    void SetupVars(IMaterialVar **params, PBR_Vars_t &info)
    {
        info.BaseTexture = BASETEXTURE;
        info.BaseTexture2 = BASETEXTURE2;
        info.BaseColor = IS_FLAG_SET(MATERIAL_VAR_MODEL) ? COLOR2 : COLOR;
        info.NormalTexture = NORMALTEXTURE;
        info.BumpMap = BUMPMAP;
        info.BumpMap2 = BUMPMAP2;
		info.LightmapTexture = LIGHTMAP; // Model lightmapping
        info.BaseTextureFrame = FRAME;
        info.BaseTextureFrame2 = FRAME2;
        info.BaseTextureTransform = BASETEXTURETRANSFORM;
        info.AlphaTestReference = ALPHATESTREFERENCE;
        info.FlashlightTexture = FLASHLIGHTTEXTURE;
        info.FlashlightTextureFrame = FLASHLIGHTTEXTUREFRAME;
        info.EnvMap = ENVMAP;
        info.EmissionTexture = EMISSIONTEXTURE;
        info.EmissionTexture2 = EMISSIONTEXTURE2;
        info.MRAOTexture = MRAOTEXTURE;
        info.MRAOTexture2 = MRAOTEXTURE2;
        info.UseParallax = PARALLAX;
        info.ParallaxDepth = PARALLAXDEPTH;
        info.ParallaxCenter = PARALLAXCENTER;
        info.EmissionScale = EMISSIONSCALE;
        info.EmissionScale2 = EMISSIONSCALE2;
        info.HSV = HSV;
        info.HSV_blend = HSV_BLEND;

		info.BumpFrame = BUMPFRAME;

		// Detail textures...
		//info.DetailTexture = DETAIL;
		//info.DetailTransform = DETAILTEXTURETRANSFORM;
		//info.DetailFrame = DETAILFRAME;
		//info.DetailScale = DETAILSCALE;
		//info.DetailTint = DETAILTINT;
		//info.DetailBlendMode = DETAILBLENDMODE;
		//info.DetailBlendFactor = DETAILBLENDFACTOR;

		// Envmapping...
		info.EnvMapTint = ENVMAPTINT;
		info.EnvMapContrast = ENVMAPCONTRAST;
		info.EnvMapSaturation = ENVMAPSATURATION;

		// Parallax Corrected Cubemaps...
		info.EnvmapParallax = ENVMAPPARALLAX;
		info.EnvMapParallaxOBB1 = ENVMAPPARALLAXOBB1;
		info.EnvMapParallaxOBB2 = ENVMAPPARALLAXOBB2;
		info.EnvMapParallaxOBB3 = ENVMAPPARALLAXOBB3;
		info.EnvmapOrigin = ENVMAPORIGIN;
    };

    // Initializing parameters
    SHADER_INIT_PARAMS()
    {

		// WRD: There used to be a fallback that would put the string from $normaltexture to $bumpmap.
		//		However I inverted this for model lightmapping support ( and for the future )
		//		In SDK2013mp, which Map Labs/Base is yet* not using, the engine will disable model lightmaps-
		//		if you have $bumpmap or $phong. So we set whatever $bumpmap is to $normaltexture and then undefine it
		//		which allows you to use $bumpmap for model lightmapping.

        // Fallback for changed parameter
		if (params[BUMPMAP]->IsDefined())
		{
			params[NORMALTEXTURE]->SetStringValue(params[BUMPMAP]->GetStringValue());
			params[BUMPMAP]->SetUndefined(); // extremely important for model lightmaps to work with $bumpmap
		}

        // Dynamic lights need a bumpmap
		// WRD : these checks are not required, Shaderdraw checks for whether or not its a texture, if not it will bind a default engine one.
        //	if (!params[NORMALTEXTURE]->IsDefined())
        //  params[NORMALTEXTURE]->SetStringValue("dev/flat_normal");
		//	if (!params[BUMPMAP2]->IsDefined())
		//	params[BUMPMAP2]->SetStringValue("dev/flat_normal");

		if (!params[MRAOTEXTURE2]->IsDefined())
			params[MRAOTEXTURE2]->SetStringValue("dev/pbr_mraotexture");

        // Set a good default mrao texture
        if (!params[MRAOTEXTURE]->IsDefined())
            params[MRAOTEXTURE]->SetStringValue("dev/pbr_mraotexture");

        // PBR relies heavily on envmaps
        if (!params[ENVMAP]->IsDefined())
            params[ENVMAP]->SetStringValue("env_cubemap");

        // Check if the hardware supports flashlight border color
        if (g_pHardwareConfig->SupportsBorderColor())
        {
            params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight_border");
        }
        else
        {
            params[FLASHLIGHTTEXTURE]->SetStringValue("effects/flashlight001");
        }
    };

    // Define shader fallback
    SHADER_FALLBACK
    {
        return 0;
    };

    SHADER_INIT
	{
		PBR_Vars_t info;
		SetupVars(params, info);

		// WRD: This texture should always exist. If it isn't then something else is SERIOUSLY wrong!
		LoadTexture(info.FlashlightTexture, TEXTUREFLAGS_SRGB);

		if (params[NORMALTEXTURE]->IsDefined())
		{
			LoadBumpMap(info.NormalTexture);
		}

		if (params[BUMPMAP2]->IsDefined())
		{
			LoadBumpMap(info.BumpMap2);
		}

		int envMapFlags = g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE ? TEXTUREFLAGS_SRGB : 0;
		// WRD: This does NOT work. If any other material before the pbr one loads the envmap WITHOUT this flag, then loading it again here will NOT add it.
		//		As a workaround, you can manually flag your cubemaps with the 'No Minimum Mipmap" flag to force this behaviour.
		envMapFlags |= TEXTUREFLAGS_ALL_MIPS;
		LoadCubeMap(info.EnvMap, envMapFlags);

		// WRD I removed the >= 0 checks because they won't do anything.
		// Note : all the previous branching didn't check it either so why should these textures work differently?
		if (params[EMISSIONTEXTURE]->IsDefined())
			LoadTexture(info.EmissionTexture, TEXTUREFLAGS_SRGB);

		if (params[EMISSIONTEXTURE2]->IsDefined())
			LoadTexture(info.EmissionTexture2, TEXTUREFLAGS_SRGB);

		Assert(info.MRAOTexture >= 0);
		LoadTexture(info.MRAOTexture, 0);

		Assert(info.MRAOTexture2 >= 0);
		LoadTexture(info.MRAOTexture2, 0);

		if (params[info.BaseTexture]->IsDefined())
		{
			LoadTexture(info.BaseTexture, TEXTUREFLAGS_SRGB);
		}

		if (params[info.BaseTexture2]->IsDefined())
		{
			LoadTexture(info.BaseTexture2, TEXTUREFLAGS_SRGB);
		}

		//if (params[info.DetailTexture]->IsDefined())
		//{
			//if (params[info.DetailBlendMode]->GetIntValue() > 0)
			//{
			//	LoadTexture(info.DetailTexture, 0);
			//}
			//else
			//{
			//	LoadTexture(info.DetailTexture, TEXTUREFLAGS_SRGB); // when mod2x, load as sRGB
			//}
		//}

        if (IS_FLAG_SET(MATERIAL_VAR_MODEL)) // Set material var2 flags specific to models
        {
            SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_HW_SKINNING);             // Required for skinning
            SET_FLAGS2(MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL);         // Required for dynamic lighting
            SET_FLAGS2(MATERIAL_VAR2_NEEDS_TANGENT_SPACES);             // Required for dynamic lighting
            SET_FLAGS2(MATERIAL_VAR2_LIGHTING_VERTEX_LIT);              // Required for dynamic lighting
            SET_FLAGS2(MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS);   // Required for ambient cube
            SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);              // Required for flashlight
            SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);                   // Required for flashlight
        }
        else // Set material var2 flags specific to brushes
        {
            SET_FLAGS2(MATERIAL_VAR2_LIGHTING_LIGHTMAP);                // Required for lightmaps
            SET_FLAGS2(MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP);         // Required for lightmaps
            SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);              // Required for flashlight
            SET_FLAGS2(MATERIAL_VAR2_USE_FLASHLIGHT);                   // Required for flashlight
        }
    };

    // Drawing the shader
    SHADER_DRAW
	{
		PBR_Vars_t info;
		SetupVars(params, info);

		// Setting up booleans
		// WRD : I removed all the != -1, because this will ALWAYS return true!
		//		 ->IsDefined() also does this from time to time, so I replaced those with != defaultvalue, when applicable
		bool bIsAlphaTested = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST) != 0;
		bool bIsModel = IS_FLAG_SET(MATERIAL_VAR_MODEL);
		bool bBrush = !bIsModel; // If its not a model, its a brush... or a displacement

		bool bLightmappedModel = params[info.LightmapTexture]->IsTexture() && bIsModel;
		bool bHasBaseTexture = params[info.BaseTexture]->IsTexture();
		bool bIsWVT = params[info.BaseTexture2]->IsTexture();

		// WRD: We define them as false first so we can check them ONLY if bIsWVT is set.
		bool bHasNormalTexture2 = false;
		bool bHasMraoTexture2 = false;
		bool bHasEmissionTexture2 = false;
		bool bHasEmissionScale2 = false;

		bool bHasHSV = (info.HSV != -1) && params[info.HSV]->IsDefined();
		bool bBlendHSV = bHasHSV && IsBoolSet(info.HSV_blend, params);
		bool bHasParallaxCorrection = params[info.EnvmapParallax]->GetIntValue() != 0;

		// WRD: Don't bother checking for Texture2's if you aren't on WVT, waste of effort
		if (bIsWVT)
		{
			// Had to skip this in order to save a PSREG for PCC
			bHasParallaxCorrection = false;
			bHasNormalTexture2 = params[info.BumpMap2]->IsTexture();
			bHasEmissionTexture2 = params[info.EmissionTexture2]->IsTexture();
			bHasMraoTexture2 = params[info.MRAOTexture2]->IsTexture();
			bHasEmissionScale2 = params[info.EmissionScale2]->IsDefined();
		}

		bool bHasNormalTexture = params[info.NormalTexture]->IsTexture();
		bool bHasMraoTexture = params[info.MRAOTexture]->IsTexture();
		bool bHasEmissionTexture = params[info.EmissionTexture]->IsTexture();
		bool bHasEnvTexture = params[info.EnvMap]->IsTexture();
		//bool bHasDetailTexture = params[info.DetailTexture]->IsTexture();
		bool bHasFlashlight = UsingFlashlight(params);
		bool bHasEmissionScale = params[info.EmissionScale]->IsDefined();
		bool bHasParallax = params[info.UseParallax]->GetIntValue() != 0;

		// I'd rather check this once per shader instance than multiple times per shader instance...
		bool bSupportsSM30 = g_pHardwareConfig->SupportsShaderModel_3_0();

        // Determining whether we're dealing with a fully opaque material
        BlendType_t nBlendType = EvaluateBlendRequirements(info.BaseTexture, true);
        bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested;

        if (IsSnapshotting())
        {
            // If alphatest is on, enable it
            pShaderShadow->EnableAlphaTest(bIsAlphaTested);

            if (info.AlphaTestReference != -1 && params[info.AlphaTestReference]->GetFloatValue() > 0.0f)
            {
                pShaderShadow->AlphaFunc(SHADER_ALPHAFUNC_GEQUAL, params[info.AlphaTestReference]->GetFloatValue());
            }

            if (bHasFlashlight )
            {
                pShaderShadow->EnableBlending(true);
                pShaderShadow->BlendFunc(SHADER_BLEND_ONE, SHADER_BLEND_ONE); // Additive blending
            }
            else
            {
                SetDefaultBlendingShadowState(info.BaseTexture, true);
            }

            int nShadowFilterMode = bHasFlashlight ? g_pHardwareConfig->GetShadowFilterMode() : 0;

            // Setting up samplers
            pShaderShadow->EnableTexture(SAMPLER_BASETEXTURE, true);    // Basecolor texture
            pShaderShadow->EnableSRGBRead(SAMPLER_BASETEXTURE, true);   // Basecolor is sRGB
            pShaderShadow->EnableTexture(SAMPLER_EMISSIVE, true);       // Emission texture
            pShaderShadow->EnableSRGBRead(SAMPLER_EMISSIVE, true);      // Emission is sRGB
            pShaderShadow->EnableTexture(SAMPLER_LIGHTMAP, true);       // Lightmap texture
            pShaderShadow->EnableSRGBRead(SAMPLER_LIGHTMAP, false);     // Lightmaps aren't sRGB
            pShaderShadow->EnableTexture(SAMPLER_MRAO, true);           // MRAO texture
            pShaderShadow->EnableSRGBRead(SAMPLER_MRAO, false);         // MRAO isn't sRGB
            pShaderShadow->EnableTexture(SAMPLER_NORMAL, true);         // Normal texture
            pShaderShadow->EnableSRGBRead(SAMPLER_NORMAL, false);       // Normals aren't sRGB

			//if (bHasDetailTexture)
			//pShaderShadow->EnableTexture(SAMPLER_DETAIL, true);			// Detail texture
			//{
			//	if (params[info.DetailBlendMode]->GetIntValue() > 0)
			//	{
			//		pShaderShadow->EnableSRGBRead(SAMPLER_DETAIL, false);		// WRD: Must be sRGB as valve does that ( consistency )
			//	}
			//	else
			//	{
			//		pShaderShadow->EnableSRGBRead(SAMPLER_DETAIL, true);		// WRD: Must be sRGB as valve does that ( consistency )
			//	}
			//}

            if (bIsWVT)
            {
                pShaderShadow->EnableTexture(SAMPLER_BASETEXTURE2, true);
                pShaderShadow->EnableSRGBRead(SAMPLER_BASETEXTURE2, true);
                pShaderShadow->EnableTexture(SAMPLER_EMISSIVE2, true);
                pShaderShadow->EnableSRGBRead(SAMPLER_EMISSIVE2, true);
                pShaderShadow->EnableTexture(SAMPLER_MRAO2, true);
                pShaderShadow->EnableSRGBRead(SAMPLER_MRAO2, false);
                pShaderShadow->EnableTexture(SAMPLER_NORMAL2, true);
                pShaderShadow->EnableSRGBRead(SAMPLER_NORMAL2, false);
            }

            // If the flashlight is on, set up its textures
            if (bHasFlashlight)
            {
                pShaderShadow->EnableTexture(SAMPLER_SHADOWDEPTH, true);        // Shadow depth map
                pShaderShadow->SetShadowDepthFiltering(SAMPLER_SHADOWDEPTH);
                pShaderShadow->EnableSRGBRead(SAMPLER_SHADOWDEPTH, false);
                pShaderShadow->EnableTexture(SAMPLER_RANDOMROTATION, true);     // Noise map
                pShaderShadow->EnableTexture(SAMPLER_FLASHLIGHT, true);         // Flashlight cookie
                pShaderShadow->EnableSRGBRead(SAMPLER_FLASHLIGHT, true);

				FogToBlack();
            }
			else
			{
				DefaultFog();
			}

            // Setting up envmap
            if (bHasEnvTexture)
            {
                pShaderShadow->EnableTexture(SAMPLER_ENVMAP, true); // Envmap
                if (g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE)
                {
                    pShaderShadow->EnableSRGBRead(SAMPLER_ENVMAP, true); // Envmap is only sRGB with HDR disabled?
                }
            }

            // Enabling sRGB writing
            // See common_ps_fxc.h line 349
            // PS2b shaders and up write sRGB
            pShaderShadow->EnableSRGBWrite(true);

            if (IS_FLAG_SET(MATERIAL_VAR_MODEL))
            {
                // We only need the position and surface normal
                unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
                // We need three texcoords, all in the default float2 size
                pShaderShadow->VertexShaderVertexFormat(flags, 1, 0, 0);
            }
            else
            {
                // We need the position, surface normal, and vertex compression format
                unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL;
                if (bIsWVT)
                    flags |= VERTEX_COLOR;
                // We only need one texcoord, in the default float2 size
                pShaderShadow->VertexShaderVertexFormat(flags, 3, 0, 0);
            }

            if (!mat_pbr_parallaxmap.GetBool())
            {
				bHasParallax = false;
            }
			
			if (!bSupportsSM30 || mat_pbr_force_20b.GetBool())
            {
                // Setting up static vertex shader
                DECLARE_STATIC_VERTEX_SHADER(pbr_vs20b);
                SET_STATIC_VERTEX_SHADER_COMBO(WVT, bIsWVT);
                SET_STATIC_VERTEX_SHADER(pbr_vs20b);

                // Setting up static pixel shader
                DECLARE_STATIC_PIXEL_SHADER(pbr_ps20b);
                SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, bHasFlashlight);
                SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode);
                SET_STATIC_PIXEL_SHADER_COMBO(LIGHTMAPPED, bBrush);
                SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, bHasEmissionTexture);
                SET_STATIC_PIXEL_SHADER_COMBO(WVT, bIsWVT);
				SET_STATIC_PIXEL_SHADER_COMBO(HSV, bHasHSV);
				SET_STATIC_PIXEL_SHADER_COMBO(HSV_BLEND, bBlendHSV);
                SET_STATIC_PIXEL_SHADER(pbr_ps20b);
            }
            else
            {
                // Setting up static vertex shader
                DECLARE_STATIC_VERTEX_SHADER(pbr_vs30);
                SET_STATIC_VERTEX_SHADER_COMBO(WVT, bIsWVT);
                SET_STATIC_VERTEX_SHADER(pbr_vs30);

                // Setting up static pixel shader
                DECLARE_STATIC_PIXEL_SHADER(pbr_ps30);
                SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHT, bHasFlashlight);
                SET_STATIC_PIXEL_SHADER_COMBO(FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode);
                SET_STATIC_PIXEL_SHADER_COMBO(LIGHTMAPPED, bBrush);
                SET_STATIC_PIXEL_SHADER_COMBO(EMISSIVE, bHasEmissionTexture);
				SET_STATIC_PIXEL_SHADER_COMBO(PARALLAXOCCLUSION, bHasParallax);
                SET_STATIC_PIXEL_SHADER_COMBO(WVT, bIsWVT);
				SET_STATIC_PIXEL_SHADER_COMBO(PCC, bHasParallaxCorrection);
				SET_STATIC_PIXEL_SHADER_COMBO(HSV, bHasHSV);
				SET_STATIC_PIXEL_SHADER_COMBO(HSV_BLEND, bBlendHSV);
                SET_STATIC_PIXEL_SHADER(pbr_ps30);
            }

            // HACK HACK HACK - enable alpha writes all the time so that we have them for underwater stuff
            pShaderShadow->EnableAlphaWrites(bFullyOpaque);
        }
        else // Not snapshotting -- begin dynamic state
        {
            bool bLightingOnly = mat_fullbright.GetInt() == 2 && !IS_FLAG_SET(MATERIAL_VAR_NO_DEBUG_OVERRIDE);

            // Setting up albedo texture
            if (bHasBaseTexture)
            {
                BindTexture(SAMPLER_BASETEXTURE, info.BaseTexture, info.BaseTextureFrame);
            }
            else
            {
                pShaderAPI->BindStandardTexture(SAMPLER_BASETEXTURE, TEXTURE_GREY);
            }

			// Setting up basecolor tint
			Vector4D HSV;
			if (bHasHSV)
			{
				params[info.HSV]->GetVecValue(HSV.Base(), 3);
				HSV.w = pShaderAPI->GetLightMapScaleFactor();
			}
			else
			{
				HSV.Init(1.0f, 1.0f, 1.0f, pShaderAPI->GetLightMapScaleFactor());
			}
			pShaderAPI->SetPixelShaderConstant(PSREG_SELFILLUMTINT, HSV.Base());

            // Setting up emission scale
            Vector EmissionScale;
            if (bHasEmissionScale)
            {
                params[info.EmissionScale]->GetVecValue(EmissionScale.Base(), 3);
            }
            else
            {
                EmissionScale.Init(1.0f, 1.0f, 1.0f);
            }
            pShaderAPI->SetPixelShaderConstant(2, EmissionScale.Base());

            // Setting up environment map
            if (bHasEnvTexture)
            {
                BindTexture(SAMPLER_ENVMAP, info.EnvMap, 0);
            }
            else
            {
                pShaderAPI->BindStandardTexture(SAMPLER_ENVMAP, TEXTURE_BLACK);
            }

			//if (bHasDetailTexture)
			//{
			//	//DevMsg("Detail Texture Set \n");
			//	BindTexture(SHADER_SAMPLER15, info.DetailTexture, 0);
			//}

            // Setting up emissive texture
            if (bHasEmissionTexture)
            {
                BindTexture(SAMPLER_EMISSIVE, info.EmissionTexture, 0);
            }
			// This USED TO bind a default black texture, however the shader has a static for Emissiontexture, so if there is no emissiontexture, it can't use one either.

            // Setting up normal map
            if (bHasNormalTexture)
            {
				BindTexture(SAMPLER_NORMAL, info.NormalTexture, info.BumpFrame);
            }
            else
            {
                pShaderAPI->BindStandardTexture(SAMPLER_NORMAL, TEXTURE_NORMALMAP_FLAT);
            }

            // Setting up mrao map
            if (bHasMraoTexture)
            {
                BindTexture(SAMPLER_MRAO, info.MRAOTexture, 0);
            }
            else
            {
                pShaderAPI->BindStandardTexture(SAMPLER_MRAO, TEXTURE_WHITE);
            }

            if (bIsWVT)
            {
                BindTexture(SAMPLER_BASETEXTURE2, info.BaseTexture2, info.BaseTextureFrame2);

                if (bHasEmissionTexture2)
                    BindTexture(SAMPLER_EMISSIVE2, info.EmissionTexture2, 0);
                else
                    pShaderAPI->BindStandardTexture(SAMPLER_EMISSIVE2, TEXTURE_BLACK);

                if (bHasNormalTexture2)
                    BindTexture(SAMPLER_NORMAL2, info.BumpMap2, 0);
                else
                    pShaderAPI->BindStandardTexture(SAMPLER_NORMAL2, TEXTURE_NORMALMAP_FLAT);

                if (bHasMraoTexture2)
                    BindTexture(SAMPLER_MRAO2, info.MRAOTexture2, 0);
                else
                    pShaderAPI->BindStandardTexture(SAMPLER_MRAO2, TEXTURE_WHITE);

                if (bHasEmissionScale2)
                    params[info.EmissionScale2]->GetVecValue(EmissionScale.Base(), 3);
                else
                    EmissionScale.Init(1.0f, 1.0f, 1.0f);
                pShaderAPI->SetPixelShaderConstant(3, EmissionScale.Base());
            }

            // Getting the light state
            LightState_t lightState;
            pShaderAPI->GetDX9LightState(&lightState);

            // Setting up the flashlight related textures and variables
            bool bFlashlightShadows = false;
            if (bHasFlashlight)
            {
                Assert(info.FlashlightTexture >= 0 && info.FlashlightTextureFrame >= 0);
                Assert(params[info.FlashlightTexture]->IsTexture());
                BindTexture(SAMPLER_FLASHLIGHT, info.FlashlightTexture, info.FlashlightTextureFrame);
                VMatrix worldToTexture;
                ITexture *pFlashlightDepthTexture;
                FlashlightState_t state = pShaderAPI->GetFlashlightStateEx(worldToTexture, &pFlashlightDepthTexture);
                bFlashlightShadows = state.m_bEnableShadows && (pFlashlightDepthTexture != NULL);

                SetFlashLightColorFromState(state, pShaderAPI, PSREG_FLASHLIGHT_COLOR);

                if (pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && state.m_bEnableShadows)
                {
                    BindTexture(SAMPLER_SHADOWDEPTH, pFlashlightDepthTexture, 0);
                    pShaderAPI->BindStandardTexture(SAMPLER_RANDOMROTATION, TEXTURE_SHADOW_NOISE_2D);
                }
            }

            // Getting fog info
            MaterialFogMode_t fogType = pShaderAPI->GetSceneFogMode();
            int fogIndex = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z) ? 1 : 0;

            // Getting skinning info
            int numBones = pShaderAPI->GetCurrentNumBones();

            // Some debugging stuff
            bool bWriteDepthToAlpha = false;
            bool bWriteWaterFogToAlpha = false;
            if (bFullyOpaque)
            {
                bWriteDepthToAlpha = pShaderAPI->ShouldWriteDepthToDestAlpha();
                bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
                AssertMsg(!(bWriteDepthToAlpha && bWriteWaterFogToAlpha),
                        "Can't write two values to alpha at the same time.");
            }

            float vEyePos_SpecExponent[4];
            pShaderAPI->GetWorldSpaceCameraPosition(vEyePos_SpecExponent);

            // Determining the max level of detail for the envmap
            int iEnvMapLOD = 6;
            auto envTexture = params[info.EnvMap]->GetTextureValue();
            if (envTexture)
            {
                // Get power of 2 of texture width
                int width = envTexture->GetMappingWidth();
                int mips = 0;
                while (width >>= 1)
                    ++mips;

                // Cubemap has 4 sides so 2 mips less
                iEnvMapLOD = mips;
            }

            // Dealing with very high and low resolution cubemaps
			// WRD : Note, this only works when the cubemap is flagged to have all mips OR if you run the game with -forceallmips
            if (iEnvMapLOD > 12)
                iEnvMapLOD = 12;
            if (iEnvMapLOD < 4)
                iEnvMapLOD = 4;

            // This has some spare space
            vEyePos_SpecExponent[3] = iEnvMapLOD;
            pShaderAPI->SetPixelShaderConstant(PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1); // c11

            // Setting lightmap texture
			if (bBrush)
			{
				// Brushes don't need ambient cubes or dynamic lights
				lightState.m_bAmbientLight = false;
				lightState.m_nNumLights = 0;

				pShaderAPI->BindStandardTexture(SAMPLER_LIGHTMAP, TEXTURE_LIGHTMAP_BUMPED);
			}
            
			// WRD: Model Lightmapping Technologies!!... needs to be specified manually in SP
			if (bLightmappedModel)
			{
				BindTexture(SAMPLER_LIGHTMAP, info.LightmapTexture);
			}

            if (!bSupportsSM30 || mat_pbr_force_20b.GetBool())
            {
                // Setting up dynamic vertex shader
                DECLARE_DYNAMIC_VERTEX_SHADER(pbr_vs20b);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(LIGHTING_PREVIEW, pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
                SET_DYNAMIC_VERTEX_SHADER(pbr_vs20b);

                // Setting up dynamic pixel shader
                DECLARE_DYNAMIC_PIXEL_SHADER(pbr_ps20b);
                SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
                SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha);
                SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha);
                SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
                SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, bFlashlightShadows);
                SET_DYNAMIC_PIXEL_SHADER(pbr_ps20b);
            }
            else
            {
                // Setting up dynamic vertex shader
                DECLARE_DYNAMIC_VERTEX_SHADER(pbr_vs30);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(DOWATERFOG, fogIndex);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, numBones > 0);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(LIGHTING_PREVIEW, pShaderAPI->GetIntRenderingParameter(INT_RENDERPARM_ENABLE_FIXED_LIGHTING) != 0);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(COMPRESSED_VERTS, (int)vertexCompression);
                SET_DYNAMIC_VERTEX_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
                SET_DYNAMIC_VERTEX_SHADER(pbr_vs30);

                // Setting up dynamic pixel shader
                DECLARE_DYNAMIC_PIXEL_SHADER(pbr_ps30);
                SET_DYNAMIC_PIXEL_SHADER_COMBO(NUM_LIGHTS, lightState.m_nNumLights);
                SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITEWATERFOGTODESTALPHA, bWriteWaterFogToAlpha);
                SET_DYNAMIC_PIXEL_SHADER_COMBO(WRITE_DEPTH_TO_DESTALPHA, bWriteDepthToAlpha);
                SET_DYNAMIC_PIXEL_SHADER_COMBO(PIXELFOGTYPE, pShaderAPI->GetPixelFogCombo());
                SET_DYNAMIC_PIXEL_SHADER_COMBO(FLASHLIGHTSHADOWS, bFlashlightShadows);
				// WRD: This **HAS TO BE** a dynamic combo. Doesn't matter much for SP, but on MP lightmapping would NOT work if it was a static.
				//		It doesn't hurt to be one in either case.
				SET_DYNAMIC_PIXEL_SHADER_COMBO(LIGHTMAPPED_MODEL, bLightmappedModel);
                SET_DYNAMIC_PIXEL_SHADER(pbr_ps30);
            }

            // Setting up base texture transform
            SetVertexShaderTextureTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, info.BaseTextureTransform);

			//float DetailScale = params[info.DetailScale]->GetFloatValue();
			//bHasDetailScale = DetailScale != 0.000f;
			//if (!bHasDetailScale)
			//{
			//	DetailScale = 4.0f;
			//}

			//if (bHasDetailTexture)
			//{
				//if (params[info.DetailTransform]->IsDefined())
				//{
				//	// Send the Transformdata to the	  Vertex_Shader_Shader
				//	SetVertexShaderTextureScaledTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, info.DetailTransform, info.DetailScale);
				//}
				//else
				//{
				//	// Even if we don't Transform the DetailTexture we set detailscale and whatever BaseTextureTransform is set to, so detail textures line up
				//	SetVertexShaderTextureScaledTransform(VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, info.BaseTextureTransform, info.DetailScale);
				//}
			//}

            // This is probably important
            SetModulationPixelShaderDynamicState_LinearColorSpace(PSREG_DIFFUSE_MODULATION);

            // Send ambient cube to the pixel shader, force to black if not available
            pShaderAPI->SetPixelShaderStateAmbientLightCube(PSREG_AMBIENT_CUBE, !lightState.m_bAmbientLight);

            // Send lighting array to the pixel shader
            pShaderAPI->CommitPixelShaderLighting(PSREG_LIGHT_INFO_ARRAY);

            // Handle mat_fullbright 2 (diffuse lighting only)
            if (bLightingOnly)
            {
                pShaderAPI->BindStandardTexture(SAMPLER_BASETEXTURE, TEXTURE_GREY); // Basecolor
            }

            // Handle mat_specular 0 (no envmap reflections)
            if (!mat_specular.GetBool())
            {
                pShaderAPI->BindStandardTexture(SAMPLER_ENVMAP, TEXTURE_BLACK); // Envmap
            }

            // Sending fog info to the pixel shader
            pShaderAPI->SetPixelShaderFogParams(PSREG_FOG_PARAMS);

            // More flashlight related stuff
            if (bHasFlashlight)
            {
                VMatrix worldToTexture;
                float atten[4], pos[4], tweaks[4];

                const FlashlightState_t &flashlightState = pShaderAPI->GetFlashlightState(worldToTexture);
                SetFlashLightColorFromState(flashlightState, pShaderAPI, PSREG_FLASHLIGHT_COLOR);

                BindTexture(SAMPLER_FLASHLIGHT, flashlightState.m_pSpotlightTexture, flashlightState.m_nSpotlightTextureFrame);

                // Set the flashlight attenuation factors
                atten[0] = flashlightState.m_fConstantAtten;
                atten[1] = flashlightState.m_fLinearAtten;
                atten[2] = flashlightState.m_fQuadraticAtten;
                atten[3] = flashlightState.m_FarZ;
                pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_ATTENUATION, atten, 1);

                // Set the flashlight origin
                pos[0] = flashlightState.m_vecLightOrigin[0];
                pos[1] = flashlightState.m_vecLightOrigin[1];
                pos[2] = flashlightState.m_vecLightOrigin[2];
                pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_POSITION_RIM_BOOST, pos, 1);

                pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_TO_WORLD_TEXTURE, worldToTexture.Base(), 4);

                // Tweaks associated with a given flashlight
                tweaks[0] = ShadowFilterFromState(flashlightState);
                tweaks[1] = ShadowAttenFromState(flashlightState);
                HashShadow2DJitter(flashlightState.m_flShadowJitterSeed, &tweaks[2], &tweaks[3]);
                pShaderAPI->SetPixelShaderConstant(PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks, 1);
            }
			else
			{
				// WRD : All of the below should only be set when no flashlight is used.

				float fEnvMapSaturation = 1;
				fEnvMapSaturation = params[info.EnvMapSaturation]->GetFloatValue();
				float fEnvMapContrast = 0;
				fEnvMapContrast = params[info.EnvMapContrast]->GetFloatValue();

				if (!params[info.EnvMapSaturation]->GetFloatValue() != 0)
				{
					fEnvMapSaturation = 1.0f;
				}

				// Packing to fit it into 32 constant registers.
				float fEnvMapTint[3] = { 0, 0, 0 };
				params[info.EnvMapTint]->GetVecValue(fEnvMapTint, 3);
				// floor makes sure that envmapcontrast values below 0.01 doesn't affect the saturation.
				float fCombine[4] = { fEnvMapTint[0], fEnvMapTint[1], fEnvMapTint[2], (floor(fEnvMapContrast * 100) * 0.01) + (fEnvMapSaturation * 0.01) };

				pShaderAPI->SetPixelShaderConstant(31, fCombine);

				if (bHasParallaxCorrection)
				{
					float envMapOrigin[4] = { 0, 0, 0, 0 };
					params[info.EnvmapOrigin]->GetVecValue(envMapOrigin, 3);
					pShaderAPI->SetPixelShaderConstant(10, params[info.EnvmapOrigin]->GetVecValue());

					float* vecs[3];
					vecs[0] = const_cast<float*>(params[info.EnvMapParallaxOBB1]->GetVecValue());
					vecs[1] = const_cast<float*>(params[info.EnvMapParallaxOBB2]->GetVecValue());
					vecs[2] = const_cast<float*>(params[info.EnvMapParallaxOBB3]->GetVecValue());

					// Is this Matrix just here to eat FPS? Whats it for
					float matrix[4][4];
					for (int i = 0; i < 3; i++)
					{
						for (int j = 0; j < 4; j++)
						{
							matrix[i][j] = vecs[i][j];
						}
					}
					// This will break your fog
					//matrix[3][0] = matrix[3][1] = matrix[3][2] = 0;
					//matrix[3][3] = 1;

					// Will only break it if you set 4 instead of 3 here I think
					pShaderAPI->SetPixelShaderConstant(26, &matrix[0][0], 3);
				}
			}

			// WRD : This used to be outside of an if-statement, leading to a bunch of crashes on my end...
			//		 I changed useParallax to a bool and check that here now.
			if (bHasParallax)
			{
				float flParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				// Parallax Depth (the strength of the effect)
				flParams[0] = GetFloatParam(info.ParallaxDepth, params, 3.0f);
				// Parallax Center (the height at which it's not moved)
				flParams[1] = GetFloatParam(info.ParallaxCenter, params, 3.0f);

				// Fitting it into 32 registers...
				if (bHasParallaxCorrection)
				{
					pShaderAPI->SetPixelShaderConstant(3, flParams, 1);
				}
				else
				{
					pShaderAPI->SetPixelShaderConstant(27, flParams, 1);
				}
			}

		}// WRD : make sure this bracket is above Draw(); or you get a ton of crashes... Happened to me once...
        // Actually draw the shader
        Draw();
    };

// Closing it off
END_SHADER;