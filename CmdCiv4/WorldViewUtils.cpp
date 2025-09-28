#include "WorldViewUtils.h"

#include <cmath>

namespace
{
	//constexpr double gLightZ = -1.5;
	//constexpr double gPow = 0.8;
	//constexpr double gStepGrad = 2.12;
	//constexpr double gStepP = 0.73;
	//constexpr double gStepQ = 0.49;

	// lighting:-1.375 1.04 step:2.12 0.74 0.46 (gradScale=0.25)
	// lighting:-1.625 1.76 step:2.48 0.16 0.24 gradScale:0.36 
	constexpr double gLightZ = -1.25;
	constexpr double gPow = 1.4;
	constexpr double gStepGrad = 2.4;
	constexpr double gStepP = 0.18;
	constexpr double gStepQ = 0.21;
	constexpr double gGradiantFudgeScale = 0.34;

	//double sq(double x)
	//{
	//	return x * x;
	//}

	struct AutoDiff
	{
		double x = 0;
		double dx = 0;

		AutoDiff(double c, double dx = 0) : x(c), dx(dx)
		{
		}

		friend AutoDiff sqrt(AutoDiff b)
		{
			return { std::sqrt(b.x), b.dx/(2*std::sqrt(b.x)) };
		}

		friend AutoDiff operator-(AutoDiff b)
		{
			return { -b.x, -b.dx };
		}

		friend AutoDiff operator+(AutoDiff a, AutoDiff b)
		{
			return { a.x + b.x, a.dx + b.dx };
		}

		friend AutoDiff operator-(AutoDiff a, AutoDiff b)
		{
			return a + -b;
		}

		friend AutoDiff operator*(AutoDiff a, AutoDiff b)
		{
			return { a.x * b.x, a.x * b.dx + b.x * a.dx };
		}

		friend AutoDiff operator/(AutoDiff a, AutoDiff b)
		{
			return { a.x / b.x, (b.x * a.dx - a.x * b.dx) / (b.x * b.x) };
		}
	};

	// http://www.peterstock.co.uk/games/adjustable_smoothstep/
	static AutoDiff smootherstep(AutoDiff x)
	{
		//return 30*(x-1)*(x-1)*x*x;
		const double g = gStepGrad;
		const double p = gStepP;
		const double q = std::clamp(gStepQ, (p - 1) * g + 1, p * g);

		const auto c = [](double g) {
			return (g - 1) / (2 - g);
		};

		const auto f = [](AutoDiff x, double p, double c) {
			return x * ((x + x * c) / (x + p * c));
		};

		if (x.x < 0)
			return 0;
		else if (x.x > 1)
			return 1;
		else if (x.x <= p)
			return (q/p) * f(x, p, c((p/q) * g));
		else
			return  1 - ((1-q)/(1-p)) * f(1-x, 1-p, c(((1-p)/(1-q)) * g));
	}

	template<class T>
	struct vec2
	{
		T x = 0;
		T y = 0;

		vec2() = default;
		vec2(T x, T y) : x(x), y(y)
		{
		}
		vec2(T s) : vec2(s, s)
		{
		}

		T lengthSq() const
		{
			return x * x + y * y;
		}

		T length() const
		{
			using std::sqrt;
			return sqrt(lengthSq());
		}

		vec2 operator-(vec2 b) const
		{
			return { x - b.x, y - b.y };
		}

		vec2 operator+(vec2 b) const
		{
			return { x + b.x, y + b.y };
		}

		vec2 operator*(vec2 b) const
		{
			return { x * b.x, y * b.y };
		}

		vec2 operator/(vec2 b) const
		{
			return { x / b.x, y / b.y };
		}

		vec2 normalised() const
		{
			return *this / length();
		}
	};

	struct Hill
	{
		double x = 0;
		double y = 0;
		double z = 0;
		double rx = 0;
		double ry = 0;
	};

	constexpr Hill kMountainHills[]{
		{ 0.5, 0.5, 1.0, 1.0, 1.0 },
		{ 0.3, 0.7, 0.9, 0.6, 0.65 },
		//{ 0.3, 0.8, 0.3, 0.3, 0.35 },
	};

	constexpr Hill kNormalHills[]{
		{ 0.5, 0.5, 1.0, 1.0, 1.0 },
	};

	static AutoDiff evalHeight(AutoDiff x, AutoDiff y, std::span<const Hill> hills)
	{
		AutoDiff peak{ 0 };

		for (const Hill& hill : hills)
		{
			const vec2<AutoDiff> center(hill.x, hill.y);
			const vec2<AutoDiff> rel = vec2{ x, y } - center;
			AutoDiff r = ((vec2<AutoDiff>(1, 1).normalised() * rel.x + vec2<AutoDiff>(1, -1).normalised() * rel.y) / vec2<AutoDiff>(hill.rx, hill.ry)).length();
			if (r.x < 0.001)
				r.dx = 0;
			//const AutoDiff r = rel.length();
			const AutoDiff z = smootherstep(1 - r * 2) * hill.z;
			if (z.x > peak.x)
				peak = z;
		}
		return peak;
	}

	using dvec2 = vec2<double>;

