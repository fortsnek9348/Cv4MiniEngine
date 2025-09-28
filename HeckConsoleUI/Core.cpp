#ifdef _MSC_VER
// Has to be first becuase of modules.
#include <crtdbg.h>
#endif

#include "inc/HeckTextUI/Core.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <iostream>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <Windows.h>
#endif

using namespace hecktui;

// UPDATE: The 6x6x6 colour is /not/ proportional to SRGB!
// https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797?permalink_comment_id=5138847#gistcomment-5138847
// Had to update this code.

constexpr EColour _encodeCubeColour(int r, int g, int b)
{
	return EColour(int(EColour::Grey0) + 6 * 6 * r + 6 * g + b);
}

static constexpr std::array<int, 3> _decodeCubeColour(EColour colour)
{
	int index = int(colour);
	index -= int(hecktui::EColour::Grey0);
	assert(unsigned(index) < 6 * 6 * 6);
	const int b = index % 6; index /= 6;
	const int g = index % 6; index /= 6;
	const int r = index % 6;
	return { r, g, b };
}

static constexpr uint8_t _ansiColourCubeComponentToByte(int i)
{
	return uint8_t(i <= 1 ? i * 95 : 135 + (i - 2) * 40);
}

static constexpr int _byteToAnsiColourCubeComponent(uint8_t i)
{
	if (i >= 95)
		return 1 + (i - 95 + 20) / 40;
	else
		return i <= 95 / 2 ? 0 : 1;
}

static_assert(_byteToAnsiColourCubeComponent(0) == 0);
static_assert(_byteToAnsiColourCubeComponent(255) == 5);

static constexpr EColour _toCubeColourFromRGB8(RGB8 rgb)
{
	return _encodeCubeColour(
		_byteToAnsiColourCubeComponent(rgb[0]),
		_byteToAnsiColourCubeComponent(rgb[1]),
		_byteToAnsiColourCubeComponent(rgb[2])
	);
}

static constexpr RGB8 _toRGB8FromCubeColour(EColour colour)
{
	int index = int(colour);
	index -= int(hecktui::EColour::Grey0);
	assert(unsigned(index) < 6 * 6 * 6);
	const int b = index % 6; index /= 6;
	const int g = index % 6; index /= 6;
	const int r = index % 6;
	return {
		_ansiColourCubeComponentToByte(r),
		_ansiColourCubeComponentToByte(g),
		_ansiColourCubeComponentToByte(b)
	};
}

static constexpr RGB8 _toRGB8FromGreyStepsColour(EColour colour)
{
	assert(EColour::Grey3 <= colour && colour <= EColour::Grey93);
	const int index = int(colour) - int(hecktui::EColour::Grey3);
	const uint8_t code = uint8_t(index * 10 + 8);
	return { code, code, code };
}

static constexpr int _colourDistSq(RGB8 a, RGB8 b)
{
	const int x = a[0] - b[0];
	const int y = a[1] - b[1];
	const int z = a[2] - b[2];
	return x * x + y * y + z * z;
}

// As in https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-data-conversion
// [0..1] == [0..255]
static constexpr uint8_t floatToByte(float x)
{
	return uint8_t(std::clamp(x, 0.0f, 1.0f) * 255.0f + 0.5f);
}

static constexpr float byteToFloat(uint8_t x)
{
	return x / 255.0f;
}

