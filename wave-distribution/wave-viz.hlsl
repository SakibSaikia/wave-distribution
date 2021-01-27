#define rootsig "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)"
#define PI 3.14159f

float rand(float2 coords) 
{
	return frac(sin(dot(coords.xy, float2(12.9898, 78.233))) * 43758.5453);
}

// http://paulbourke.net/miscellaneous/colourspace/
float3 hsv2rgb(float3 hsv)
{
	float3 rgb, sat;

	while (hsv.x < 0)
	{
		hsv.x += 360;
	}

	while (hsv.x > 360)
	{
		hsv.x -= 360;
	}

	if (hsv.x < 120)
	{
		sat.r = (120 - hsv.x) / 60.0;
		sat.g = hsv.x / 60.0;
		sat.b = 0;
	}
	else if (hsv.x < 240) 
	{
		sat.r = 0;
		sat.g = (240 - hsv.x) / 60.0;
		sat.b = (hsv.x - 120) / 60.0;
	}
	else 
	{
		sat.r = (hsv.x - 240) / 60.0;
		sat.g = 0;
		sat.b = (360 - hsv.x) / 60.0;
	}

	sat.r = min(sat.r, 1.f);
	sat.g = min(sat.g, 1.f);
	sat.b = min(sat.b, 1.f);

	rgb.r = (1 - hsv.y + hsv.y * sat.r) * hsv.z;
	rgb.g = (1 - hsv.y + hsv.y * sat.g) * hsv.z;
	rgb.b = (1 - hsv.y + hsv.y * sat.b) * hsv.z;

	return(rgb);
}

static const float4 g_verts[6] =
{
	float4(-0.5f, 0.5f, 0.f, 1.f),
	float4(0.5f, 0.5f, 0.f, 1.f),
	float4(-0.5f, -0.5f, 0.f, 1.f),
	float4(-0.5f, -0.5f, 0.f, 1.f),
	float4(0.5f, 0.5f, 0.f, 1.f),
	float4(0.5f, -0.5f, 0.f, 1.f)
};

float4 vs_main(uint vertexID : SV_VertexID) : SV_POSITION
{
	return g_verts[vertexID];
}

float4 ps_main(float4 pos : SV_POSITION) : SV_Target
{
	float3 color = float3(0.f, 1.f, 0.f);

	if (WaveGetLaneIndex() == 0)
	{
		float hue = 35.f * rand(pos.xy * 0.5f + 0.5f.xx) * 180 / PI;
		float sat = 35.f * rand(pos.yx * 0.5f + 0.5f.xx) * 180 / PI;
		color = hsv2rgb(float3(hue, 0.85f, 0.95f));
	}

	color = WaveReadLaneFirst(color);

	return float4(color, 1.f);
}