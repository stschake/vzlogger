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
	TODO move log_error to log_debug
	TODO use GetComponentImages interface (directly returning BOXA) and change to top/left/width/height?
	TODO use filter from http://www.jofcis.com/publishedpapers/2011_7_6_1886_1892.pdf for binarization?
	TODO check seedfill functions
	TODO think about using either some trainingdata or e.g. http://www.unix-ag.uni-kl.de/~auerswal/ssocr/ to detect LCD digits.
	TODO sanity check if digit used and recognized bb is far too small.
	TODO check leptonicas 1.71 simple character recognition
	*/

// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/time.h>
// #include <errno.h>
#include <cmath>

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
tar xzvf tesseract-ocr-3.02.deu.tar.gz
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
	if (json_object_object_get_ex(jb, "confidence_id", &value)){
		conf_id = json_object_get_string(value);
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
	print(log_error, "boundingbox <%s>: conf_id=%s, scaler=%d, digit=%d, (%d,%d)-(%d,%d)\n", "ocr",
		identifier.c_str(), conf_id.c_str(), scaler, digit? 1 : 0, x1, y1, x2, y2);
}

MeterOCR::RecognizerNeedle::RecognizerNeedle(struct json_object *jr) :
	Recognizer("needle")
{
(void)jr; // TODO
}

bool MeterOCR::RecognizerNeedle::recognize(PIX *image, int dX, int dY, ReadsMap &reads, PIXA *debugPixa )
{
(void)image; (void) dX; (void) dY; (void)reads; (void)debugPixa; // TODO
	return true;
}

MeterOCR::RecognizerNeedle::~RecognizerNeedle()
{
}


