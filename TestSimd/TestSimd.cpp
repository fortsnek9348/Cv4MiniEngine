#include <CommonStuff\Simd.h>
#include "GenericVector.h"

#include <random>
#include <iostream>

using namespace heck::simd;

static std::mt19937_64 gRNG{ 43969 };

struct TagRandomValuesVector {};
struct TagRandomShiftVector {};

template<class Element>
static Element makeLowNBits(size_t n)
{
	if (n >= sizeof(Element) * CHAR_BIT)
		return Element(-1);
	else
		return Element(Element(1) << n);
}

template<class...>
struct TypeTuple {};

using TestElementTypes = TypeTuple<
	int8_t,
	int16_t,
	int32_t,
	uint8_t,
	uint16_t,
	uint32_t
>;
template<class To, class T, size_t k, class Reg>
Vector<To, k> castVector(Vector<T, k, Reg> v)
{
	return Vector<To, k>(v);
}

template<class To, class T, size_t k>
GenericVector<To, k> castVector(GenericVector<T, k> v)
{
	return GenericVector<To, k>(v);
}

template<class Element, size_t kNumElements, class Reg>
struct VectorTester
{
	static constexpr size_t kBitsPerElement = sizeof(Element) * CHAR_BIT;

	using SimdVector = Vector<Element, kNumElements, Reg>;
	using ScalarVector = GenericVector<Element, kNumElements>;

	static void checkEq(auto f, auto... args)
	{
		const auto a = f(SimdVector(args)...);
		const auto b = f(ScalarVector(args)...);
		const auto arrA = a.toArray();
		const auto arrB = b.toArray();
		if (arrA != arrB)
		{
			const size_t mismatchI = std::ranges::mismatch(arrA, arrB).in1 - arrA.begin();
			std::cout << "Mismatch at index " << mismatchI << std::endl;
			std::cout << "a[i] = " << +arrA[mismatchI] << std::endl;
			std::cout << "b[i] = " << +arrB[mismatchI] << std::endl;
			(void)f(SimdVector(args)...);
			std::abort();
		}
	}

	static void checkMaskEq(auto f, auto... args)
	{
		const auto a = f(SimdVector(args)...);
		const auto b = f(ScalarVector(args)...);
		const auto arrA = a.asBitsUInt();
		const auto arrB = b.to_ullong();
		if (arrA != arrB)
		{
			//const size_t mismatchI = std::ranges::mismatch(arrA, arrB).in1 - arrA.begin();
			//std::cout << "Mismatch at index " << mismatchI << std::endl;
			//std::cout << "a[i] = " << +arrA[mismatchI] << std::endl;
			//std::cout << "b[i] = " << +arrB[mismatchI] << std::endl;
			(void)f(SimdVector(args)...);
			//(void)a.asBitset();
			std::abort();
		}
	}


