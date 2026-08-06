#include "test_support.hpp"

#include "aether/meta/equiv.hpp"

#include <cstdio>
#include <fstream>

void test_equivalence() {
    constexpr const char* kTest = "equivalence";

    aether::EquivReport rep = aether::run_equivalence_campaign(0xAE7EC0DEu, 6);

    std::fputs(aether::format_equiv_report(rep).c_str(), stdout);

    // JSON artifact for stranger reproduce / SCORECARD
    {
        std::ofstream out("artifacts/equiv_report.json");
        if (out) {
            out << aether::format_equiv_report_json(rep);
        } else {
            // build/ cwd — try parent
            std::ofstream out2("../artifacts/equiv_report.json");
            if (out2)
                out2 << aether::format_equiv_report_json(rep);
        }
    }

    TEST_CHECK(kTest, rep.seeds >= 6);
    TEST_CHECK(kTest, rep.trials >= 100);
    TEST_CHECK(kTest, rep.breaks == 0);
    TEST_CHECK(kTest, rep.real_breaks == 0);
    TEST_CHECK(kTest, rep.cascade_breaks == 0);
    TEST_CHECK(kTest, rep.native_breaks == 0);
    TEST_CHECK(kTest, rep.real_text_ok);
    TEST_CHECK(kTest, rep.real_text_windows >= 1);
    TEST_CHECK(kTest, rep.cascade_trials >= 8);
    TEST_CHECK(kTest, rep.pass());
    TEST_CHECK(kTest, rep.unique_hashes >= 40);
    TEST_CHECK(kTest, rep.paths_exercised >= 10);
    TEST_CHECK(kTest, rep.real_trials >= 20);
}