MeterOCR::RecognizerTesseract::RecognizerTesseract(struct json_object *jr) :
	Recognizer("tesseract"), api(0),
	_gamma(1.0), _gamma_min(50), _gamma_max(120),
	_min_x1(INT_MAX), _max_x2(INT_MIN), 
	_min_y1(INT_MAX), _max_y2(INT_MIN), 

	_all_digits(true)
{
	struct json_object *value;
	if (json_object_object_get_ex(jr, "gamma", &value)){
		_gamma = json_object_get_double(value);
	} 
	if (json_object_object_get_ex(jr, "gamma_min", &value)){
		_gamma_min = json_object_get_int(value);
	} 
	if (json_object_object_get_ex(jr, "gamma_max", &value)){
		_gamma_max = json_object_get_int(value);
	} 

	try {
		if (json_object_object_get_ex(jr, "boundingboxes", &value)){
			print(log_error, "boundingboxes=%s", "RecognizerTesseract", value ? json_object_to_json_string(value) : "<null>");
			int nrboxes;
			if (value && (nrboxes=json_object_array_length(value))>=1){
				// for each object:
				for (int i = 0; i < nrboxes; i++) {
					struct json_object *jb = json_object_array_get_idx(value, i);
					_boxes.push_back(BoundingBox(jb));
				}
			}else{
				throw vz::OptionNotFoundException("no boundingboxes given");
			}
		}else{
			throw vz::OptionNotFoundException("no boundingboxes given");
		}		
	} catch (vz::OptionNotFoundException &e) {
		// boundingboxes is mandatory
		throw;
	} catch (vz::VZException &e) {
		print(log_error, "Failed to parse 'boundingboxes'", "RecognizerTesseract");
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
	
	// extend by autorange: (not needed anymore, will be done before)
	/*
	if (_autofix_range>0){
		if (_autofix_x-_autofix_range < _min_x1) _min_x1 = _autofix_x-_autofix_range;
		if (_autofix_y-_autofix_range < _min_y1) _min_y1 = _autofix_y-_autofix_range;
		if (_autofix_x+_autofix_range > _max_x2) _max_x2 = _autofix_x+_autofix_range;
		if (_autofix_y+_autofix_range > _max_y2) _max_y2 = _autofix_y+_autofix_range;
	}*/
	if (_min_x1<0) _min_x1=0;
	if (_min_y1<0) _min_y1=0;
}

MeterOCR::MeterOCR(std::list<Option> options)
		: Protocol("ocr"), _rotate(0.0), 
		_autofix_range(0), _autofix_x(-1), _autofix_y(-1)
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
	

	try{
		struct json_object *jso = optlist.lookup_json_object(options, "autofix");
		print(log_error, "autofix=%s", name().c_str(), jso ? json_object_to_json_string(jso) : "<null>");
		// range, x, y
		struct json_object *jv;
		if (json_object_object_get_ex(jso, "range", &jv)){
			_autofix_range = json_object_get_int(jv);
		}
		if (json_object_object_get_ex(jso, "x", &jv)){
			_autofix_x = json_object_get_int(jv);
		}
		if (json_object_object_get_ex(jso, "y", &jv)){
			_autofix_y = json_object_get_int(jv);
		}
		if (_autofix_range <1 || _autofix_x<_autofix_range || _autofix_y<_autofix_range ){ // all 3 values need to be valid if autofix specified
			throw vz::OptionNotFoundException("autofix range < 1 or x < range or y < range");
		}
	} catch (vz::OptionNotFoundException &e) {
		// autofix is optional (default none)
	} catch (vz::VZException &e) {
		print(log_error, "Failed to parse 'autofix'", name().c_str());
		throw;
	}
	
	try {
		struct json_object *jso = optlist.lookup_json_array(options, "recognizer");
		print(log_error, "recognizer=%s", name().c_str(), jso ? json_object_to_json_string(jso) : "<null>");
		int nrboxes;
		if (jso && (nrboxes=json_object_array_length(jso))>=1){
			// for each object:
			for (int i = 0; i < nrboxes; i++) {
				struct json_object *jb = json_object_array_get_idx(jso, i);
				Recognizer *r=0;

				// check type, default to tesseract
				std::string rtype("tesseract");
				struct json_object *value;
				if (json_object_object_get_ex(jb, "type", &value)){
					rtype = json_object_get_string(value);
				} 
				if (0 == rtype.compare("tesseract")) r = new RecognizerTesseract(jb); else
				if (0 == rtype.compare("needle")) r = new RecognizerNeedle(jb);
				if (!r) throw vz::OptionNotFoundException("recognizer type unknown!");
				_recognizer.push_back(r);
			}
		}else{
			throw vz::OptionNotFoundException("no recognizer given");
		}
	} catch (vz::OptionNotFoundException &e) {
		// recognizer is mandatory
		//print(log_error, "Config parameter 'recognizer' missing!", name().c_str());
		throw;
	} catch (vz::VZException &e) {
		print(log_error, "Failed to parse 'recognizer'", name().c_str());
		throw;
	}

}

void MeterOCR::Recognizer::saveDebugImage(PIXA *debugPixa, PIX *image, const char *title)
{
	if (debugPixa) pixSaveTiled(image, debugPixa, 1, 1, 1, 8);
	(void) title; // TODO use pixSaveTiledWithText
}

