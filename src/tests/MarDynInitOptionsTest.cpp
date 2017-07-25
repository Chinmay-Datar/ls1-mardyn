/*
 * MarDynTest.cpp
 *
 * @Date: 12.07.2010
 * @Author: eckhardw
 */

#include "tests/MarDynInitOptionsTest.h"
#include "utils/OptionParser.h"

#include <string>

using namespace std;

TEST_SUITE_REGISTRATION(MarDynInitOptionsTest);

// declaration of the function to be tested in Mardyn.cpp
optparse::Values& initOptions(optparse::OptionParser *op);


MarDynInitOptionsTest::MarDynInitOptionsTest() {
}

MarDynInitOptionsTest::~MarDynInitOptionsTest() {
}

void MarDynInitOptionsTest::testAllOptions() {
	const char* const argv[] = { "DummyProgrammeName", "-n", "4", "-t", "-v",
	    //"--phasespace-file", "phasespace.inp",
	    //"--particle-container", "LinkedCells",
	    //"--domain-decomposition", "KDDecomposition",
	    "test_prefix" };

	int argc = sizeof(argv) / sizeof(char*);
	optparse::OptionParser op;
	initOptions(&op);
	optparse::Values options = op.parse_args(argc, argv);

	int n = options.get("timesteps");
	ASSERT_TRUE_MSG("timesteps isSetByUser must be true!", options.is_set_by_user("timesteps"));
	ASSERT_EQUAL(4, n);

	bool test(options.is_set_by_user("tests"));
	ASSERT_TRUE_MSG("test option must be set", test);

//	string prefix(options.get("outputprefix"));
//	ASSERT_TRUE_MSG("outputprefix isSetByUser must be true!", options.is_set_by_user("outputprefix"));
//	ASSERT_EQUAL(string("test_prefix"), prefix);

//	bool verbose = options.get("verbose");
	bool verbose = options.is_set_by_user("verbose");
	ASSERT_TRUE_MSG("verbose option must be set", verbose);

	// TODO: how to query other commandline arguments!?

}

