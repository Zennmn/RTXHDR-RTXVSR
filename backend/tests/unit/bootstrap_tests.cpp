#include "core/result.h"

#include <gtest/gtest.h>

TEST(Bootstrap, resultCarriesValue) {
    const auto result = vsr::Result<int>::Ok(42);

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), 42);
}

TEST(Bootstrap, resultCarriesError) {
    const auto result = vsr::Result<void>::Fail({"bootstrap_error", "Bootstrap error.", ""});

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().code, "bootstrap_error");
}
