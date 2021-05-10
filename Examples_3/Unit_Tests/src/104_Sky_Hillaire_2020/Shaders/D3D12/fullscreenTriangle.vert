struct VSOutput
{
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
};

VSOutput main(in uint id : SV_VertexID)
{
	VSOutput out_vs;
	out_vs.texcoord.x = (id == 2) ?  2.0 :  0.0;
	out_vs.texcoord.y = (id == 1) ?  2.0 :  0.0;

	out_vs.position = float4(out_vs.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
	return out_vs;
}