	struct dvec3
	{
		double x = 0;
		double y = 0;
		double z = 0;

		dvec3() = default;
		dvec3(dvec2 v, double z) : dvec3(v.x, v.y, z)
		{
		}
		dvec3(double x, double y, double z) : x(x), y(y), z(z)
		{
		}
		dvec3(double s) : dvec3(s, s, s)
		{
		}

		friend double dot(dvec3 a, dvec3 b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		friend dvec3 cross(dvec3 a, dvec3 b)
		{
			return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
		}

		double lengthSq() const
		{
			return dot(*this, *this);
		}

		double length() const
		{
			return std::sqrt(lengthSq());
		}

		dvec3 operator/(dvec3 b) const
		{
			return { x / b.x, y / b.y, z / b.z };
		}

		dvec3 normalised() const
		{
			return *this / length();
		}
	};

	static dvec3 computeNormal(dvec2 p, std::span<const Hill> hills)
	{
		//const dvec2 center(0.5);
		//const dvec2 rel = p - center;
		//const double r = rel.length();
		//const dvec2 unitVec = rel.normalised();

		const AutoDiff heightGradX = evalHeight({ p.x, 1.0 }, { p.y, 0.0 }, hills) * gGradiantFudgeScale;
		const AutoDiff heightGradY = evalHeight({ p.x, 0.0 }, { p.y, 1.0 }, hills) * gGradiantFudgeScale;
		const dvec3 normal3D = cross(dvec3(1, 0, heightGradX.dx), dvec3(0, 1, heightGradY.dx));

		//const dvec2 normal2D = smootherstepNormal(std::min(1.0, r * 2));
		//
		//const dvec2 normal3Dxy = unitVec * -normal2D.x;
		//const double normal3Dz = normal2D.y;
		//
		//return dvec3(normal3Dxy, normal3Dz).normalised();
		return normal3D.normalised();
	}

	// Diameter of 1.0.
	double evalShading(double x, double y, std::span<const Hill> hills)
	{
		const dvec3 N = computeNormal({ x, y }, hills);
		const dvec3 lightDir = dvec3(1, 1, gLightZ).normalised();
		//return N.x;
		return std::pow(std::max(0.0, -dot(N, lightDir)), 1 / gPow);
	}

	constexpr std::array kDithers{ L' ', L'░', L'▒', L'▓' };
}



WorldViewFramebuffer WorldViewUtils::renderHill(heck::ivec2 dim, std::array<EColour, 4> colours)
{
	constexpr int kTotalShades = int((colours.size() - 1) * kDithers.size()) + 1;

	WorldViewFramebuffer fb(dim);

	for (int y = 0; y < dim.y; ++y)
	{
		for (int x = 0; x < dim.x; ++x)
		{
			const dvec2 coord{ (x + 0.5) / double(dim.x), (y + 0.5) / double(dim.y) };
			const double shade = evalShading(coord.x, coord.y, kNormalHills);

			const int shadeInt = std::min<int>(std::lround(shade * kTotalShades), kTotalShades - 1);

			WorldViewFramebuffer::Cell cell{
				.c = kDithers[shadeInt % 4],
				.textColour = colours[std::min(shadeInt / kDithers.size() + 1, colours.size() - 1)],
				.backColour = colours[shadeInt / kDithers.size()],
			};

			if (cell.c == kDithers.back())
			{
				// Flip the colours so that the background colour takes up most pixels.
				cell.c = kDithers[1];
				std::swap(cell.textColour, cell.backColour);
			}

			fb.drawCell(heck::ivec2{ x, y }, cell);
		}
	}

	return fb;
}

WorldViewFramebuffer WorldViewUtils::renderPeak(heck::ivec2 dim, std::array<EColour, 4> baseColours, std::array<EColour, 4> snowColours)
{
	constexpr int kTotalShades = int((baseColours.size() - 1) * kDithers.size()) + 1;

	WorldViewFramebuffer fb(dim);

	for (int y = 0; y < dim.y; ++y)
	{
		for (int x = 0; x < dim.x; ++x)
		{
			const dvec2 coord{ (x + 0.5) / double(dim.x), (y + 0.5) / double(dim.y) };
			const double shade = evalShading(coord.x, coord.y, kMountainHills);

			const int shadeInt = std::min<int>(std::lround(shade * kTotalShades), kTotalShades - 1);

			const bool isPeak = ((coord - 0.5) * dvec2(1, 1)).length() < 0.2;
			//const bool isPeak = false;
			const auto& thisColours = isPeak ? snowColours : baseColours;

			WorldViewFramebuffer::Cell cell{
				.c = kDithers[shadeInt % 4],
				.textColour = thisColours[std::min(shadeInt / kDithers.size() + 1, thisColours.size() - 1)],
				.backColour = thisColours[shadeInt / kDithers.size()],
			};

			if (cell.c == kDithers.back())
			{
				// Flip the colours so that the background colour takes up most pixels.
				cell.c = kDithers[1];
				std::swap(cell.textColour, cell.backColour);
			}

			fb.drawCell(heck::ivec2{ x, y }, cell);
		}
	}

	return fb;
}
