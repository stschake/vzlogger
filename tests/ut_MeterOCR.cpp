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
/* the better we make the image preparation the more some edges get detected as digits
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
*/
TEST(MeterOCR, basic_not_prepared_digits)
{
	std::list<Option> options;
	options.push_back(Option("file", (char*)"tests/meterOCR/sensus_test1_nronly1.png"));
	options.push_back(Option("rotate", -2.0)); // rotate by -2deg (counterclockwise)
	struct json_object *jso = json_tokener_parse("[\
	{\"identifier\": \"water cons\", \"scaler\":4,\"digit\":true, \"box\": {\"x1\": 41, \"x2\": 75, \"y1\": 20, \"y2\": 66}},\
	{\"identifier\": \"water cons\", \"scaler\":3,\"digit\":true, \"box\": {\"x1\": 104, \"x2\": 136, \"y1\": 20, \"y2\": 66}},\
	{\"identifier\": \"water cons\", \"scaler\":2,\"digit\":true, \"box\": {\"x1\": 166, \"x2\": 195, \"y1\": 20, \"y2\": 66}},\
	{\"identifier\": \"water cons\", \"scaler\":1,\"digit\":true, \"box\": {\"x1\": 229, \"x2\": 264, \"y1\": 20, \"y2\": 66}},\
	{\"identifier\": \"water cons\", \"scaler\":0,\"digit\":true, \"box\": {\"x1\": 290, \"x2\": 325, \"y1\": 20, \"y2\": 66}}\
	]"); // should detect 00432
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

TEST(MeterOCR, emh_test2_not_prepared)
{
	std::list<Option> options;
	options.push_back(Option("file", (char*)"tests/meterOCR/emh_test2.png"));
	options.push_back(Option("gamma", 0.8));
	options.push_back(Option("gamma_min", 50));
	options.push_back(Option("gamma_max", 180));
	//options.push_back(Option("rotate", -2.0)); // rotate by -2deg (counterclockwise)
	struct json_object *jso = json_tokener_parse("[\
	{\"identifier\": \"cons HT\", \"box\": {\"x1\": 122, \"x2\": 235, \"y1\": 8, \"y2\": 54}},\
	{\"identifier\": \"cons NT\", \"box\": {\"x1\": 122 , \"x2\": 235, \"y1\": 58, \"y2\": 104}}]");
	options.push_back(Option("boundingboxes", jso));
	json_object_put(jso);
	MeterOCR m(options);

	ASSERT_EQ(SUCCESS, m.open());

	std::vector<Reading> rds;
	rds.resize(2);
	EXPECT_EQ(2, m.read(rds, 2));

	double value = rds[0].value();
	EXPECT_EQ(757.6, value); // 2nd value 11557.0
	value = rds[1].value();
	EXPECT_EQ(557.0, value); // 2nd value 11557.0 but we moved the boundingbox due to some light effect.
	

	ASSERT_EQ(0, m.close());
}

TEST(MeterOCR, emh_test2_not_prepared_digits)
{
	std::list<Option> options;
	options.push_back(Option("file", (char*)"tests/meterOCR/emh_test2.png"));
	options.push_back(Option("gamma", 0.8));
	options.push_back(Option("gamma_min", 50));
	options.push_back(Option("gamma_max", 180));
	//options.push_back(Option("rotate", -2.0)); // rotate by -2deg (counterclockwise)
	struct json_object *jso = json_tokener_parse("[\
	{\"identifier\": \"cons HT\", \"digit\":true, \"scaler\":2,\"box\": {\"x1\": 122, \"x2\": 151, \"y1\": 8, \"y2\": 54}},\
	{\"identifier\": \"cons HT\", \"digit\":true, \"scaler\":1,\"box\": {\"x1\": 152, \"x2\": 180, \"y1\": 8, \"y2\": 54}},\
	{\"identifier\": \"cons HT\", \"digit\":true, \"scaler\":0,\"box\": {\"x1\": 181, \"x2\": 208, \"y1\": 8, \"y2\": 54}},\
	{\"identifier\": \"cons HT\", \"digit\":true, \"scaler\":-1,\"box\": {\"x1\": 212, \"x2\": 235, \"y1\": 8, \"y2\": 54}},\
	{\"identifier\": \"cons NT\", \"digit\":true, \"scaler\":2,\"box\": {\"x1\": 122, \"x2\": 151, \"y1\": 58, \"y2\": 104}},\
	{\"identifier\": \"cons NT\", \"digit\":true, \"scaler\":1,\"box\": {\"x1\": 152, \"x2\": 180, \"y1\": 58, \"y2\": 104}},\
	{\"identifier\": \"cons NT\", \"digit\":true, \"scaler\":0,\"box\": {\"x1\": 181, \"x2\": 208, \"y1\": 58, \"y2\": 104}},\
	{\"identifier\": \"cons NT\", \"digit\":true, \"scaler\":-1,\"box\": {\"x1\": 212, \"x2\": 235, \"y1\": 58, \"y2\":104}}\
	]");
	options.push_back(Option("boundingboxes", jso));
	json_object_put(jso);
	MeterOCR m(options);

	ASSERT_EQ(SUCCESS, m.open());

	std::vector<Reading> rds;
	rds.resize(2);
	EXPECT_EQ(2, m.read(rds, 2));

	double value = rds[0].value();
	EXPECT_EQ(757.6, value); // 2nd value 11557.0
	value = rds[1].value();
	EXPECT_EQ(557.0, value); // 2nd value 11557.0 but we moved the boundingbox due to some light effect.
	

	ASSERT_EQ(0, m.close());
}


