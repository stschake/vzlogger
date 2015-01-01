#include "gtest/gtest.h"
#include "Options.hpp"
#include "protocols/MeterD0.hpp"

// this is a dirty hack. we should think about better ways/rules to link against the
// test objects.
// e.g. single files in the tests directory that just include the real file. This way one
// will avoid multiple includes of the same file in a different test case
// (or linking against the real file from the CMakeLists.txt)

#include "../src/protocols/MeterD0.cpp"
#include "../src/Options.cpp"
#include "../src/Obis.cpp"
#include "../src/Reading.cpp"

void print(log_level_t l, char const*s1, char const*s2, ...)
{
	if (l!= log_debug)
	{
		fprintf(stdout, "\n  %s:", s2);
		va_list argp;
		va_start(argp, s2);
		vfprintf(stdout, s1, argp);
		va_end(argp);
		fprintf(stdout, "\n");
	}
}

int writes(int fd, const char *str)
{
	EXPECT_NE((char*)0, str);
	int len = strlen(str);
	return write(fd, str, len);
}

TEST(MeterD0, HagerEHZ_basic) {
	char tempfilename[L_tmpnam+1];
	ASSERT_NE(tmpnam_r(tempfilename), (char*)0);
	std::list<Option> options;
	options.push_back(Option("device", tempfilename));
	MeterD0 m(options);
	ASSERT_STREQ(m.device(), tempfilename) << "devicename not eq " << tempfilename;
	ASSERT_EQ(0, mkfifo(tempfilename, S_IRUSR|S_IWUSR));
	int fd = open(tempfilename, O_RDWR);
	ASSERT_NE(fd, -1);
	ASSERT_EQ(SUCCESS, m.open());

	// now we can simulate some input by simply writing into fd
	std::vector<Reading> rds;
	rds.resize(10);

	// write two good data set
	writes(fd, "/HAG5eHZ010C_EHZ1vA02\r\n");
	writes(fd, "1-0:1.8.0*255(000001.2963)\r\n"); // works only with \r\n error (see ack handling)
	writes(fd, "!\n");
	writes(fd, "/HAG5eHZ010C_EHZ1vA02\r\n");
	writes(fd, "1-0:1.7.0*255(000001.2964)\r\n");
	writes(fd, "1-0:1.9.0*255(000001.2965)\r\n");
	writes(fd, "!\n");

	// now read two readings:
	EXPECT_EQ(1, m.read(rds, 10));
	// check obis data:
	ReadingIdentifier *p = rds[0].identifier().get();
	double value = rds[0].value();
	EXPECT_EQ(1.2963, value);
	ObisIdentifier *o = dynamic_cast<ObisIdentifier*>(p);
	ASSERT_NE((ObisIdentifier*)0, o);
	EXPECT_TRUE(Obis(1, 0, 1, 8, 0, 255)==(o->obis()));

	EXPECT_EQ(2, m.read(rds, 2));
	o = dynamic_cast<ObisIdentifier*>(rds[0].identifier().get());
	ASSERT_NE((ObisIdentifier*)0, o);
	value = rds[0].value();
	EXPECT_EQ(1.2964, value);
	EXPECT_TRUE(Obis(1, 0, 1, 7, 0, 255)==(o->obis()));

	o = dynamic_cast<ObisIdentifier*>(rds[1].identifier().get());
	ASSERT_NE((ObisIdentifier*)0, o);
	value = rds[1].value();
	EXPECT_EQ(1.2965, value);
	EXPECT_TRUE(Obis(1, 0, 1, 9, 0, 255)==(o->obis()));

	EXPECT_EQ(0, m.close());

	EXPECT_EQ(0, close(fd));
	EXPECT_EQ(0, unlink(tempfilename));
}