bool MeterOCR::RecognizerTesseract::recognize(PIX *imageO, int dX, int dY, ReadsMap &readings, PIXA *debugPixa){

	// init tesseract (TODO if we do this in the constructor we get corrupted readings after 3-4 read calls....) this is just a workaround
	if (!initTesseract()) return 0;

	PIX *image=pixClone(imageO);

	// now crop the image if possible:
	if (_min_x1>0 || _max_x2>0 || _min_y1>0 || _max_y2>0){
		int w = _max_x2>0 ? _max_x2 : pixGetWidth(image);
		w -= _min_x1;
		int h = _max_y2>0 ? _max_y2 : pixGetHeight(image);
		h -= _min_y1;
		print(log_error, "Cropping image to (%d,%d)x(%d,%d)", "RecognizerTesseract", _min_x1, _min_y1, w, h);
		PIX *image2 = pixCreate(w, h, pixGetDepth(image));
		pixCopyResolution(image2, image);
		pixCopyColormap(image2, image);
		pixRasterop(image2, 0, 0, w, h, PIX_SRC, image, _min_x1+dX, _min_y1+dY);
		pixDestroy(&image);
		image=image2;
		saveDebugImage(debugPixa, image, "cropped");
	}

	// Convert the RGB image to grayscale
	Pix *image_gs = pixConvertRGBToLuminance(image);
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		saveDebugImage(debugPixa, image, "grayscale");
	}

	// Increase the dynamic range
	// make dark gray *black* and light gray *white*
	image = pixGammaTRC(image, image, _gamma, _gamma_min, _gamma_max);
	if (image){
		saveDebugImage(debugPixa, image, "gamma");
	}

	//image_gs = pixCloseGray(image, 25, 25);
	image_gs = pixUnsharpMaskingGray( image, 3, 0.5 ); // TODO make a parameter, only useful for blurred images
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		saveDebugImage(debugPixa, image, "unsharp");
	}


	// Normalize for uneven illumination on gray image
	Pix *pixg=0;
	pixBackgroundNormGrayArrayMorph(image, NULL, 4, 5, 200, &pixg);
	image_gs = pixApplyInvBackgroundGrayMap(image, pixg, 4, 4);
	pixDestroy(&pixg);
	
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		saveDebugImage(debugPixa, image, "normalize");
	}

	image_gs = pixBlockconv(image, 1, 1);
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		saveDebugImage(debugPixa, image, "blockconv");
	}

	// Threshold to 1 bpp
	image_gs = pixThresholdToBinary(image, 120);
	if (image_gs){
		pixDestroy(&image);
		image = image_gs;
		image_gs = 0;
		saveDebugImage(debugPixa, image, "binary");
	}
	
	api->SetImage(image);
	
	Pix *dump = api->GetThresholdedImage();
//	outfilename=_file;
//	outfilename.append("thresh.tif");
//    pixWrite(outfilename.c_str(), dump, IFF_TIFF_G4);

    // get picture size:
    int width = pixGetWidth(image);
    int height = pixGetHeight(image);
    print(log_error, "Image size width=%d, height=%d\n", "RecognizerTesseract", width, height);
	

	BOXA *boxa = boxaCreate(_boxes.size()); // there can be more, just initial number
	BOXA *boxb = boxaCreate(_boxes.size());
	
	// add autofix target to boxb (blue) and found to boxa(green)
	/*
	if (_autofix_range>0){
		int cx = _autofix_x - _min_x1 - autofix_dX;
		int cy = _autofix_y - _min_y1 - autofix_dY;
		boxaAddBox(boxb, boxCreate(cx - _autofix_range, cy - _autofix_range, 2* _autofix_range, 2*_autofix_range), L_INSERT);
		if (_autofix_range>1){ // add the center
			boxaAddBox(boxb, boxCreate(cx,cy, 1, 1), L_INSERT);
		}
		boxaAddBox(boxa, boxCreate(cx+autofix_dX, cy+autofix_dY, 1, 1), L_INSERT);
	}
	*/

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

		if (api->Recognize(0)==0){
			std::string outtext;
			tesseract::ResultIterator* ri = api->GetIterator();
			tesseract::PageIteratorLevel level = tesseract::RIL_WORD;
			double min_conf=DBL_MAX;
			if (ri != 0){
				do{
					const char* word = ri->GetUTF8Text(level);
					float conf = ri->Confidence(level);
					int x1, y1, x2, y2;
					ri->BoundingBox(level, &x1, &y1, &x2, &y2);
					print(log_error, "word: '%s'; \tconf: %.2f; BoundingBox: %d,%d,%d,%d;\n", "RecognizerTesseract",
						word, conf, x1, y1, x2, y2);
					if (conf>15.0 && outtext.length()==0){
						outtext=word; // TODO choose the one with highest confidence? or ignore if more than 1?
						if (conf<min_conf) min_conf=conf;
					}
					if (word) delete[] word;	

					// for debugging draw the box in the picture:
					BOX *box=boxCreate(x1, y1, x2-x1, y2-y1);
					boxaAddBox(boxa, box, L_INSERT);
				} while (ri->Next(level));
				delete ri;
			}
			print(log_error, "%s=%s", "RecognizerTesseract", b.identifier.c_str(), outtext.c_str());
		
			if (b.conf_id.length()) readings[b.identifier].conf_id=b.conf_id;
		
			// if we couldn't read any text mark this as not available (using NAN (not a number))
			if (outtext.length()==0){
				readings[b.identifier].value = NAN;
				readings[b.identifier].min_conf = 0;
			}else{
				readings[b.identifier].value += strtod(outtext.c_str(), NULL) * pow(10, b.scaler);
				if (min_conf<readings[b.identifier].min_conf) readings[b.identifier].min_conf = min_conf;
				outtext.clear();
			}
		}
	}

	Pix *bbpix2 = pixDrawBoxa(dump, boxa, 1, 0x00ff0000); // rgba color -> green, detected
	Pix *bbpix = pixDrawBoxa(bbpix2, boxb, 1, 0x0000ff00); // blue = bounding box for search
	pixDestroy(&bbpix2);
	boxaDestroy(&boxa);
	boxaDestroy(&boxb);
	saveDebugImage(debugPixa, bbpix, "bb");
	pixDestroy(&bbpix);

	pixDestroy(&dump);
	pixDestroy(&image);
	return true;
}

