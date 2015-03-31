#include <catch.hpp>
#include <rapidcheck-catch.h>

#include "rapidcheck/newgen/Transform.h"
#include "rapidcheck/newgen/Create.h"

#include "util/GenUtils.h"
#include "util/Predictable.h"
#include "util/Generators.h"
#include "util/ShrinkableUtils.h"

using namespace rc;
using namespace rc::test;

TEST_CASE("newgen::map") {
    prop("maps the shrinkable returned by the generator",
         [](const Shrinkable<int> &shrinkable) {
             const auto mapper = [](int x) { return x * x; };
             const auto mapped = newgen::map(
                 mapper,
                 Gen<int>(fn::constant(shrinkable)))(Random(), 0);
             RC_ASSERT(shrinkable::map(mapper, shrinkable) == mapped);
         });

    prop("forwards the parameter to the generator",
         [](const GenParams &params) {
             const auto gen = newgen::map(
                 [](GenParams &&x) { return std::move(x); },
                 genPassedParams());
             RC_ASSERT(gen(params.random, params.size).value() == params);
         });

    SECTION("works with non-copyable types") {
        const auto value = newgen::map(
            [](NonCopyable &&nc) { return std::move(nc); },
            newgen::arbitrary<NonCopyable>())(Random(), 0).value();
        REQUIRE(isArbitraryPredictable(value));
    }

    prop("uses newgen::arbitrary if no generator is specified",
         [](const GenParams &params) {
             const auto value = newgen::map<Predictable>(
                 [](Predictable &&x) { return std::move(x); })(
                     params.random, params.size).value();
             RC_ASSERT(isArbitraryPredictable(value));
         });

    prop("finds minimum where string represtation of unsigned integer must be"
         " longer than some value",
         [](const Random &random) {
             const auto gen = newgen::map(
                 [](unsigned int x) { return std::to_string(x); },
                 newgen::arbitrary<unsigned int>());
             const auto n = *gen::ranged(2, 7);
             std::string expected(n, '0');
             expected[0] = '1';
             const auto result = searchGen(
                 random, gen::kNominalSize, gen,
                 [=](const std::string &x) { return x.size() >= n; });
             RC_ASSERT(result == expected);
         });
}

TEST_CASE("newgen::cast") {
    prop("casting to a larger type and then back yields original",
         [](const Shrinkable<uint8_t> &shrinkable) {
             const Gen<uint8_t> gen(fn::constant(shrinkable));
             const auto cast = newgen::cast<uint8_t>(newgen::cast<int>(gen));
             RC_ASSERT(cast(Random(), 0) == shrinkable);
         });
}

TEST_CASE("newgen::suchThat") {
    prop("generated value always matches predicate",
         [](const GenParams &params) {
             const auto gen = newgen::suchThat(
                 [](int x) { return (x % 2) == 0; },
                 newgen::arbitrary<int>());
             const auto shrinkable = gen(params.random, params.size);
             onAnyPath(
                 shrinkable,
                 [](const Shrinkable<int> &value,
                    const Shrinkable<int> &shrink) {
                     RC_ASSERT((value.value() % 2) == 0);
                 });
         });

    prop("if predicate returns true for every value, returned shrinkable is"
         " unchanged",
         [](const GenParams &params, const Shrinkable<int> &shrinkable) {
             const Gen<int> underlying(fn::constant(shrinkable));
             const auto gen = newgen::suchThat(fn::constant(true), underlying);
             RC_ASSERT(underlying(params.random, params.size) ==
                       gen(params.random, params.size));
         });

    prop("throws GenerationFailure if value cannot be generated",
         [](const GenParams &params) {
             const auto gen = newgen::suchThat(fn::constant(false),
                                               newgen::just<int>(0));
             try {
                 gen(params.random, params.size);
             } catch (const GenerationFailure &e) {
                 RC_SUCCEED("Threw GenerationFailure");
             }
             RC_FAIL("Didn't throw GenerationFailure");
         });

    prop("passes the passed size to the underlying generator on the first try",
         [] {
             const auto size = *gen::ranged<int>(0, 2000);
             const auto gen = newgen::suchThat(fn::constant(true),
                                               genPassedParams());
             const auto params = gen(Random(), size).value();
             RC_ASSERT(params.size == size);
         });

    prop("uses newgen::arbitrary if no generator is specified",
         [](const GenParams &params) {
             const auto value = newgen::suchThat<Predictable>(
                 fn::constant(true))(params.random, params.size).value();
             RC_ASSERT(isArbitraryPredictable(value));
         });

    SECTION("works with non-copyable types") {
        const auto value = newgen::suchThat(
            fn::constant(true),
            newgen::arbitrary<Predictable>())(Random(), 0).value();
        REQUIRE(isArbitraryPredictable(value));
    }
}