TEST(MeterD0, HagerEHZ_waitsync) {
	char tempfilename[L_tmpnam+1];
	char strend[5] = "end\0";
	ASSERT_NE(tmpnam_r(tempfilename), (char*)0);
	std::list<Option> options;
	options.push_back(Option("device", tempfilename));
	options.push_back(Option("wait_sync", strend));
	MeterD0 m(options);
	ASSERT_STREQ(m.device(), tempfilename) << "devicename not eq " << tempfilename;
	ASSERT_EQ(0, mkfifo(tempfilename, S_IRUSR|S_IWUSR));
	int fd = open(tempfilename, O_RDWR);
	ASSERT_NE(fd, -1);
	ASSERT_EQ(SUCCESS, m.open());

	// now we can simulate some input by simply writing into fd
	std::vector<Reading> rds;
	rds.resize(10);

	// write one good data set: (two as wait_sync end skips the first one!)
	writes(fd, "HAG5eHZ010C_EHZ1vA02\r\n"); // this is invalid as the first char is missing but that's what wait_sync is for
	writes(fd, "1-0:1.8.0*255(000001.2963)\r\n"); // works only with \r\n error (see ack handling)
	writes(fd, "!\n");
	writes(fd, "/HAG5eHZ010C_EHZ1vA02\r\n");
	writes(fd, "2-1:2.3.4*255(999999.9999)\r\n");
	writes(fd, "!\n");

	EXPECT_EQ(1, m.read(rds, 10));
	// check obis data:
	ReadingIdentifier *p = rds[0].identifier().get();
	double value = rds[0].value();
	EXPECT_EQ(999999.9999, value);
	ObisIdentifier *o = dynamic_cast<ObisIdentifier*>(p);
	ASSERT_NE((ObisIdentifier*)0, o);
	EXPECT_TRUE(Obis(2, 1, 2, 3, 4, 255)==(o->obis()));


	EXPECT_EQ(0, m.close());

	EXPECT_EQ(0, close(fd));
	EXPECT_EQ(0, unlink(tempfilename));
}

TEST(MeterD0, LandisGyr_basic) {
	char tempfilename[L_tmpnam+1];
	char str_pullseq[12] = "2f3f210d0a";
	ASSERT_NE(tmpnam_r(tempfilename), (char*)0);
	std::list<Option> options;
	options.push_back(Option("device", tempfilename));
	options.push_back(Option("pullseq", str_pullseq));
	MeterD0 m(options);
	ASSERT_STREQ(m.device(), tempfilename) << "devicename not eq " << tempfilename;
	ASSERT_EQ(0, mkfifo(tempfilename, S_IRUSR|S_IWUSR));
	int fd = open(tempfilename, O_RDWR);
	ASSERT_NE(fd, -1);
	ASSERT_EQ(SUCCESS, m.open());

	// now we can simulate some input by simply writing into fd
	std::vector<Reading> rds;
	rds.resize(10);

	// write one good data set
	/*
	/?!                 <-- Wiederholung der gesendeten Pullsequenz

	/LGZ52ZMD120APt.G03 <-- Antwort mit Herstellerkennung, mögliche Baudrate und Typ
	F.F(00000000)       <-- Fehlercode
	0.0.0( 20000)       <-- Zählernummer
	1.8.1(001846.0*kWh) <-- Hochtarif, bzw. einziger Tarif: Zählerstand Energielieferung
	1.8.2(000000.0*kWh) <-- Niedertarif: Zählerstand Energielieferung
	2.8.1(004329.6*kWh) <-- Hochtarif, bzw. einziger Tarif: Zählerstand Energieeinspeisung
	2.8.2(000000.0*kWh) <-- Niedertarif: Zählerstand Energieeinspeisung
	1.8.0(001846.0*kWh) <-- Summe Zählerstand Energielieferung
	2.8.0(004329.6*kWh) <-- Summe Zählerstand Energieeinspeisung
	!                   <-- Endesequenz
	*/
	// without splitting the MeterD0::read source/logic (or multithreading) we can't
	// wait for the pullseq before we put the data. Thus we just put the data in the buffer
	// and check afterwards whether a pullseq was send as well.


	writes(fd, "/?!\r\n/LGZ52ZMD120APt.G03\r\n");
	writes(fd, "F.F(00000000)\r\n"); // works only with \r\n error (see ack handling)
	writes(fd, "0.0.0( 20000)\r\n");
	writes(fd, "1.8.1(001846.0*kWh)\r\n");
	writes(fd, "1.8.2(000000.0*kWh)\r\n");
	writes(fd, "2.8.1(004329.6*kWh)\r\n");
	writes(fd, "2.8.2(000000.0*kWh)\r\n");
	writes(fd, "1.8.0(001846.0*kWh)\r\n");
	writes(fd, "2.8.0(004329.6*kWh)\r\n");
	writes(fd, "!");

	// now perform one read call
	EXPECT_EQ(8, m.read(rds, 10));
	// check whether pullseq is remaining in the fifo:
	char buf[100];
	ssize_t len = read(fd, buf, sizeof(buf));
	ASSERT_EQ((ssize_t)strlen("/?!\r\n"), len) << "buf=[" << buf << "]";

	// check obis data:
	ReadingIdentifier *p = rds[2].identifier().get();
	double value = rds[2].value();
	EXPECT_EQ(1846.0, value);
	ObisIdentifier *o = dynamic_cast<ObisIdentifier*>(p);
	ASSERT_NE((ObisIdentifier*)0, o);
	EXPECT_TRUE(Obis(0xff, 0xff, 1, 8, 1, 255)==(o->obis()));

	o = dynamic_cast<ObisIdentifier*>(rds[3].identifier().get());
	ASSERT_NE((ObisIdentifier*)0, o);
	value = rds[3].value();
	EXPECT_EQ(0.0, value);
	EXPECT_TRUE(Obis(0xff, 0xff, 1, 8, 2, 0xff)==(o->obis()));

	o = dynamic_cast<ObisIdentifier*>(rds[4].identifier().get());
	ASSERT_NE((ObisIdentifier*)0, o);
	value = rds[4].value();
	EXPECT_EQ(4329.6, value);
	EXPECT_TRUE(Obis(0xff, 0xff, 2, 8, 1, 0xff)==(o->obis()));

	EXPECT_EQ(0, m.close());

	EXPECT_EQ(0, close(fd));
	EXPECT_EQ(0, unlink(tempfilename));
}