	void testVectorType()
	{
		checkEq([](auto a, auto b) { return a + b; }, genRandomArray(), genRandomArray());
		checkEq([](auto a, auto b) { return a - b; }, genRandomArray(), genRandomArray());
		checkEq([](auto a, auto b) { return a * b; }, genRandomArray(), genRandomArray());
		if constexpr (sizeof(Element) >= 2) // Not implemented for bytes.
			checkEq([](auto a, auto b) { return vmulhi(a, b); }, genRandomArray(), genRandomArray());

		// Shifting not implemented for bytes.
		if constexpr (sizeof(Element) >= 2)
		{
			[this] <size_t... k>(std::index_sequence<k...>) {
				(checkEq([](auto a) { return a >> imm<k>; }, genRandomArray()), ...);
				(checkEq([](auto a) { return a << imm<k>; }, genRandomArray()), ...);
				((/*std::cout << "Shift div test " << k << std::endl,*/ checkEq([](auto a) { return a / imm<std::make_unsigned_t<Element>(1) << k>; }, genRandomArray())), ...);
			}(std::make_index_sequence<kBitsPerElement>());
			checkEq([](auto a, auto b) { return a >> b; }, genRandomArray(), genRandomShifts());
			checkEq([](auto a, auto b) { return a << b; }, genRandomArray(), genRandomShifts());
		}

		if constexpr (sizeof(Element) >= 4)
		{
			const auto dividend = genRandomArray();
			checkEq([](auto a, auto b) { return vdiv(a, b); }, dividend, genRandomDivisor(dividend));
			// Extreme cases.
			checkEq([](auto a, auto b) { return vdiv(a, b); }, genSpecificArray(0x7FFFFF), genSpecificArray(1 << 23));
			checkEq([](auto a, auto b) { return vdiv(a, b); }, genSpecificArray(0x7FFF'FFFF), genSpecificArray(Element(1) << 31));
			if constexpr (std::is_unsigned_v<Element>)
				checkEq([](auto a, auto b) { return vdiv(a, b); }, genSpecificArray(0xFFFF'FFFF), genSpecificArray(Element(1) << 31));
		}

		checkEq([](auto a) {
			for (size_t i = 0; i < kNumElements; ++i)
				a.setElement(i, a.getElement(i) + Element(i));
			return a;
			}, genRandomArray());

		checkEq([](auto a) {
			[&a] <size_t... k>(std::index_sequence<k...>) {
				(a.template setElement<k>(a.template getElement<k>() + Element(k)), ...);
			}(std::make_index_sequence<kNumElements>());
			return a;
			}, genRandomArray());

		[this] <class... T>(TypeTuple<T...>) {
			(this->testConversionTo<T>(), ...);
		}(TestElementTypes());

		std::vector<Element> dataSource(1000);
		if constexpr (sizeof(Element) == 4)
		{
			std::ranges::generate(dataSource, [dist = std::uniform_int_distribution<Element>()]() mutable { return dist(gRNG); });
			checkEq([&]<class V>(V a) { return V(dataSource, a); }, genRandomArrayForN(dataSource.size()));
		}

		if constexpr (kNumElements <= 64) // Giant masks not implemented.
		{
			checkMaskEq([](auto a, auto b) { return vcmplt(a, b); }, genRandomArray(), genRandomArray());
			checkMaskEq([](auto a, auto b) { return vcmple(a, b); }, genRandomArray(), genRandomArray());
			checkMaskEq([](auto a, auto b) { return vcmpgt(a, b); }, genRandomArray(), genRandomArray());
			checkMaskEq([](auto a, auto b) { return vcmpge(a, b); }, genRandomArray(), genRandomArray());
			checkMaskEq([](auto a, auto b) { return vcmpeq(a, b); }, genRandomArray(), genRandomArray());
			checkMaskEq([](auto a, auto b) { return ~vcmpeq(a, b); }, genRandomArray(), genRandomArray());
			checkMaskEq([](auto a, auto b) { return vtest(a, b); }, genRandomArray(), genRandomArray());

			if constexpr (sizeof(Element) == 4)
			{
				const std::bitset<kNumElements> mask = genRandomMask();
				checkEq([&]<class V>(V a) { return V(dataSource, a, typename V::Mask(mask)); }, genRandomArrayForN(dataSource.size()));
			}
		}

		checkEq([](auto a, auto b) { return a.permute(b); }, genUniformRandomArray(), genUniformRandomArray());
	}

	std::array<Element, kNumElements> genSpecificArray(Element value)
	{
		std::array<Element, kNumElements> v{};
		v.fill(value);
		return v;
	}

	std::array<Element, kNumElements> genRandomArrayForN(size_t n)
	{
		--n;
		std::uniform_int_distribution<Element> dist(0, std::in_range<Element>(n) ? Element(n) : std::numeric_limits<Element>::max());
		std::array<Element, kNumElements> v{};
		std::ranges::generate(v, [&] { return dist(gRNG); });
		return v;
	}

	std::array<Element, kNumElements> genUniformRandomArray()
	{
		std::uniform_int_distribution<uintmax_t> bitsDist{};
		std::array<Element, kNumElements> v{};
		std::ranges::generate(v, [&] {
			return Element(bitsDist(gRNG));
			});
		return v;
	}

	std::array<Element, kNumElements> genRandomArray()
	{
		// bits = [min..max]
		// length = [0..k-1], where k = num bits per element
		// type = { any number, pow2, pow2MinusOne, pow2PlusOne }

		std::uniform_int_distribution<uintmax_t> bitsDist{};
		std::uniform_int_distribution<size_t> lenDist{ 0, kBitsPerElement };
		std::discrete_distribution<size_t> typeDist({ 100, 30, 30, 30 });
		
		std::array<Element, kNumElements> v{};
		std::ranges::generate(v, [&] {
			const uintmax_t bits = bitsDist(gRNG);
			const size_t bitWidth = lenDist(gRNG);
			const size_t type = typeDist(gRNG);

			switch (type)
			{
			case 0:
			default:
				return Element(bits & makeLowNBits<Element>(bitWidth));
			case 1:
				return Element(makeLowNBits<Element>(bitWidth) + 1);
			case 2:
				return Element(makeLowNBits<Element>(bitWidth));
			case 3:
				return Element(makeLowNBits<Element>(bitWidth) + 2);
			}

			});

		return v;
	}

	std::array<Element, kNumElements> genRandomShifts()
	{
		return (ScalarVector(genRandomArray()) & ScalarVector(kBitsPerElement - 1)).toArray();
	}

	std::array<Element, kNumElements> genRandomDivisor(std::array<Element, kNumElements> dividend)
	{
		std::array<Element, kNumElements> divisor = genRandomArray();
		for (size_t i = 0; i < kNumElements; ++i)
		{
			if (divisor[i] == 0)
				divisor[i] = Element(-1); // Avoid div by zero. And try a more interesting value than +1.
			if constexpr (std::is_signed_v<Element>)
				if (dividend[i] == std::numeric_limits<Element>::min() && divisor[i] == -1) // Avoid signed div overflow.
					divisor[i] = 1;
		}
		return divisor;
	}

	std::bitset<kNumElements> genRandomMask() const
	{
		std::bitset<kNumElements> m{};
		for (size_t i = 0; i < kNumElements; ++i)
			m[i] = (bool)std::uniform_int_distribution(0, 1)(gRNG);
		return m;
	}

	template<class ElementTo>
	void testConversionTo()
	{
		if constexpr (sizeof(ElementTo) * kNumElements * 8 >= 128) // Partial registers not supported.
		{
			checkEq([](auto a) { return castVector<ElementTo>(a); }, genRandomArray());
		}
	}
};

template<class Element, class Reg>
static void testRegElementType()
{
	static constexpr size_t kElementsPerReg = sizeof(Reg) / sizeof(Element);
	VectorTester<Element, kElementsPerReg, Reg>().testVectorType();
	VectorTester<Element, kElementsPerReg * 2, Reg>().testVectorType();
	VectorTester<Element, kElementsPerReg * 4, Reg>().testVectorType();
}

template<class Reg>
static void testReg()
{
	[] <class... T>(TypeTuple<T...>) {
		(testRegElementType<T, Reg>(), ...);
	}(TestElementTypes());
}


int main()
{
	for (int i = 0; i < 100; ++i)
	{
		testReg<Avx128IntegerReg>();
		testReg<Avx256IntegerReg>();

		// Clang is more strict.
#ifdef __clang__
		if constexpr (heck::simd::kEnableAVX512)
#endif
		testReg<Avx512IntegerReg>();
	}

	return 0;
}