bool MeterOCR::RecognizerTesseract::initTesseract()
{
	if (api) deinitTesseract(); // we want to deinit/init in this case on purpose! (see TODO in read)
	
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
		print(log_error, "Could not init tesseract!", "RecognizerTesseract");
		throw vz::VZException("could not init tesseract");
	}
	
	api->SetVariable("tessedit_char_whitelist", "0123456789.m"); // TODO think about removing 'm' (should not be within the boundingboxes)
	api->SetPageSegMode(_all_digits ? tesseract::PSM_SINGLE_CHAR : tesseract::PSM_SINGLE_BLOCK); // PSM_SINGLE_WORD);
	
	return true;
}

bool MeterOCR::RecognizerTesseract::deinitTesseract()
{
	if (!api) return false;
	api->End();
	delete api;
	api = 0;
	return true;
}

MeterOCR::RecognizerTesseract::~RecognizerTesseract()
{
	// end tesseract usage:
	deinitTesseract();
}

MeterOCR::~MeterOCR() {
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



ssize_t MeterOCR::read(std::vector<Reading> &rds, size_t max_reads) {

	unsigned int i = 0;
	std::string outfilename;
	std::string id;
	print(log_debug, "MeterOCR::read: %d, %d", name().c_str(), rds.size(), max_reads);

	if (max_reads<1) return 0;
	
	PIXA *debugPixa=pixaCreate(0);
	
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
		// for debugging to see the rotated picture:
		if (debugPixa) pixSaveTiled(image, debugPixa, 1, 0, 1, 32);
		// TODO double check with pixFindSkew? (auto-rotate?)		
	}else{
		// add a small version of the input image:
		if (debugPixa) pixSaveTiled(image, debugPixa, 1, 0, 1, 32);
	}

	// now do autofix detection:
	// this works by scanning for two edges and moving the image relatively so that the two edges cross inside the (x/y) position
	// We do this before cropping
	// and later crop again towards the detected center
	int autofix_dX=0, autofix_dY=0;
	if (_autofix_range>0){
		// TODO add search direction. now we do from left to right and from bottom to top
		// TODO add parameter for edge intensity/threshold
		autofixDetection(image, autofix_dX, autofix_dY, debugPixa);
	}

	ReadsMap readings;
	// now call each recognizer and let them do their part:
	for (std::list<Recognizer*>::iterator it = _recognizer.begin(); it != _recognizer.end(); ++it){ // let's stick to begin not cbegin (c++11)
		if (*it)
			(*it)->recognize(image, autofix_dX, autofix_dY, readings, debugPixa);
	}
	
	pixDestroy(&image);
	if (debugPixa && pixaGetCount(debugPixa)>0){
		// todo output debugpix:
		PIX *pixt = pixaDisplay(debugPixa, 0, 0);
		std::string outfilename = _file;
		outfilename.append("_debug.png");
		pixWrite(outfilename.c_str(), pixt, IFF_PNG);
		pixDestroy(&pixt);
		pixaDestroy(&debugPixa);
	}

	// return all readings:
	for(std::map<std::string, Reads>::iterator it = readings.begin(); it!= readings.end(); ++it){
		const Reads &r=it->second;
		print(log_error, "returning: id <%s> value <%f>", name().c_str(), it->first.c_str(), r.value);
		if (!isnan(r.value)){
			rds[i].value(r.value);
			rds[i].identifier(new StringIdentifier(it->first));
			rds[i].time();
			i++;
			if (i>=max_reads) break;
		}
		if (r.conf_id.length()>0){
			rds[i].value(r.min_conf);
			rds[i].identifier(new StringIdentifier(r.conf_id));
			rds[i].time();
			i++;
			if (i>=max_reads) break;
		}
	}

	return i;
}

