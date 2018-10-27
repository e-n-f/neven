#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>

/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <fd_emb_sdk.h>

btk_HDCR gdcr;
btk_HSDK gsdk;
btk_HFaceFinder gfd;

static void *amalloc(unsigned int size) {
	return malloc(size);
}

static void
initialize(int w, int h) {
	// char *path = "Embedded/common/data/APIEm/Modules/RFFstd_501.bmd";
	char *path = "/usr/local/share/RFFprec_501.bmd";

	const int MAX_FILE_SIZE = 65536;
	void *initData = malloc(MAX_FILE_SIZE); /* enough to fit entire file */
	int filedesc = open(path, O_RDONLY);
	int initDataSize = read(filedesc, initData, MAX_FILE_SIZE);
	close(filedesc);
	// printf("got %d of data\n", initDataSize);

	// --------------------------------------------------------------------
	btk_HSDK sdk = NULL;
	btk_SDKCreateParam sdkParam = btk_SDK_defaultParam();
	sdkParam.fpMalloc = amalloc;
	sdkParam.fpFree = free;
	sdkParam.maxImageWidth = w;
	sdkParam.maxImageHeight = h;

	btk_Status status = btk_SDK_create(&sdkParam, &sdk);
	// make sure everything went well
	if (status != btk_STATUS_OK) {
		fprintf(stderr, "couldn't read %s or otherwise start up\n", path);
		return;
	}

	btk_HDCR dcr = NULL;
	btk_DCRCreateParam dcrParam = btk_DCR_defaultParam();
	btk_DCR_create(sdk, &dcrParam, &dcr);

	btk_HFaceFinder fd = NULL;
	btk_FaceFinderCreateParam fdParam = btk_FaceFinder_defaultParam();
	fdParam.pModuleParam = initData;
	fdParam.moduleParamSize = initDataSize;
	fdParam.maxDetectableFaces = 64;
	status = btk_FaceFinder_create(sdk, &fdParam, &fd);
	btk_FaceFinder_setRange(fd, 20, w / 2); /* set eye distance range */

	// make sure everything went well
	if (status != btk_STATUS_OK) {
		// XXX: be more precise about what went wrong
		fprintf(stderr, "couldn't read %s or otherwise initialize\n", path);
		return;
	}

	// free the configuration file
	free(initData);

	gfd = fd;
	gsdk = sdk;
	gdcr = dcr;

	return;
}

static void
destroy() {
	btk_FaceFinder_close(gfd);
	btk_DCR_close(gdcr);
	btk_SDK_close(gsdk);
}

typedef unsigned char uint8_t;

