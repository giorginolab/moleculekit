#ifdef PLATFORM_Linux
#if defined(__i386__) || defined(__x86_64__)
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
#endif
#endif

/* (c) 2015-2016 Acellera Ltd www.acellera.com
 * All rights reserved
 * Distributed under the terms of the HTMD Software License Agreement
 * No redistribution in whole or in part
 */

#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "xdrfile.h"
#include "xdrfile_xtc.h"
#include "xtc.h"
#include <stdint.h>
#include <string.h>
//#include "rpc/types.h"
//#include "rpc/xdr.h"
//#include <linux/limits.h>

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

#include <libgen.h>
#include <sys/stat.h>

static void *condfree(void *p)
{
	if (p)
		free(p);
	return NULL;
}

#ifdef TEST
int safe_mode()
{
	return 0;
}

#endif

#define X(atom, frame, nframes, nf3) atom *nf3
#define Y(atom, frame, nframes, nf3) atom *nf3 + 1
#define Z(atom, frame, nframes, nf3) atom *nf3 + 2

// I redefine here for frames to optimize calculations in the single frame use case
#define Xf(atom, frame, nframes) atom * 3 * nframes + frame
#define Yf(xidx, nframes) xidx + nframes
#define Zf(yidx, nframes) yidx + nframes

// int resolve_filename( const char *logical, char *physical, int length );
/*
struct {
  float4 box;
  int natoms;
  unsigned long step;
  double time;
  double4 *pos;
} XTC_frame;
*/

int xtc_natoms(char *filename)
{
	int natoms = 0;
	if (exdrOK != read_xtc_natoms(filename, &natoms))
	{
		fprintf(stderr, "xtc_read(): could not get natoms\n");
		return -1;
	}
	return natoms;
}

int xtc_nframes(char *filename)
{
	char index_file[PATH_MAX + 1];
	char *f1 = strdup(filename);
	char *f2 = strdup(filename);
	sprintf(index_file, "%s/.%s", dirname(f1), basename(f2));
	free(f1);
	free(f2);

	struct stat st_index_file, st_traj_file;
	if ((stat(index_file, &st_index_file) == 0) && (stat(filename, &st_traj_file) == 0))
	{
		// If the index file is older than the trajectory file rewrite it
		// If the index file is size 0 rewrite it
		if ((st_index_file.st_size > 0) && (st_index_file.st_mtime > st_traj_file.st_mtime))
		{
			// The index file exists. Get numframes from it's size
			return (st_index_file.st_size) / sizeof(uint64_t);
		}
		else
		{
			unlink(index_file);
		}
	}

	int natoms = 0;
	int nframes = 0;
	rvec *p = NULL;
	XDRFILE *xd = NULL;
	float time;
	int step;
	float prec;
	matrix box;
	FILE *indexfn = NULL;

	int writing_index = 0;

	if (exdrOK != read_xtc_natoms(filename, &natoms))
	{
		fprintf(stderr, "xtc_read(): could not get natoms\n");
		return -1;
	}
	if (!natoms)
	{
		fprintf(stderr, "xtc_read(): natoms is 0\n");
		return -1;
	}

	xd = xdrfile_open(filename, "r");

	if (NULL == xd)
	{
		fprintf(stderr, "xtc_read(): could not open file\n");
		return -1;
	}

	p = (rvec *)malloc(sizeof(rvec) * natoms);

	indexfn = fopen(index_file, "r");

	if (!indexfn)
	{
		indexfn = fopen(index_file, "w");
		if (indexfn)
		{
			if (getenv("DEBUG"))
				fprintf(stderr, "xtc_read(): writing index [%s]\n", index_file);
			writing_index = 1;
		}
	}

	int retval = 0;
	uint64_t offset = 0;

	while (exdrOK == (retval = read_xtc(xd, natoms, &step, &time, box, p, &prec)))
	{
		if (writing_index)
		{
			fwrite(&(offset), sizeof(uint64_t), 1, indexfn);
		}

		nframes++;
		offset = ftell(xd->fp);
	}

	condfree((void *)p);
	p = NULL;

	xdrfile_close(xd);

	if (retval == exdr3DX)
	{
		fprintf(stderr, "xtc_read(): XTC file is corrupt\n");
		if (indexfn)
			fclose(indexfn);
		return -1;
	}

	if (indexfn)
		fclose(indexfn);

	if (indexfn)
	{
		// Always try to make the index file globally readable
		// so bob can read alice's indices
		chmod(index_file, 0644);
	}

	return nframes;
}

