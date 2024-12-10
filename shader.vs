cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
}

float4 VS (float4 Pos : POSITION) : SV_POSITION
{
	Pos = mul(Pos, World);
	Pos = mul(Pos, View);
	Pos = mul(Pos, Projection);

	return Pos;
}