bool MeterOCR::autofixDetection(Pix *image_org, int &dX, int&dY, PIXA *debugPixa)
{
	std::string outfilename;

	// now do autofix detection:
	// this works by scanning for two edges and moving the image relatively so that the two edges cross inside the (x/y) position
	// We do this before cropping
	// and later crop again towards the detected center
	if (_autofix_range>0){		
		int w = 2*_autofix_range+1;
		// 1.step: crop the small detection area
		PIX *img = pixCreate(w, w, pixGetDepth(image_org));
		pixCopyResolution(img, image_org);
		pixCopyColormap(img, image_org);
		pixRasterop(img, 0, 0, w, w, PIX_SRC, image_org, _autofix_x-_autofix_range, _autofix_y-_autofix_range);		
		// don't destroy image, we don't take the ownership
		
		// 2nd step: change to grayscale
		Pix *image_gs = pixConvertRGBToLuminance(img);
		if (image_gs){
			pixDestroy(&img);
			if (debugPixa) pixSaveTiledWithText(image_gs, debugPixa, w, 1, 10, 1, 0, "autofix", 0xff000000, L_ADD_BELOW);
		}
		
		// 3rd step pixTwoSidedEdgeFilter
		Pix *imgEdgeV = pixTwoSidedEdgeFilter(image_gs, L_VERTICAL_EDGES);
		Pix *imgEdgeH = pixTwoSidedEdgeFilter(image_gs, L_HORIZONTAL_EDGES);
		pixDestroy(&image_gs);
		if (!imgEdgeV || !imgEdgeH) {
			if (imgEdgeV) pixDestroy(&imgEdgeV);
			if (imgEdgeH) pixDestroy(&imgEdgeH);
			return false;
		}

		// 4th step: pixThresholdToBinary
		Pix *imgBinV = pixThresholdToBinary(imgEdgeV, 40);
		pixDestroy(&imgEdgeV);
		Pix *imgBinH = pixThresholdToBinary(imgEdgeH, 40);
		pixDestroy(&imgEdgeH);
		
		// 5th step: determine min for pixGetLastOffPixel
		int minOnX = w; int minOnY = -1;
		for (int i=0; i<w; ++i){
			int mX=w, mY=w;
			pixGetLastOnPixelInRun(imgBinV, 0, i, L_FROM_LEFT, &mX);
			pixGetLastOnPixelInRun(imgBinH, i, w-1, L_FROM_BOT, &mY);			
			if (mX<minOnX) minOnX=mX;
			if (mY>minOnY) minOnY=mY;
		}
		pixDestroy(&imgBinV);
		pixDestroy(&imgBinH);
		
		if (minOnX>=0 && minOnX<w && minOnY>=0 && minOnY<w){
			dX = - _autofix_range + minOnX;
			dY = - _autofix_range + minOnY;
			print(log_error, "autofixDetection: dX=%d, dY=%d\n", name().c_str(), dX, dY);
			return true;
		}
		print(log_error, "autofixDetection: not found!\n", name().c_str());
	}
	return false;
}

