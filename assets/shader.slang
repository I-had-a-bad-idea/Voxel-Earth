struct VSInput {
    uint Pos;
    uint Normal;
    uint UV;
    uint AtlasTile;
};


Sampler2D textures[];

struct ObjectData {
    float4x4 model;
    uint32_t texture_index;
    uint32_t selected;
};

struct PushConstants {
    ShaderData* shader_data_addrress;
    float time;
};

struct ShaderData {
    float4x4 projection;
    float4x4 view;
    float4 lightPos;
    ObjectData objects[4096];
};

struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
    float3 Factor;
    float3 LightVec;
    float3 ViewVec;
    uint32_t InstanceIndex;
    uint32_t TextureIndex;
    uint2 AtlasTile;
};

float3 decode_position(uint packed)
{
    return float3(
        packed & 0x7Fu,
        (packed >> 7) & 0x7Fu,
        (packed >> 14) & 0x7Fu
    );
}

float3 decode_normal(uint packed)
{
    switch (packed & 7u)
    {
    case 0: return float3(-1, 0, 0);
    case 1: return float3(1, 0, 0);
    case 2: return float3(0, -1, 0);
    case 3: return float3(0, 1, 0);
    case 4: return float3(0, 0, -1);
    case 5: return float3(0, 0, 1);
    }

    return float3(0, 0, 0);
}

float2 decode_uv(uint packed)
{
    return float2(
        packed & 0xFFFFu,
        packed >> 16
    );
}

uint2 decode_atlas_tile(uint packed)
{
    return uint2(
        packed & 0xFFFFu,
        packed >> 16
    );
}

[shader("vertex")]
VSOutput main(
    VSInput input,
    uniform PushConstants pushConstants,
    uint instanceIndex : SV_VulkanInstanceID)
{
    VSOutput output;
    ShaderData* shaderData = (ShaderData*)pushConstants.shader_data_addrress;

    ObjectData obj = shaderData->objects[instanceIndex];
    float4x4 modelMat = obj.model;
    output.TextureIndex = obj.texture_index;

    float4x4 viewModel = mul(shaderData->view, modelMat);

    float3 position = decode_position(input.Pos);
    float3 normal = decode_normal(input.Normal);
    float2 uv = decode_uv(input.UV);
    uint2 atlasTile = decode_atlas_tile(input.AtlasTile);

    output.Normal = mul((float3x3)viewModel, normal);

    uv.y = 1.0f - uv.y; // Do the flip, that is no longer done when loading the mesh
    output.UV = uv;
    output.AtlasTile = atlasTile;

    output.Pos = mul(
        shaderData->projection,
        mul(viewModel, float4(position, 1.0))
    );

    output.Factor = obj.selected != 0 ? 3.0 : 1.0;
    output.InstanceIndex = instanceIndex;

    float3 fragPos = mul(viewModel, float4(position, 1.0)).xyz;
    float3 lightPos = mul(shaderData->view, shaderData->lightPos).xyz;

    output.LightVec = lightPos - fragPos;
    output.ViewVec = -fragPos;

    return output;
}

static const float ATLAS_WIDTH = 16.0f;
static const float ATLAS_HEIGHT = 16.0f;

[shader("fragment")]
float4 main(VSOutput input) {
    // Phong lighting
    float3 N = normalize(input.Normal);
    float3 L = normalize(input.LightVec);
    float3 V = normalize(input.ViewVec);
    float3 R = reflect(-L, N);

    float3 ambient = 0.1;

    float3 diffuse = max(dot(N, L), 0.0025);
    
    // Repeat the texture for every block in a greedy quad.
    float2 localUV = frac(input.UV);

    float2 tileSize = 1.0 / float2(ATLAS_WIDTH, ATLAS_HEIGHT);
    float2 tileOffset = float2(input.AtlasTile.x, input.AtlasTile.y);

    float2 atlasUV = tileOffset * tileSize + localUV * tileSize;

    // Sample from texture
    float3 color = textures[NonUniformResourceIndex(input.TextureIndex)].Sample(atlasUV).rgb * input.Factor;
    return float4((ambient + diffuse) * color, 1.0);
}