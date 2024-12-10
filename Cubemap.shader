cbuffer ObjectBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

struct VertexInputType
{
    float4 position : POSITION;	
	float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 localPos : TEXCOORD1;
};

SamplerState textureSampler;
TextureCube environmentMap : register(t0);

PixelInputType VSMain(VertexInputType input)
{
	PixelInputType output;
    
    output.localPos = normalize(input.position.xyz);

    float4x4 newView = viewMatrix;
	newView[3][0] = 0.0;
	newView[3][1] = 0.0;
    newView[3][2] = 0.0;

    input.position.w = 1.0f;
    output.position = mul(input.position, newView);
    output.position = mul(output.position, projectionMatrix);

    output.position = output.position.xyzw;
    output.position.z = output.position.w * 0.9999;

    return output;
}

float4 PSMain(PixelInputType input) : SV_TARGET
{
    //float3 envColor = environmentMap.Sample(textureSampler, input.localPos).rgb;
    float3 envColor =  environmentMap.SampleLevel(textureSampler, input.localPos, 1.2).rgb;

    envColor = envColor / (envColor + float3(1, 1, 1));
    envColor = pow(envColor, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));

    return float4(envColor, 1.0);
}