void xtc_read_new(char *filename, float *coords_arr, float *box_arr, float *time_arr, int *step_arr, int natoms, int nframes)
{
	rvec *p = NULL;
	XDRFILE *xd = NULL;
	float time;
	int step;
	float prec;
	matrix box;
	int nf3 = nframes * 3; // Precalculate 3 * nframes for the coordinate lookup macro

	if (!natoms)
	{
		fprintf(stderr, "xtc_read(): natoms is 0\n");
		return;
	}

	xd = xdrfile_open(filename, "r");

	if (NULL == xd)
	{
		fprintf(stderr, "xtc_read(): could not open file\n");
		return;
	}

	p = (rvec *)malloc(sizeof(rvec) * natoms);

	int retval = 0;
	int fidx = 0;
	int _natoms_garbage = 0;
	int xidx, yidx, zidx, aidx;
	while (exdrOK == (retval = read_xtc(xd, _natoms_garbage, &step, &time, box, p, &prec)))
	{
		time_arr[fidx] = time;
		step_arr[fidx] = step;

		box_arr[fidx] = box[0][0];
		box_arr[fidx + nframes] = box[1][1];
		box_arr[fidx + 2 * nframes] = box[2][2];

		for (aidx = 0; aidx < natoms; aidx++)
		{
			xidx = Xf(aidx, fidx, nframes);
			yidx = Yf(xidx, nframes);
			zidx = Zf(yidx, nframes);
			coords_arr[xidx] = p[aidx][0];
			coords_arr[yidx] = p[aidx][1];
			coords_arr[zidx] = p[aidx][2];
		}

		fidx++;
	}

	condfree((void *)p);
	p = NULL;

	xdrfile_close(xd);

	if (retval == exdr3DX)
	{
		fprintf(stderr, "xtc_read(): XTC file is corrupt\n");
	}
}

struct XTC_frame *xtc_read(char *filename, int *natoms, int *nframes, double *dt, int *dstep)
{
	struct XTC_frame *frames = NULL;
	*natoms = 0;
	*nframes = 0;
	rvec *p = NULL;
	XDRFILE *xd = NULL;
	float time;
	int step;
	float prec;
	matrix box;
	int i;
	FILE *indexfn = NULL;

	char index_file[PATH_MAX + 1];
	int writing_index = 0;

	char *f1, *f2;

	f1 = strdup(filename);
	f2 = strdup(filename);
	sprintf(index_file, "%s/.%s", dirname(f1), basename(f2));
	free(f1);
	free(f2);

	if (exdrOK != read_xtc_natoms(filename, natoms))
	{
		fprintf(stderr, "xtc_read(): could not get natoms\n");
		return NULL;
	}
	if (!*natoms)
	{
		fprintf(stderr, "xtc_read(): natoms is 0\n");
		return NULL;
	}

	xd = xdrfile_open(filename, "r");

	if (NULL == xd)
	{
		fprintf(stderr, "xtc_read(): could not open file\n");
		return NULL;
	}

	p = (rvec *)malloc(sizeof(rvec) * *natoms);

	indexfn = fopen(index_file, "r");

	if (!indexfn)
	{
		indexfn = fopen(index_file, "w");
		if (indexfn)
		{
			if (getenv("DEBUG"))
				fprintf(stderr, "xtc_read(): writing index [%s]\n", index_file);
			writing_index = 1;
		}
	}

	/*
 extern int read_xtc(XDRFILE *xd,int natoms,int *step,float *time,
		  matrix box,rvec *x,float *prec);
*/
	int retval = 0;
	uint64_t offset = 0;

	while (exdrOK == (retval = read_xtc(xd, *natoms, &step, &time, box, p, &prec)))
	{
		frames = (struct XTC_frame *)realloc(frames, sizeof(struct XTC_frame) * (*nframes + 1));

		if (writing_index)
		{
			fwrite(&(offset), sizeof(uint64_t), 1, indexfn);
		}

		if (!(frames))
		{
			fprintf(stderr, "xtc_read(): Allocation error in xtc.c (1). nframes=%d natoms=%d\n", *nframes, *natoms);
			if (indexfn)
				fclose(indexfn);
			return NULL;
		}
		//		printf( "XTC: %f %d\n", time, step );
		frames[*nframes].time = time;
		frames[*nframes].step = step;
		frames[*nframes].box[0] = box[0][0];
		frames[*nframes].box[1] = box[1][1];
		frames[*nframes].box[2] = box[2][2];
		frames[*nframes].pos = (float *)malloc(sizeof(float) * 3 * *natoms);

		if (!(frames[*nframes].pos))
		{
			fprintf(stderr, "xtc_read(): Allocation error in xtc.c (2). nframes=%d natoms=%d\n", *nframes, *natoms);
			if (indexfn)
				fclose(indexfn);
			return NULL;
		}

		float *pp = frames[*nframes].pos;

		for (i = 0; i < *natoms; i++)
		{
			pp[i * 3 + 0] = p[i][0];
			pp[i * 3 + 1] = p[i][1];
			pp[i * 3 + 2] = p[i][2];
		}

		(*nframes)++;
		offset = ftell(xd->fp);
	}

