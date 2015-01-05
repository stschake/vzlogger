/**
 * Read data from captured images via text recognition using
 * tesseract-ocr.
 *
 * @copyright Copyright (c) 2015, Matthias Behr
 * @license http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author Matthias Behr <mbehr (@) mcbehr.de>
 */
/*
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with volkszaehler.org. If not, see <http://www.gnu.org/licenses/>.
 */

/*
Quick patent check: (no warranties, just my personal opinion, only checked as I respect IP rights)
 DE1020100530017 -> describing a different method to calculate the "Uebertrag". should not be affected
 EP1596164 -> using basically two different light sources. should not be affected
 DE102010053019 -> nice method to ease positioning sensors (see FAST EnergyCam). should not be affected.
 TODO check some more.
 
 my opinion: simple image recognition for numbers is "Stand der Technik"
*/


/*
	To-Do list:
	TODO add cache to see whether image changed (e.g. chksum, file date,...)
	TODO add conf level that has to be achieved otherwise values will be ignored
	TODO implement digit (just to add more conf by restricting the value to int -9...+9)
	TODO remove unit or implement it
	TODO move log_error to log_debug
	TODO use GetComponentImages interface (directly returning BOXA) and change to top/left/width/height?
	TODO use filter from http://www.jofcis.com/publishedpapers/2011_7_6_1886_1892.pdf for binarization?
	TODO check seedfill functions
	*/

// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/time.h>
// #include <errno.h>
#include <cmath>
#include <map>

#include "protocols/MeterOCR.hpp"
#include "Options.hpp"
#include <VZException.hpp>

/* install libleptonica: 

sudo apt-get install libpng-dev libtiff-dev
wget http://www.leptonica.org/source/leptonica-1.71.tar.gz
tar -zxvf leptonica-1.71.tar.gz
cd leptonica-1.71
./configure --disable-programs
make
sudo make install
sudo ldconfig

*/


/* install tesseract: 
sudo apt-get install libtool libpng-dev libjpeg-dev
git clone https://code.google.com/p/tesseract-ocr/
cd tesseract-ocr
git checkout 3.02.02
./autogen.sh
./configure
make
sudo make install
sudo ldconfig
cd ..
wget https://tesseract-ocr.googlecode.com/files/tesseract-ocr-3.02.deu.tar.gz
sudo cp tesseract-ocr/tessdata/deu.traineddata /usr/local/share/tessdata/
sudo cp tesseract-ocr/tessdata/deu-frak.traineddata /usr/local/share/tessdata/

*/

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

MeterOCR::BoundingBox::BoundingBox(struct json_object *jb) :
	scaler(0), digit(false), x1(-1), y1(-1), x2(-1), y2(-1)
{
	// we need at least "identifier"
	struct json_object *value;
	if (json_object_object_get_ex(jb, "identifier", &value)){
		identifier = json_object_get_string(value);
	}else{
		throw vz::OptionNotFoundException("boundingbox identifier");
	}
	if (json_object_object_get_ex(jb, "unit", &value)){
		unit = json_object_get_string(value);
	}
	if (json_object_object_get_ex(jb, "scaler", &value)){
		scaler = json_object_get_int(value);
	}
	if (json_object_object_get_ex(jb, "digit", &value)){
		digit = json_object_get_boolean(value);
	}
	// now parse the box coordinates:
	if (json_object_object_get_ex(jb, "box", &value)){
		// x1, y1, x2, y2
		struct json_object *jv;
		if (json_object_object_get_ex(value, "x1", &jv)){
			x1 = json_object_get_int(jv);
		}
		if (json_object_object_get_ex(value, "y1", &jv)){
			y1 = json_object_get_int(jv);
		}
		if (json_object_object_get_ex(value, "x2", &jv)){
			x2 = json_object_get_int(jv);
			if (x2<x1) throw vz::OptionNotFoundException("boundingbox x2 < x1");
		}
		if (json_object_object_get_ex(value, "y2", &jv)){
			y2 = json_object_get_int(jv);
			if (y2<y1) throw vz::OptionNotFoundException("boundingbox y2 < y1");
		}
	}
	print(log_error, "boundingbox <%s>: unit=<%s>, scaler=%d, digit=%d, (%d,%d)-(%d,%d)\n", "ocr",
		identifier.c_str(), unit.c_str(), scaler, digit? 1 : 0, x1, y1, x2, y2);
}


