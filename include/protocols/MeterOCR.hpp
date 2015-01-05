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
#include <protocols/Protocol.hpp>

namespace tesseract
{
	class TessBaseAPI;
}

class MeterOCR : public vz::protocol::Protocol {

public:
	MeterOCR(std::list<Option> options);
	virtual ~MeterOCR();

	int open();
	int close();
	ssize_t read(std::vector<Reading> &rds, size_t n);
  
  private:

  // class for the parameters:
class BoundingBox
{
public:
	BoundingBox (struct json_object *jb);
	std::string identifier;
	std::string unit;
	int scaler;
	bool digit;
	int x1, y1, x2, y2;
};

	std::string _file;
	tesseract::TessBaseAPI *api;
	double _rotate;
	double _gamma;
	int _gamma_min;
	int _gamma_max;
	typedef std::list<BoundingBox> StdListBB;
	StdListBB _boxes;
	int _min_x1, _max_x2, _min_y1, _max_y2;
	bool _all_digits;
};

#endif