static constexpr std::array<float, 256> kSRGBToLinearTable{
	0.0f				 ,
	0.0003035269835488375f,
	0.000607053967097675f,
	0.0009105809506465125f,
	0.00121410793419535f,
	0.0015176349177441874f,
	0.001821161901293025f,
	0.0021246888848418626f,
	0.0024282158683907f,
	0.0027317428519395373f,
	0.003035269835488375f,
	0.003346535763899161f,
	0.003676507324047436f,
	0.004024717018496307f,
	0.004391442037410293f,
	0.004776953480693729f,
	0.005181516702338386f,
	0.005605391624202723f,
	0.006048833022857054f,
	0.006512090792594475f,
	0.006995410187265387f,
	0.007499032043226175f,
	0.008023192985384994f,
	0.008568125618069307f,
	0.009134058702220787f,
	0.00972121732023785f,
	0.010329823029626936f,
	0.010960094006488246f,
	0.011612245179743885f,
	0.012286488356915872f,
	0.012983032342173012f,
	0.013702083047289686f,
	0.014443843596092545f,
	0.01520851442291271f,
	0.01599629336550963f,
	0.016807375752887384f,
	0.017641954488384078f,
	0.018500220128379697f,
	0.019382360956935723f,
	0.0202885630566524f,
	0.021219010376003555f,
	0.02217388479338738f,
	0.02315336617811041f,
	0.024157632448504756f,
	0.02518685962736163f,
	0.026241221894849898f,
	0.027320891639074894f,
	0.028426039504420793f,
	0.0295568344378088f,
	0.030713443732993635f,
	0.03189603307301153f,
	0.033104766570885055f,
	0.03433980680868217f,
	0.03560131487502034f,
	0.03688945040110004f,
	0.0382043715953465f,
	0.03954623527673284f,
	0.04091519690685319f,
	0.042311410620809675f,
	0.043735029256973465f,
	0.04518620438567554f,
	0.046665086336880095f,
	0.04817182422688942f,
	0.04970656598412723f,
	0.05126945837404324f,
	0.052860647023180246f,
	0.05448027644244237f,
	0.05612849004960009f,
	0.05780543019106723f,
	0.0595112381629812f,
	0.06124605423161761f,
	0.06301001765316767f,
	0.06480326669290577f,
	0.06662593864377289f,
	0.06847816984440017f,
	0.07036009569659588f,
	0.07227185068231748f,
	0.07421356838014963f,
	0.07618538148130785f,
	0.07818742180518633f,
	0.08021982031446832f,
	0.0822827071298148f,
	0.08437621154414882f,
	0.08650046203654976f,
	0.08865558628577294f,
	0.09084171118340768f,
	0.09305896284668745f,
	0.0953074666309647f,
	0.09758734714186246f,
	0.09989872824711389f,
	0.10224173308810132f,
	0.10461648409110419f,
	0.10702310297826761f,
	0.10946171077829933f,
	0.1119324278369056f,
	0.11443537382697373f,
	0.11697066775851084f,
	0.11953842798834562f,
	0.12213877222960187f,
	0.12477181756095049f,
	0.12743768043564743f,
	0.1301364766903643f,
	0.13286832155381798f,
	0.13563332965520566f,
	0.13843161503245183f,
	0.14126329114027164f,
	0.14412847085805777f,
	0.14702726649759498f,
	0.14995978981060856f,
	0.15292615199615017f,
	0.1559264637078274f,
	0.1589608350608804f,
	0.162029375639111f,
	0.1651321945016676f,
	0.16826940018969075f,
	0.1714411007328226f,
	0.17464740365558504f,
	0.17788841598362912f,
	0.18116424424986022f,
	0.184474994500441f,
	0.18782077230067787f,
	0.19120168274079138f,
	0.1946178304415758f,
	0.19806931955994886f,
	0.20155625379439707f,
	0.20507873639031693f,
	0.20863687014525575f,
	0.21223075741405523f,
	0.21586050011389926f,
	0.2195261997292692f,
	0.2232279573168085f,
	0.22696587351009836f,
	0.23074004852434915f,
	0.23455058216100522f,
	0.238397573812271f,
	0.24228112246555486f,
	0.24620132670783548f,
	0.25015828472995344f,
	0.25415209433082675f,
	0.2581828529215958f,
	0.26225065752969623f,
	0.26635560480286247f,
	0.2704977910130658f,
	0.27467731206038465f,
	0.2788942634768104f,
	0.2831487404299921f,
	0.2874408377269175f,
	0.29177064981753587f,
	0.2961382707983211f,
	0.3005437944157765f,
	0.3049873140698863f,
	0.30946892281750854f,
	0.31398871337571754f,
	0.31854677812509186f,
	0.32314320911295075f,
	0.3277780980565422f,
	0.33245153634617935f,
	0.33716361504833037f,
	0.3419144249086609f,
	0.3467040563550296f,
	0.35153259950043936f,
	0.3564001441459435f,
	0.3613067797835095f,
	0.3662525955988395f,
	0.3712376804741491f,
	0.3762621229909065f,
	0.38132601143253014f,
	0.386429433787049f,
	0.39157247774972326f,
	0.39675523072562685f,
	0.4019777798321958f,
	0.4072402119017367f,
	0.41254261348390375f,
	0.4178850708481375f,
	0.4232676699860717f,
	0.4286904966139066f,
	0.43415363617474895f,
	0.4396571738409188f,
	0.44520119451622786f,
	0.45078578283822346f,
	0.45641102318040466f,
	0.4620769996544071f,
	0.467783796112159f,
	0.47353149614800955f,
	0.4793201831008268f,
	0.4851499400560704f,
	0.4910208498478356f,
	0.4969329950608704f,
	0.5028864580325687f,
	0.5088813208549338f,
	0.5149176653765214f,
	0.5209955732043543f,
	0.5271151257058131f,
	0.5332764040105052f,
	0.5394794890121072f,
	0.5457244613701866f,
	0.5520114015120001f,
	0.5583403896342679f,
	0.5647115057049292f,
	0.5711248294648731f,
	0.5775804404296506f,
	0.5840784178911641f,
	0.5906188409193369f,
	0.5972017883637634f,
	0.6038273388553378f,
	0.6104955708078648f,
	0.6172065624196511f,
	0.6239603916750761f,
	0.6307571363461468f,
	0.6375968739940326f,
	0.6444796819705821f,
	0.6514056374198242f,
	0.6583748172794485f,
	0.665387298282272f,
	0.6724431569576875f,
	0.6795424696330938f,
	0.6866853124353135f,
	0.6938717612919899f,
	0.7011018919329731f,
	0.7083757798916868f,
	0.7156935005064807f,
	0.7230551289219693f,
	0.7304607400903537f,
	0.7379104087727308f,
	0.7454042095403874f,
	0.7529422167760779f,
	0.7605245046752924f,
	0.768151147247507f,
	0.7758222183174236f,
	0.7835377915261935f,
	0.7912979403326302f,
	0.799102738014409f,
	0.8069522576692516f,
	0.8148465722161012f,
	0.8227857543962835f,
	0.8307698767746546f,
	0.83879901174074f,
	0.846873231509858f,
	0.8549926081242338f,
	0.8631572134541023f,
	0.8713671191987972f,
	0.8796223968878317f,
	0.8879231178819663f,
	0.8962693533742664f,
	0.9046611743911496f,
	0.9130986517934192f,
	0.9215818562772946f,
	0.9301108583754237f,
	0.938685728457888f,
	0.9473065367331999f,
	0.9559733532492861f,
	0.9646862478944651f,
	0.9734452903984125f,
	0.9822505503331171f,
	0.9911020971138298f,
	1.0f,
};