int writes_hex(int fd, const char *str);

TEST(MeterD0, ACE3000_basic) {
	char tempfilename[L_tmpnam+1];
	char str_pullseq[12] = "2f3f210d0a";
	ASSERT_NE(tmpnam_r(tempfilename), (char*)0);
	std::list<Option> options;
	options.push_back(Option("device", tempfilename));
	options.push_back(Option("pullseq", str_pullseq));
	MeterD0 m(options);
	ASSERT_STREQ(m.device(), tempfilename) << "devicename not eq " << tempfilename;
	ASSERT_EQ(0, mkfifo(tempfilename, S_IRUSR|S_IWUSR));
	int fd = open(tempfilename, O_RDWR);
	ASSERT_NE(fd, -1);
	ASSERT_EQ(SUCCESS, m.open());

	// now we can simulate some input by simply writing into fd
	std::vector<Reading> rds;
	rds.resize(5);

	// write one good data set
	/*
	multiple <0x7f> ... garbage?
	/?!                 <-- Wiederholung der gesendeten Pullsequenz
	/ACE03k260V01.19    <-- Antwort mit Herstellerkennung, mögliche Baudrate und Typ
	F.F(00)             <-- Fehlercode
	C.1(1126120053322353)  <-- Zählernummer ?
	C.5.0(00)           <-- ?
	1.8.0(013925.5*)    <-- Summe Zählerstand Energielieferung
	Y<0x02><0x02><0x01><0x00>!<0x0d><0x0a><0x03>F<0x7f>    <-- Endesequenz and garbage?
	*/
	// without splitting the MeterD0::read source/logic (or multithreading) we can't
	// wait for the pullseq before we put the data. Thus we just put the data in the buffer
	// and check afterwards whether a pullseq was send as well.


	writes_hex(fd, "7f7f7f7f7f2f3f210d0a2f414345305c336b3236305630312e31390d0a");
	writes_hex(fd, "02462e46283030290d0a432e31283131323631");
	writes_hex(fd, "3230303533333232333533290d0a");
	writes_hex(fd, "432e352e30283030290d0a"); // C.5.0(00)
	writes_hex(fd, "312e382e30283031333932352e352a29590202010021"); // 1.8.0(01392.5*) ... !
	writes_hex(fd, "0d0a03467f"); // (newline and <ETX> <BCC =0x46> garbage...) TODO add BCC check (according to DIN 66219 / IEC 1155, if STX/ETX there should be BCC as well. BCC = xor all from STX (not incl.) to ETX (incl.))

	// now perform one read call:
	EXPECT_EQ(4, m.read(rds, 4));
	// check whether garbage (after !) and pullseq is remaining in the fifo:
	// will be ignored on next read call TODO add check
	char buf[100];
	ssize_t len = read(fd, buf, sizeof(buf));
	ASSERT_EQ((ssize_t)strlen("\x0d\x0a\x03\x46\x7f/?!\r\n"), len) << "buf=[" << buf << "]";

	// check obis data:
	ReadingIdentifier *p = rds[3].identifier().get();
	double value = rds[3].value();
	EXPECT_EQ(13925.5, value);
	ObisIdentifier *o = dynamic_cast<ObisIdentifier*>(p);
	ASSERT_NE((ObisIdentifier*)0, o);
	EXPECT_TRUE(Obis(0xff, 0xff, 1, 8, 0, 255)==(o->obis()));

	o = dynamic_cast<ObisIdentifier*>(rds[1].identifier().get());
	ASSERT_NE((ObisIdentifier*)0, o);
	value = rds[1].value();
	EXPECT_EQ(1126120053322353.0, value);
	EXPECT_TRUE(Obis(0xff, 0xff, 96, 1, 0xff, 0xff)==(o->obis()));

	o = dynamic_cast<ObisIdentifier*>(rds[0].identifier().get());
	ASSERT_NE((ObisIdentifier*)0, o);
	value = rds[0].value();
	EXPECT_EQ(0.0, value);
	EXPECT_TRUE(Obis(0xff, 0xff, 97, 97, 0xff, 0xff)==(o->obis()));

	EXPECT_EQ(0, m.close());

	EXPECT_EQ(0, close(fd));
	EXPECT_EQ(0, unlink(tempfilename));
}