MeterOCR::MeterOCR(std::list<Option> options)
		: Protocol("ocr"), api(NULL), _rotate(0.0), _gamma(1.0), _gamma_min(50), _gamma_max(120),
		_min_x1(INT_MAX), _max_x2(INT_MIN), 
		_min_y1(INT_MAX), _max_y2(INT_MIN), _all_digits(true)
{
	OptionList optlist;

	try {
		_file = optlist.lookup_string(options, "file");
	} catch (vz::VZException &e) {
		print(log_error, "Missing image file name", name().c_str());
		throw;
	}
	try {
		_rotate = optlist.lookup_double(options, "rotate");
	} catch (vz::OptionNotFoundException &e) {
		// keep default (0.0)
	} catch (vz::InvalidTypeException &e) {
		print(log_error, "Invalid type for 'rotate'", name().c_str());
		throw;
	} catch (vz::VZException &e) {
		print(log_error, "Failed to parse 'rotate'", name().c_str());
		throw;
	}
	
	try {
		_gamma = optlist.lookup_double(options, "gamma");
	} catch (vz::OptionNotFoundException &e) {
		// keep default (1.0)
	} catch (vz::InvalidTypeException &e) {
		print(log_error, "Invalid type for 'gamma'", name().c_str());
		throw;
	} catch (vz::VZException &e) {
		print(log_error, "Failed to parse 'gamma'", name().c_str());
		throw;
	}
		try {
		_gamma_min = optlist.lookup_int(options, "gamma_min");
	} catch (vz::OptionNotFoundException &e) {
		// keep default
	} catch (vz::InvalidTypeException &e) {
		print(log_error, "Invalid type for 'gamma_min'", name().c_str());
		throw;
	} catch (vz::VZException &e) {
		print(log_error, "Failed to parse 'gamma_min'", name().c_str());
		throw;
	}
	try {
		_gamma_max = optlist.lookup_int(options, "gamma_max");
	} catch (vz::OptionNotFoundException &e) {
		// keep default (1.0)
	} catch (vz::InvalidTypeException &e) {
		print(log_error, "Invalid type for 'gamma_max'", name().c_str());
		throw;
	} catch (vz::VZException &e) {
		print(log_error, "Failed to parse 'gamma_max'", name().c_str());
		throw;
	}

	try {
		struct json_object *jso = optlist.lookup_json_array(options, "boundingboxes");
		print(log_error, "boundingboxes=%s", name().c_str(), jso ? json_object_to_json_string(jso) : "<null>");
		int nrboxes;
		if (jso && (nrboxes=json_object_array_length(jso))>=1){
			// for each object:
			for (int i = 0; i < nrboxes; i++) {
				struct json_object *jb = json_object_array_get_idx(jso, i);
				_boxes.push_back(BoundingBox(jb));
			}
		}else{
			throw vz::OptionNotFoundException("no boundingboxes given");
		}
	} catch (vz::OptionNotFoundException &e) {
		// boundingboxes is mandatory
		//print(log_error, "Config parameter 'boundingboxes' missing!", name().c_str());
		throw;
	} catch (vz::VZException &e) {
		print(log_error, "Failed to parse 'boundingboxes'", name().c_str());
		throw;
	}
	
	// calc the max. bounding box. images will be cropped to it:
	for (StdListBB::iterator it = _boxes.begin(); it != _boxes.end(); ++it){ // let's stick to begin not cbegin (c++11)
		const BoundingBox &b = *it;
		if (b.x1<_min_x1) _min_x1=b.x1;
		if (b.x2>_max_x2) _max_x2=b.x2;
		if (b.y1<_min_y1) _min_y1=b.y1;
		if (b.y2>_max_y2) _max_y2=b.y2;
		if (!b.digit) _all_digits=false;
	}
	if (_min_x1<0) _min_x1=0;
	if (_min_y1<0) _min_y1=0;

	// init tesseract-ocr without specifiying tessdata path
	api = new tesseract::TessBaseAPI();
	
	// disable dictionary:
	api->SetVariable("load_system_dawg", "F");
	api->SetVariable("load_freq_dawg", "F");

	// only for debugging:
	api->SetVariable("tessedit_write_images", "T");
	
	if (api->Init(NULL, "deu")) {
		delete api;
		api = NULL;
		print(log_error, "Could not init tesseract!", name().c_str());
		throw vz::VZException("could not init tesseract");
	}
	
	api->SetVariable("tessedit_char_whitelist", "0123456789.m"); // TODO think about removing 'm' (should not be within the boundingboxes)
	api->SetPageSegMode(_all_digits ? tesseract::PSM_SINGLE_CHAR : tesseract::PSM_SINGLE_BLOCK); // PSM_SINGLE_WORD);
	
}