	*dt = 0.;
	*dstep = 0;
	if (*nframes >= 2)
	{
		*dt = frames[1].time - frames[0].time;
		*dstep = frames[1].step - frames[0].step;
	}

	condfree((void *)p);
	p = NULL;

	xdrfile_close(xd);

	if (retval == exdr3DX)
	{
		fprintf(stderr, "xtc_read(): XTC file is corrupt\n");
		condfree((void *)frames);
		frames = NULL;
		if (indexfn)
			fclose(indexfn);
		return (NULL);
	}

	if (indexfn)
		fclose(indexfn);

	if (!frames)
	{
		fprintf(stderr, "xtc_read(): no frames read\n");
	}

	if (indexfn)
	{
		// Always try to make the index file globally readable
		// so bob can read alice's indices
		chmod(index_file, 0644);
	}

	return frames;
}

int xtc_write(char *filename, int natoms, int nframes, int *step, float *timex, float *pos, float *box)
{
	XDRFILE *xd = NULL;
	rvec *p = NULL;
	int i, f;
	int xidx, yidx, zidx;
	matrix b;
	float prec = 1000;
	int nf3 = nframes * 3;

	char *f1, *f2;

	// if( step <=0 ) {
	//	xd = xdrfile_open( filename, "w" );
	// }
	// else {

	// Invalidate any index file
	char index_file[PATH_MAX + 1];
	f1 = strdup(filename);
	f2 = strdup(filename);
	sprintf(index_file, "%s/.%s", dirname(f1), basename(f2));
	unlink(index_file);
	free(f1);
	free(f2);

	xd = xdrfile_open(filename, "a");
	//}

	if (!xd)
	{
		return 1;
	}

	for (f = 0; f < nframes; f++)
	{
		p = (rvec *)malloc(sizeof(rvec) * natoms * 3);

		b[0][0] = box[0 * nframes + f];
		b[0][1] = 0.;
		b[0][2] = 0.;

		b[1][0] = 0.;
		b[1][1] = box[1 * nframes + f];
		b[1][2] = 0.;

		b[2][0] = 0.;
		b[2][1] = 0.;
		b[2][2] = box[2 * nframes + f];
		// printf("Boxes: %f %f %f\n", b[0][0], b[1][1], b[2][2]);

		for (i = 0; i < natoms; i++)
		{
			xidx = Xf(i, f, nframes);
			yidx = Yf(xidx, nframes);
			zidx = Zf(yidx, nframes);
			p[i][0] = pos[xidx];
			p[i][1] = pos[yidx];
			p[i][2] = pos[zidx];
			// printf("Coords %d: %f %f %f\n", i, pos[Xf(i,f,nframes,nf3)], pos[Yf(i,f,nframes,nf3)], pos[Zf(i,f,nframes,nf3)]);
		}
		// printf("Frame: %d Step: %d Time: %f\n", f, step[f], timex[f]);
		write_xtc(xd, natoms, (unsigned int)step[f], (float)timex[f], b, p, prec);

		condfree((void *)p);
		p = NULL;
	}
	xdrfile_close(xd);

	return 0;
}

