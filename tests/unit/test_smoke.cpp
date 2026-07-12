#include "harness.hpp"
#include "src/types.hpp"
TEST(smoke_types_compile) {
    droid::LoadResult r;
    CHECK(!r.ok);
    CHECK(r.errors.empty());
}