MeterOCR::~MeterOCR() {
	// end tesseract usage:
	api->End();
	delete api;
}

int MeterOCR::open() {
	// check  (open/close) file on each reading, so we check here just once
	FILE *_fd = fopen(_file.c_str(), "r");

	if (_fd == NULL) {
		print(log_error, "fopen(%s): %s", name().c_str(), _file.c_str(), strerror(errno));
		return ERR;
	}

	(void) fclose(_fd);
	_fd = NULL;

	return SUCCESS;
}

int MeterOCR::close() {
	return 0;
}

double radians (double d) { 
return d * M_PI / 180; 
} 

ssize_t MeterOCR::read(std::vector<Reading> &rds, size_t n) {

	unsigned int i = 0;
	std::string outtext, outfilename;
	std::string id;
	print(log_debug, "MeterOCR::read: %d, %d", name().c_str(), rds.size(), n);

	if (n<1) return 0;

	// open image:
	Pix *image = pixRead(_file.c_str());
	if (!image){
		print(log_debug, "pixRead returned NULL!", name().c_str());
		return 0;
	}

	// rotate image if parameter set:
	if (abs(_rotate)>=0.1){
		Pix *image_rot = pixRotate(image, radians(_rotate), L_ROTATE_AREA_MAP, L_BRING_IN_WHITE, 0,0);
	
		if (image_rot){
			pixDestroy(&image);
			image = image_rot;
			image_rot=0;
		}

		outfilename=_file;
		outfilename.append("rot");
		(void) pixWrite(outfilename.c_str(), image, IFF_DEFAULT);
		// TODO double check with pixFindSkew? (auto-rotate?)
	}

	// now crop the image if possible:
	if (_min_x1>0 || _max_x2>0 || _min_y1>0 || _max_y2>0){
		int w = _max_x2>0 ? _max_x2 : pixGetWidth(image);
		w -= _min_x1;
		int h = _max_y2>0 ? _max_y2 : pixGetHeight(image);
		h -= _min_y1;
		print(log_error, "Cropping image to (%d,%d)x(%d,%d)", name().c_str(), _min_x1, _min_y1, w, h);
		PIX *image2 = pixCreate(w, h, pixGetDepth(image));
		pixCopyResolution(image2, image);
		pixCopyColormap(image2, image);
		pixRasterop(image2, 0, 0, w, h, PIX_SRC, image, _min_x1, _min_y1);
		pixDestroy(&image);
		image=image2;
	}

	// TODO add support for contrast?
/*	image = pixContrastTRC(image, image, 0.8);
	outfilename=_file;
	outfilename.append("_s0_constrast.png");
	(void)pixWrite(outfilename.c_str(), image, IFF_PNG);	
*/

	// Convert the RGB image to grayscale
	Pix *image_gs = pixConvertRGBToLuminance(image);
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		outfilename=_file;
		outfilename.append("_s1_gs.png");
		(void)pixWrite(outfilename.c_str(), image, IFF_PNG);
	}

	// Increase the dynamic range
	// make dark gray *black* and light gray *white*
	image = pixGammaTRC(image, image, _gamma, _gamma_min, _gamma_max);
	if (image){
		outfilename=_file;
		outfilename.append("_s2_gamma.png");
		(void)pixWrite(outfilename.c_str(), image, IFF_PNG);
	}

	//image_gs = pixCloseGray(image, 25, 25);
	image_gs = pixUnsharpMaskingGray( image, 3, 0.5 ); // TODO make a parameter, only useful for blurred images
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		outfilename=_file;
		outfilename.append("_s3_unsharp.png");
		(void)pixWrite(outfilename.c_str(), image, IFF_PNG);
	}

 /*
	image = pixGammaTRC(image, image, _gamma, 50, 200); 
	outfilename=_file;
	outfilename.append("_s4_gamma.png");
	(void)pixWrite(outfilename.c_str(), image, IFF_PNG);
	
	pixOtsuAdaptiveThreshold(image, pixGetWidth(image)/4, pixGetHeight(image), 1, 1, 0.1, NULL, &image_gs);
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		outfilename=_file;
		outfilename.append("_s5_otsu.png");
		(void)pixWrite(outfilename.c_str(), image, IFF_PNG);
	}
 try the Otsu approach */
	// Normalize for uneven illumination on gray image
	Pix *pixg=0;
	pixBackgroundNormGrayArrayMorph(image, NULL, 4, 5, 200, &pixg);
	image_gs = pixApplyInvBackgroundGrayMap(image, pixg, 4, 4);
	pixDestroy(&pixg);
	
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		outfilename=_file;
		outfilename.append("_s4_no.png");
		(void)pixWrite(outfilename.c_str(), image, IFF_PNG);
	}

	image_gs = pixBlockconv(image, 1, 1);
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		outfilename=_file;
		outfilename.append("_s6_bc.png");
		(void)pixWrite(outfilename.c_str(), image, IFF_PNG);
	}

	// Threshold to 1 bpp
	image_gs = pixThresholdToBinary(image, 120);
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		outfilename=_file;
		outfilename.append("_s7_bi.png");
		(void)pixWrite(outfilename.c_str(), image, IFF_PNG);
	}
	
	api->SetImage(image);
	
	Pix *dump = api->GetThresholdedImage();