static constexpr uint8_t toSRGBByteFromLinear(float y)
{
	const auto it = std::ranges::upper_bound(kSRGBToLinearTable, y);

	const size_t index = std::clamp<size_t>(it - kSRGBToLinearTable.begin(), 1, 255);

	const float thisValue = *it;
	const float prevValue = it[-1];

	return uint8_t(index - (y * 2 < thisValue + prevValue));

	//if (y <= 0.0031308f)
	//	return 12.92f * y;
	//else
	//	return 1.055f * std::pow(y, 1/2.4f) - 0.055f;
}


constexpr uint8_t _toSRGBGreyByte(RGB8 rgb)
{
	// https://stackoverflow.com/questions/15686277/convert-rgb-to-grayscale-in-c
	const float linearR = kSRGBToLinearTable[rgb[0]];
	const float linearG = kSRGBToLinearTable[rgb[1]];
	const float linearB = kSRGBToLinearTable[rgb[2]];
	const float linearGrey = 0.2126f * linearR + 0.7152f * linearG + 0.0722f * linearB;
	return floatToByte(toSRGBByteFromLinear(linearGrey));
}

constexpr EColour _toNearestGreyStepsColourFromRGB8(RGB8 rgb)
{
	const uint8_t byteGrey = _toSRGBGreyByte(rgb);
	//const uint8_t code = uint8_t(index * 10 + 8);
	// 8, 18, 28, ...
	return (EColour)std::clamp((byteGrey - 8 + 5) / 10, (int)EColour::Grey3, (int)EColour::Grey93);
}



// Convert from system palette to RGB cube or grey-scale.
static constexpr RGB8 kSystemColourRGBTable[]{
	{ 12,12,12    },
	{ 197,15,31   },
	{ 19,161,14   },
	{ 193,156,0   },
	{ 0,55,218    },
	{ 136,23,152  },
	{ 58,150,221  },
	{ 204,204,204 },
	{ 118,118,118 },
	{ 231,72,86   },
	{ 22,198,12   },
	{ 249,241,165 },
	{ 59,120,255  },
	{ 180,0,158   },
	{ 97,214,214  },
	{ 242,242,242 },
};

static constexpr EColour _toNearestColourFromRGB8(RGB8 rgb)
{
	const EColour cubeColour = _toCubeColourFromRGB8(rgb);
	const RGB8 cubeRGB = _toRGB8FromCubeColour(cubeColour);
	if (cubeRGB == rgb)
		return cubeColour;

	const EColour greyColour = _toNearestGreyStepsColourFromRGB8(rgb);
	const RGB8 greyRGB = _toRGB8FromGreyStepsColour(greyColour);
	if (greyRGB == rgb)
		return greyColour;

	const int cubeDist = _colourDistSq(cubeRGB, rgb);
	const int greyDist = _colourDistSq(greyRGB, rgb);
	return cubeDist < greyDist ? cubeColour : greyColour;
}