void output(unsigned char *image, int width, int height, int depth, char *filename,
	    int left, int top, int right, int bottom) {
	static char oldfile[1000] = "";

	char fname[5000];
	int quality = 75;
	static int count = 0;

	printf("%s vs %s %d\n", oldfile, filename, strcmp(filename, oldfile));

	char *base = filename;
	char *cp;
	for (cp = filename; *cp; cp++) {
		if (*cp == '/') {
			base = cp + 1;
		}
	}

	//sprintf(fname, "out/%04d.jpg", count++);
	mkdir("out", 0777);
	if (strcmp(filename, oldfile) == 0) {
		char tmpfile[1000];
		strcpy(tmpfile, base);

		for (cp = tmpfile; *cp; cp++) {
			if (*cp == '.') {
				*cp = '\0';
				break;
			}
		}

		count++;
		sprintf(fname, "out/%s-%d.jpg", tmpfile, count);
	} else {
		sprintf(fname, "out/%s", base);
		count = 0;
	}

	strcpy(oldfile, filename);

	printf("write %s\n", fname);

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *outfile;		 /* target file */
	JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
	int row_stride;		 /* physical row width in image buffer */

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	if ((outfile = fopen(fname, "wb")) == NULL) {
		fprintf(stderr, "can't open %s\n", fname);
		exit(1);
	}
	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = right - left; /* image width and height, in pixels */
	cinfo.image_height = bottom - top;
	cinfo.input_components = depth; /* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB; /* colorspace of input image */
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

	jpeg_start_compress(&cinfo, TRUE);

	row_stride = (right - left) * depth; /* JSAMPLEs per row in image_buffer */

	int y = top;
	while (cinfo.next_scanline < cinfo.image_height) {
		/* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
		row_pointer[0] = &image[y * depth * width + left * depth];
		//row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
		y++;
	}

	/* Step 6: Finish compression */

	jpeg_finish_compress(&cinfo);
	/* After finish_compress, we can close the output file. */
	fclose(outfile);

	/* Step 7: release JPEG compression object */

	/* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_compress(&cinfo);

	/* And we're done! */
}

float dist(float x1, float y1, float x2, float y2) {
	float dx = x2 - x1;
	float dy = y2 - y1;

	return sqrt((dx * dx) + (dy * dy));
}

void produce(unsigned char *image, int width, int height, int depth, char *filename,
	     int left, int top, int right, int bottom, int x1, int y1, int x2, int y2, int x3, int y3) {
	// printf("processing %d by %d, depth %d\n", width, height, depth);

	// printf("%d,%d %d,%d %d,%d\n", x1, y1, x2, y2, x3, y3);

	int nheight = (y3 - y1) * 2;
	int nwidth = nheight;

	int yd = y1 - (y3 - y1) / 2;
	int xd = (x1 + x2) / 2 - nwidth / 2;

	printf("%dx%d+%d+%d %s\n", nwidth, nheight, xd, yd, filename);
	return;

	// printf("%d,%d %d,%d\n", nwidth, nheight, xd, yd);

	unsigned char outbuf[nwidth * nheight * depth];

	for (int y = 0; y < nheight; y++) {
		for (int x = 0; x < nwidth; x++) {
			int origx = x + xd;
			int origy = y + yd;

			if (origx < 0 || origx >= width) {
				origx = 0;
				origy = 0;
			}
			if (origy < 0 || origy >= height) {
				origx = 0;
				origy = 0;
			}

			int origoff = (int) origx * depth +
				      (int) origy * width * depth;

			int off = x * depth + y * nwidth * depth;

			for (int layer = 0; layer < depth; layer++) {
				outbuf[off + layer] = image[origoff + layer];
			}
		}
	}

	output(outbuf, nwidth, nheight, depth, filename, 0, 0, nwidth, nheight);

	return;
}

static int
detect(unsigned char *bitmap, int width, int height) {
	// get the fields we need
	btk_HDCR hdcr = gdcr;
	btk_HFaceFinder hfd = gfd;

	// get to our BW temporary buffer
	unsigned char *bwbuffer = malloc(width * height);

	// convert the image to B/W
	uint8_t *dst = (uint8_t *) bwbuffer;

	unsigned char const *src = bitmap;
	int wpr = width * 3;
	int x, y;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int r = src[x * 3];
			int g = src[x * 3 + 1];
			int b = src[x * 3 + 2];

			// L coefficients 0.299 0.587 0.11
			//int L = (r<<1) + (g2<<1) + (g2>>1) + b;
			int L = r * .299 + g * .587 + b * .11;
			*dst++ = L;
		}
		src += wpr;
	}

	// run detection
	btk_DCR_assignGrayByteImage(hdcr, bwbuffer, width, height);

	int numberOfFaces = 0;
	if (btk_FaceFinder_putDCR(hfd, hdcr) == btk_STATUS_OK) {
		numberOfFaces = btk_FaceFinder_faces(hfd);
	}

	// release the arrays we're using
	free(bwbuffer);
	return numberOfFaces;
}

#if 0
static void
get_face(JNIEnv *_env, jobject _this,
     jobject face, jint index)
{
    btk_HDCR hdcr = (btk_HDCR)(_env->GetIntField(_this, gFaceDetectorOffsets.dcr));
    btk_HFaceFinder hfd =
        (btk_HFaceFinder)(_env->GetIntField(_this, gFaceDetectorOffsets.fd));

    FaceData faceData;
    btk_FaceFinder_getDCR(hfd, hdcr);
    getFaceData(hdcr, &faceData);

    const float X2F = 1.0f / 65536.0f;
    _env->SetFloatField(face, gFaceOffsets.confidence,  faceData.confidence);
    _env->SetFloatField(face, gFaceOffsets.midpointx,   faceData.midpointx);
    _env->SetFloatField(face, gFaceOffsets.midpointy,   faceData.midpointy);
    _env->SetFloatField(face, gFaceOffsets.eyedist,     faceData.eyedist);
    _env->SetFloatField(face, gFaceOffsets.eulerx,      0);
    _env->SetFloatField(face, gFaceOffsets.eulery,      0);
    _env->SetFloatField(face, gFaceOffsets.eulerz,      0);
}
#endif