//	outfilename=_file;
//	outfilename.append("thresh.tif");
//    pixWrite(outfilename.c_str(), dump, IFF_TIFF_G4);

    // get picture size:
    int width = pixGetWidth(image);
    int height = pixGetHeight(image);
    print(log_error, "Image size width=%d, height=%d\n", name().c_str(), width, height);
	
	std::map<std::string, double> readings;
	BOXA *boxa = boxaCreate(_boxes.size()); // there can be more, just initial number
	BOXA *boxb = boxaCreate(_boxes.size());

	// now iterate for each bounding box defined:
	for (StdListBB::iterator it = _boxes.begin(); it != _boxes.end(); ++it){ // let's stick to begin not cbegin (c++11)
		const BoundingBox &b = *it;
		int left = b.x1 >= _min_x1 ? b.x1-_min_x1 : 0;
		int top = b.y1 >= _min_y1 ? b.y1-_min_y1 : 0;
		int w= b.x2>=0? b.x2-left-_min_x1 : (width-left);
		int h= b.y2>=0? b.y2-top-_min_y1 : (height-top);
		api->SetRectangle( // left, top, width, height // BoundingBox are abs. coordinates. SetRectangle are relative (w/h)
			left, 
			top,
			w,
			h);

		BOX *box=boxCreate(left, top, w, h);
		boxaAddBox(boxb, box, L_INSERT);

		api->Recognize(0);
		tesseract::ResultIterator* ri = api->GetIterator();
		tesseract::PageIteratorLevel level = tesseract::RIL_WORD;
		if (ri != 0){
			do{
				const char* word = ri->GetUTF8Text(level);
				float conf = ri->Confidence(level);
				int x1, y1, x2, y2;
				ri->BoundingBox(level, &x1, &y1, &x2, &y2);
				print(log_error, "word: '%s'; \tconf: %.2f; BoundingBox: %d,%d,%d,%d;\n", name().c_str(),
					word, conf, x1, y1, x2, y2);
				if (conf>15.0 && outtext.length()==0) outtext=word; // TODO choose the one with highest confidence? or ignore if more than 1?
				delete[] word;	

				// for debugging draw the box in the picture:
				BOX *box=boxCreate(x1, y1, x2-x1, y2-y1);
				boxaAddBox(boxa, box, L_INSERT);
			} while (ri->Next(level));
		}
		print(log_error, "%s=%s", name().c_str(), b.identifier.c_str(), outtext.c_str());
		readings[b.identifier] += strtod(outtext.c_str(), NULL) * pow(10, b.scaler);
		outtext.erase();
	}

	Pix *bbpix2 = pixDrawBoxa(dump, boxa, 1, 0x00ff0000); // rgba color -> green, detected
	Pix *bbpix = pixDrawBoxa(bbpix2, boxb, 1, 0x0000ff00); // blue = bounding box for search
	pixDestroy(&bbpix2);
	boxaDestroy(&boxa);
	boxaDestroy(&boxb);
	outfilename=_file;
	outfilename.append("bb.png");
	pixWrite(outfilename.c_str(), bbpix, IFF_PNG);
	pixDestroy(&bbpix);

	pixDestroy(&dump);

	// return all readings:
	for(std::map<std::string, double>::iterator it = readings.begin(); it!= readings.end(); ++it){
		print(log_error, "returning: id <%s> value <%f>", name().c_str(), it->first.c_str(), it->second);
		rds[i].value(it->second); // TODO shall we add the unit here or simply don't support units?
		rds[i].identifier(new StringIdentifier(it->first));
		rds[i].time();
		i++;
	}
	
	pixDestroy(&image);

	return i;
}

