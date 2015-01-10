/**
 *  Read data from captured images via text recognition using
 * tesseract-ocr.
 *
 * @package vzlogger
 * @copyright Copyright (c) 2015, The volkszaehler.org project
 * @license http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author Matthias Behr <mbehr (@) mcbehr.de>
 */
/*
 * This file is part of volkzaehler.org
 *
 * volkzaehler.org is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * volkzaehler.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with volkszaehler.org. If not, see <http://www.gnu.org/licenses/>.
 */
 
#ifndef _MeterOCR_H_
#define _MeterOCR_H_

#include <stdio.h>
#include <cfloat>
#include <map>
#include <protocols/Protocol.hpp>

namespace tesseract
{
	class TessBaseAPI;
}

typedef struct Pix PIX;
typedef struct Pixa PIXA;

class Reads{
public:
	Reads() : value(0.0), min_conf(DBL_MAX) {};
	double value;
	std::string conf_id;
	double min_conf;
};

typedef std::map<std::string, Reads> ReadsMap;

class MeterOCR : public vz::protocol::Protocol {

public:
	MeterOCR(std::list<Option> options);
	virtual ~MeterOCR();

	int open();
	int close();
	ssize_t read(std::vector<Reading> &rds, size_t n);
  
  private:

	bool autofixDetection(PIX *image, int &dX, int &dY, PIXA *debugPixa);

  // class for the parameters:
class BoundingBox
{
public:
	BoundingBox (struct json_object *jb);
	std::string identifier;
	std::string conf_id;
	int scaler;
	bool digit;
	int x1, y1, x2, y2;
};

typedef std::list<BoundingBox> StdListBB;

class Recognizer
{
public:
	Recognizer(const std::string &type) : _type(type) {};
	virtual bool recognize(PIX *image, int dX, int dY, ReadsMap &reads, PIXA *debugPixa ) = 0;
	virtual ~Recognizer(){};
protected:
	void saveDebugImage(PIXA* debugPixa, PIX* img, const char *title);
	std::string _type;
};

class RecognizerTesseract : public Recognizer
{
public:
	RecognizerTesseract(struct json_object *);
	bool recognize(PIX *image, int dX, int dY, ReadsMap &reads, PIXA *debugPixa );
	virtual ~RecognizerTesseract();
protected:
	bool initTesseract();
	bool deinitTesseract();
	
	tesseract::TessBaseAPI *api;
	double _gamma;
	int _gamma_min;
	int _gamma_max;
	int _min_x1, _max_x2, _min_y1, _max_y2;
	bool _all_digits;
	StdListBB _boxes;
	
};

class RecognizerNeedle : public Recognizer
{
public:
	RecognizerNeedle(struct json_object *);
	bool recognize(PIX *image, int dX, int dY, ReadsMap &reads, PIXA *debugPixa );
	virtual ~RecognizerNeedle();	
};

	std::string _file;
	double _rotate;
	std::list<Recognizer*> _recognizer;
	int _autofix_range, _autofix_x, _autofix_y;
};

#endif