static constexpr auto kSystemColourConversionTable = [] {
	std::array<EColour, 16> table{};
	for (int i = 0; i < 16; ++i)
		table[i] = _toNearestColourFromRGB8(kSystemColourRGBTable[i]);
	return table;
}();

static constexpr RGB8 _toRGB8(EColour colour)
{
	if (colour <= EColour::White)
		return kSystemColourRGBTable[(int)colour];
	else if (colour <= EColour::Grey3)
		return _toRGB8FromCubeColour(colour);
	else
		return _toRGB8FromGreyStepsColour(colour);
}
/*
// Sys colours, colour cube, and the greyscale steps.
static constexpr auto kGreyscaleColourList = [] {
	std::array<std::pair<EColour, uint8_t>, 4 + 6 + 24> table{};
	table[0] = { kSystemColourTable[0], 0 };
	table[1] = { kSystemColourTable[7], 0 };
	table[2] = { kSystemColourTable[8], 0 };
	table[3] = { kSystemColourTable[15], 0 };
	for (int i = 0; i < 6; ++i)
		table[4+i] = { _encodeCubeColour(i, i, i), 0 };
	for (int i = (int)EColour::Grey3; i <= (int)EColour::Grey93; ++i)
		table[4+6+i] = { EColour(i), 0 };

	for (auto& [colour, shade] : table)
		shade = _toRGB8(colour)[0];

	std::ranges::sort(table, std::less<>(), std::views::values);

	return table;
}();

static constexpr auto kGreyscaleNearestColourTable = [] {
	std::array<EColour, 256> table{};

	int greyscaleOrderedShadesI = 0;

	for (int i = 0; i < 256; ++i)
	{
		while (greyscaleOrderedShadesI + 1 < kGreyscaleColourList.size() && kGreyscaleColourList[greyscaleOrderedShadesI + 1].second <= i)
			++greyscaleOrderedShadesI;

		const int thisShade = kGreyscaleColourList[greyscaleOrderedShadesI].second;
		const int nextShade = kGreyscaleColourList[greyscaleOrderedShadesI + 1].second;
		table[i] = kGreyscaleColourList[greyscaleOrderedShadesI + (i * 2 > thisShade + nextShade)].first;
	}

	return table;
}();*/


EColour hecktui::encodeCubeColour(CubeRGB rgb)
{
	return _encodeCubeColour(rgb[0], rgb[1], rgb[2]);
}

CubeRGB hecktui::decodeCubeColour(EColour colour)
{
	return _decodeCubeColour(colour);
}

/*
EColour hecktui::toColourFromRGB8(uint8_t r, uint8_t g, uint8_t b)
{
	return _toColourFromRGB8(r, g, b);
}*/


EColour hecktui::toNonSysColour(EColour colour)
{
	if (colour <= EColour::White)
		return kSystemColourConversionTable[(int)colour];
	else
		return colour;
}

EColour hecktui::toColourFromRGB8(RGB8 rgb)
{
	return _toNearestColourFromRGB8(rgb);
}
RGB8 hecktui::toRGB8(EColour colour)
{
	return _toRGB8(colour);
}


 EColour hecktui::toColourFromRGBF(RGBF rgb)
{
	 return toColourFromRGB8({
		 floatToByte(rgb[0]),
		 floatToByte(rgb[1]),
		 floatToByte(rgb[2]),
		 });
}

 RGBF hecktui::toRGBF(EColour colour)
{
	const RGB8 rgb8 = _toRGB8(colour);

	return {
		byteToFloat(rgb8[0]),
		byteToFloat(rgb8[1]),
		byteToFloat(rgb8[2]),
	};
}

 RGB8 hecktui::toRGB8(RGBF colour)
 {
	 return {
		floatToByte(colour[0]),
		floatToByte(colour[1]),
		floatToByte(colour[2]),
	 };
 }


 uint32_t hecktui::getDoubleClickMilliseconds() noexcept
 {
#ifdef _WIN32
	 return GetDoubleClickTime();
#else
	 return 270; // My double click speed. How do you get this on Linux...
#endif
 }

#ifdef _MSC_VER
void hecktui::debugLog(std::wstring s)
{
	OutputDebugStringW(s.c_str());
}

void hecktui::checkDebugBreakKey()
{
	if (GetAsyncKeyState(VK_OEM_8) < 0)
		_CrtDbgBreak();
}

int hecktui::getDefaultVerticalScrollAmount() noexcept
{
	static const int nn = [] {
		UINT n = 0;
		if (!SystemParametersInfoA(SPI_GETWHEELSCROLLLINES, 0, &n, 0))
			n = 3;
		return std::max(1, int(n));
	}();
	return nn;
}
#else
int hecktui::getDefaultVerticalScrollAmount() noexcept
{
	// Is it possible to get this from Linux desktop?
	return 3;
}
#endif