void xtc_read_frame(char *filename, float *coords_arr, float *box_arr, float *time_arr, int *step_arr, int natoms, int frame, int nframes, int fidx)
{
	struct XTC_frame *frames = NULL;
	rvec *p = NULL;
	XDRFILE *xd = NULL;
	float time;
	int step;
	float prec;
	matrix box;
	int i;
	FILE *indexfn = NULL;

	char index_file[PATH_MAX + 1];

	int reading_index = 0;
	int writing_index = 0;

	if (frame < 0)
	{
		fprintf(stderr, "xtc_read_frame(): Frame <0\n");
		return;
	}

	char *f1 = strdup(filename);
	char *f2 = strdup(filename);
	sprintf(index_file, "%s/.%s", dirname(f1), basename(f2));
	free(f1);
	free(f2);

	if (getenv("DEBUG"))
	{
		fprintf(stderr, "Looking for index file [%s]\n", index_file);
	}

	{
		struct stat st;
		if (stat(index_file, &st) == 0)
		{
			if (0 == st.st_size)
			{ // The index file exists  but is zero length. Delete it
				unlink(index_file);
			}
		}
	}

	indexfn = fopen(index_file, "r");

	if (!indexfn)
	{
		if (getenv("DEBUG"))
			fprintf(stderr, "xtc_read_frame():Reading using whole file\n");

		// REAd the whole thing and return the selected frame
		int traj_nframes;
		double dt;
		int dstep;
		int *garbage_natoms;

		//	printf("reading whole file\n" );
		frames = xtc_read(filename, garbage_natoms, &traj_nframes, &dt, &dstep);
		int xidx, yidx, zidx, aidx;
		if (frame < traj_nframes)
		{
			for (i = 0; i < traj_nframes; i++)
			{
				if (i != frame)
				{
					free(frames[i].pos);
				}
			}
			time_arr[fidx] = frames[frame].time;
			step_arr[fidx] = frames[frame].step;
			box_arr[fidx] = frames[frame].box[0];
			box_arr[fidx + nframes] = frames[frame].box[1];
			box_arr[fidx + 2 * nframes] = frames[frame].box[2];
			for (aidx = 0; aidx < natoms; aidx++)
			{
				xidx = Xf(aidx, fidx, nframes);
				yidx = Yf(xidx, nframes);
				zidx = Zf(yidx, nframes);
				coords_arr[xidx] = frames[frame].pos[aidx * 3 + 0];
				coords_arr[yidx] = frames[frame].pos[aidx * 3 + 1];
				coords_arr[zidx] = frames[frame].pos[aidx * 3 + 2];
			}

			if (!frames)
			{
				fprintf(stderr, "xtc_read_frame(): failure to read file (whole file path)\n");
			}
			return;
		}
	}
	else
	{
		uint64_t len;
		if (getenv("DEBUG"))
			fprintf(stderr, "xtc_read_frame():Reading using index\n");

		if (exdrOK != read_xtc_natoms(filename, &natoms))
		{
			fprintf(stderr, "xtc_read_frame(): failed to call read_xtc_natoms (index path) [%s]\n", filename);
			return;
		}
		if (!natoms)
		{
			fprintf(stderr, "xtc_read_frame(): natoms invalid (index path)\n");
			return;
		}

		fseek(indexfn, frame * sizeof(uint64_t), SEEK_SET);
		int ret = fread(&len, sizeof(uint64_t), 1, indexfn);
		if (1 != ret)
		{
			fclose(indexfn);
			fprintf(stderr, "xtc_read_frame(): Could not read index [%d] of index file (index path) ret=%d errno=%d\n", frame, ret, errno);
			// Maybe the index is corrupted? remove it, to be safe
			unlink(index_file);
			return;
		}
		fclose(indexfn);

		xd = xdrfile_open(filename, "r");
		if (NULL == xd)
		{
			fprintf(stderr, "xtc_read_frame(): Could not open file [%s] (index path)\n", filename);
			return;
		}

		// MOve to offest of the single frame
		if (0 != fseek(xd->fp, len, SEEK_SET))
		{
			fprintf(stderr, "xtc_read_frame(): Failed to seek: errno=%d\n", errno);
		}

		p = (rvec *)malloc(sizeof(rvec) * natoms);

		int xidx, yidx, zidx, aidx;
		if (exdrOK == (read_xtc(xd, natoms, &step, &time, box, p, &prec)))
		{
			time_arr[fidx] = time;
			step_arr[fidx] = step;
			box_arr[fidx] = box[0][0];
			box_arr[fidx + nframes] = box[1][1];
			box_arr[fidx + 2 * nframes] = box[2][2];
			for (aidx = 0; aidx < natoms; aidx++)
			{
				xidx = Xf(aidx, fidx, nframes);
				yidx = Yf(xidx, nframes);
				zidx = Zf(yidx, nframes);
				coords_arr[xidx] = p[aidx][0];
				coords_arr[yidx] = p[aidx][1];
				coords_arr[zidx] = p[aidx][2];
			}
		}
	}

	condfree((void *)p);
	p = NULL;

	xdrfile_close(xd);

	return;
}