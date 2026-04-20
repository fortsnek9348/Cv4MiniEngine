#include "inc/CommonStuff/div.h"

using namespace heck;

static_assert(fdiv(10, 10u) == 1);
static_assert(fdiv(5, 10u) == 0);
static_assert(fdiv(4, 10u) == 0);
static_assert(fdiv(-4, 10u) == -1);
static_assert(fdiv(-5, 10u) == -1);
static_assert(fdiv(-6, 10u) == -1);
static_assert(fdiv(-10, 10u) == -1);
static_assert(fdiv(-11, 10u) == -2);

static_assert(cdiv(10, 10u) == 1);
static_assert(cdiv(5, 10u) == 1);
static_assert(cdiv(4, 10u) == 1);
static_assert(cdiv(-4, 10u) == 0);
static_assert(cdiv(-5, 10u) == 0);
static_assert(cdiv(-6, 10u) == 0);
static_assert(cdiv(-10, 10u) == -1);
static_assert(cdiv(-11, 10u) == -1);

static_assert(rdiv(5, 10u) == 1);
static_assert(rdiv(4, 10u) == 0);
static_assert(rdiv(-4, 10u) == 0);
static_assert(rdiv(-5, 10u) == -1);
static_assert(rdiv(-6, 10u) == -1);