void report(char *s, int i, unsigned char *image, int width, int height, int depth, char *filename) {
	btk_HDCR hdcr = gdcr;
	btk_HFaceFinder hfd = gfd;
	btk_Node leftEye, rightEye, mouth;

	btk_FaceFinder_getDCR(hfd, hdcr);

	btk_DCR_getNode(gdcr, 0, &leftEye);
	btk_DCR_getNode(gdcr, 1, &rightEye);
	btk_DCR_getNode(gdcr, 2, &mouth);

	float x1 = (float) (leftEye.x) / (1 << 16);
	float y1 = (float) (leftEye.y) / (1 << 16);

	float x2 = (float) (rightEye.x) / (1 << 16);
	float y2 = (float) (rightEye.y) / (1 << 16);

	float x3 = (float) (mouth.x) / (1 << 16);
	float y3 = (float) (mouth.y) / (1 << 16);

#if 0
	int n;
	for (n = 2; n < 20; n++) {
    btk_DCR_getNode(gdcr, n, &leftEye);

	printf("node %d %f,%f\n", n,
(float)(leftEye.x) / (1 << 16),
(float)(leftEye.y) / (1 << 16));

	}
#endif

	produce(image, width, height, depth, filename, 0, 0, width, height,
		x1, y1, x2, y2, x3, y3);

	//fdata->eyedist = (float)(rightEye.x - leftEye.x) / (1 << 16);
	//fdata->midpointx = (float)(rightEye.x + leftEye.x) / (1 << 17);
	//fdata->midpointy = (float)(rightEye.y + leftEye.y) / (1 << 17);
	//fdata->confidence = (float)btk_DCR_confidence(hdcr) / (1 << 24);
}

void process(unsigned char *image, int width, int height, int depth, char *filename,
	     int left, int top, int right, int bottom) {
	// printf("\n\nprocessing %d by %d, depth %d in %s\n", width, height, depth, filename);

	initialize(width, height);
	if (width > 5000 || height > 5000) {
		printf("too big\n");
		return;
	}

	int count = detect(image, width, height);
	// printf("found %d\n", count);
	int i;

	for (i = 0; i < count; i++) {
		report(filename, i, image, width, height, depth, filename);
	}

	if (i == 0) {
		printf("# %s\n", filename);
	}

	fflush(stdout);
	destroy();
}

void process2(unsigned char *image, int width, int height, int depth, char *filename,
	      int left, int top, int right, int bottom) {
	int nwidth, nheight;

	nwidth = 320;
	nheight = height * 320 / width;

	if (nheight > 240) {
		nheight = 240;
		nwidth = width * 240 / height;
	}

	printf("scale to %d by %d\n", nwidth, nheight);
	unsigned char outbuf[320 * 240 * depth];

	int x, y, layer;
	for (y = 0; y < nheight; y++) {
		for (x = 0; x < nwidth; x++) {
			int origx = x * width / nwidth;
			int origy = y * height / nheight;

			int origoff = origx * depth +
				      origy * width * depth;

			int off = x * depth + y * nwidth * depth;

			for (layer = 0; layer < depth; layer++) {
				outbuf[off + layer] = image[origoff + layer];
			}
		}
	}

	output(outbuf, nwidth, nheight, depth, filename, 0, 0, nwidth, nheight);

	return;
}

void read_JPEG_file(char *filename) {
	struct jpeg_decompress_struct cinfo;
	FILE *infile;      /* source file */
	JSAMPARRAY buffer; /* Output row buffer */
	int row_stride;    /* physical row width in output buffer */

	unsigned char *image;
	unsigned char *where;

	if ((infile = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "can't open %s\n", filename);
		return;
	}

	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);

	(void) jpeg_read_header(&cinfo, TRUE);
	(void) jpeg_start_decompress(&cinfo);

	/* JSAMPLEs per row in output buffer */
	row_stride = cinfo.output_width * cinfo.output_components;
	/* Make a one-row-high sample array that will go away when done with image */
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	image = malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);
	if (image == NULL) {
		fprintf(stderr, "failed to allocate memory\n");
		exit(EXIT_FAILURE);
	}
	where = image;

	while (cinfo.output_scanline < cinfo.output_height) {
		(void) jpeg_read_scanlines(&cinfo, buffer, 1);

		memcpy(where, buffer[0], row_stride);
		where += row_stride;
	}

	(void) jpeg_finish_decompress(&cinfo);

	process(image, cinfo.output_width, cinfo.output_height, cinfo.output_components, filename,
		0, 0, cinfo.output_width, cinfo.output_height);
	free(image);

	jpeg_destroy_decompress(&cinfo);
	fclose(infile);
}

int main(int argc, char **argv) {
	int i;

	for (i = 1; i < argc; i++) {
		// printf("%s\n", argv[i]);
		read_JPEG_file(argv[i]);
	}
}