TEST(MeterD0, SLB_DC3_basic)
{
	/* problem from http://volkszaehler.org/pipermail/volkszaehler-users/2014-December/005065.html
	 * meter uses EDIS not OBIS. See here for difference: http://www.emsycon.de/downloads/Note0503_683.pdf
[Dec 23 11:59:07][d0]   Pull answer (vendor=SLB, baudrate=4, identification=\@DC341TMPBF2ZAK)
[Dec 23 11:59:11][d0]   Sending ack sequence send (len:6 is:6,041
).
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte  hex= 2
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte F hex= 46
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte . hex= 2E
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte F hex= 46
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte ( hex= 28
[Dec 23 11:59:11][d0]   Parsed reading (OBIS code=F.F, value=00000000, unit=)
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte
 hex= A
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte 0 hex= 30
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte . hex= 2E
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte 0 hex= 30
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte . hex= 2E
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte 0 hex= 30
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte ( hex= 28
[Dec 23 11:59:11][d0]   Parsed reading (OBIS code=0.0.0, value=02538883, unit=)
[Dec 23 11:59:11][d0]   DEBUG OBIS_CODE byte
 hex= A

	 *
	 */

	// for this meter it's important to have no wildcard logic as
	// otherwise the history data confuse the real data
	// let's check a simple case for now:
	ASSERT_EQ(Obis("1.8.0"), Obis(255,255,1,8,0,255));
	ASSERT_FALSE(Obis("1.8.0*1")==Obis("1.8.0"));
	ASSERT_EQ(Obis("1.8.0*1"), Obis(255,255,1,8,0,1));

}
