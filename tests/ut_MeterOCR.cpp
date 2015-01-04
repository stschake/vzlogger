/*
 * unit tests for MeterOCR.cpp
 * Author: Matthias Behr, 2015
 */

#include "gtest/gtest.h"
#include "json/json.h"
#include "protocols/MeterOCR.hpp"

// this is a dirty hack. we should think about better ways/rules to link against the
// test objects.
// e.g. single files in the tests directory that just include the real file. This way one
// will avoid multiple includes of the same file in a different test case
// (or linking against the real file from the CMakeLists.txt)

#include "../src/protocols/MeterOCR.cpp"

TEST(MeterOCR, basic_prepared)
{
	std::list<Option> options;
	options.push_back(Option("file", (char*)"tests/meterOCR/sensus_test1_nronly2.png"));
	ASSERT_THROW( MeterOCR m2(options), vz::VZException); // missing boundingboxes parameter
	struct json_object *jsa = json_tokener_parse("[{\"identifier\": \"id1\"}]");
	ASSERT_TRUE(jsa!=0);
	options.push_back(Option("boundingboxes", jsa));
	json_object_put(jsa);
	MeterOCR m(options);

	ASSERT_EQ(SUCCESS, m.open());

	std::vector<Reading> rds;
	rds.resize(1);
	EXPECT_EQ(1, m.read(rds, 1));

	double value = rds[0].value();
	EXPECT_EQ(432, value);

	ASSERT_EQ(0, m.close());

}

TEST(MeterOCR, basic_scaler)
{
	std::list<Option> options;
	options.push_back(Option("file", (char*)"tests/meterOCR/sensus_test1_nronly2.png"));
	ASSERT_THROW( MeterOCR m2(options), vz::VZException); // missing boundingboxes parameter
	struct json_object *jsa = json_tokener_parse("[{\"identifier\": \"id1\", \"scaler\": -2}]");
	ASSERT_TRUE(jsa!=0);
	options.push_back(Option("boundingboxes", jsa));
	json_object_put(jsa);
	MeterOCR m(options);

	ASSERT_EQ(SUCCESS, m.open());

	std::vector<Reading> rds;
	rds.resize(1);
	EXPECT_EQ(1, m.read(rds, 1));

	double value = rds[0].value();
	EXPECT_EQ(4.32, value);

	ASSERT_EQ(0, m.close());

}

TEST(MeterOCR, basic_two_boxes_same_id)
{
	std::list<Option> options;
	options.push_back(Option("file", (char*)"tests/meterOCR/sensus_test1_nronly2.png"));
	ASSERT_THROW( MeterOCR m2(options), vz::VZException); // missing boundingboxes parameter
	struct json_object *jsa = json_tokener_parse("[{\"identifier\": \"id1\"},{\"identifier\": \"id1\"}]");
	ASSERT_TRUE(jsa!=0);
	options.push_back(Option("boundingboxes", jsa));
	json_object_put(jsa);
	MeterOCR m(options);

	ASSERT_EQ(SUCCESS, m.open());

	std::vector<Reading> rds;
	rds.resize(1);
	EXPECT_EQ(1, m.read(rds, 1));

	double value = rds[0].value();
	EXPECT_EQ(2.0*432, value);

	ASSERT_EQ(0, m.close());

}

TEST(MeterOCR, basic_not_prepared)
{
	std::list<Option> options;
	options.push_back(Option("file", (char*)"tests/meterOCR/sensus_test1_nronly1.png"));
	options.push_back(Option("rotate", -2.0)); // rotate by -2deg (counterclockwise)
	struct json_object *jso = json_tokener_parse("[{\"identifier\": \"water cons\", \"box\": {\"x1\": 42, \"x2\": 395, \"y1\": 10, \"y2\": 65}}]"); // should detect 00432m
	options.push_back(Option("boundingboxes", jso));
	json_object_put(jso);
	MeterOCR m(options);

	ASSERT_EQ(SUCCESS, m.open());

	std::vector<Reading> rds;
	rds.resize(1);
	EXPECT_EQ(1, m.read(rds, 1));

	double value = rds[0].value();
	EXPECT_EQ(432, value);

	ASSERT_EQ(0, m.close());
}

