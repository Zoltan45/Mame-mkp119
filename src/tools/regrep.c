/***************************************************************************

    Regression test report generator

    Copyright (c) 1996-2007, Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "osdcore.h"
#include "png.h"


/***************************************************************************
    CONSTANTS & DEFINES
***************************************************************************/

#define MAX_COMPARES            16
#define BITMAP_SPACE			4

enum
{
	STATUS_NOT_PRESENT = 0,
	STATUS_SUCCESS,
	STATUS_SUCCESS_DIFFERENT,
	STATUS_MISSING_FILES,
	STATUS_EXCEPTION,
	STATUS_FATAL_ERROR,
	STATUS_FAILED_VALIDITY,
	STATUS_OTHER,
	STATUS_COUNT
};

enum
{
	BUCKET_UNKNOWN = 0,
	BUCKET_IMPROVED,
	BUCKET_REGRESSED,
	BUCKET_CHANGED,
	BUCKET_MULTI_ERROR,
	BUCKET_CONSISTENT_ERROR,
	BUCKET_GOOD,
	BUCKET_GOOD_BUT_CHANGED,
	BUCKET_GOOD_BUT_CHANGED_SCREENSHOTS,
	BUCKET_COUNT
};



/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

typedef struct _summary_file summary_file;
struct _summary_file
{
    summary_file *  next;
    char            name[10];
    char            source[100];
    UINT8           status[MAX_COMPARES];
    UINT8			matchbitmap[MAX_COMPARES];
    char *          text[MAX_COMPARES];
    UINT32          textsize[MAX_COMPARES];
    UINT32          textalloc[MAX_COMPARES];
};


typedef struct _summary_list summary_list;
struct _summary_list
{
    summary_list *  next;
    summary_file *  files;
    char *			dir;
    char 			version[40];
};



/***************************************************************************
    GLOBAL VARIABLES
***************************************************************************/

static summary_file *filehash[128][128];
static summary_list lists[MAX_COMPARES];
static int list_count;

static const char *bucket_name[] =
{
	"Unknown",
	"Games That Have Improved",
	"Games That Have Regressed",
	"Games With Changed Screenshots",
	"Games With Multiple Errors",
	"Games With Consistent Errors",
	"Games That Are Consistently Good",
	"Games That Regressed But Improved",
	"Games With Changed Screenshots",
};

static const int bucket_output_order[] =
{
	BUCKET_REGRESSED,
	BUCKET_IMPROVED,
	BUCKET_CHANGED,
	BUCKET_GOOD_BUT_CHANGED_SCREENSHOTS,
	BUCKET_GOOD_BUT_CHANGED,
	BUCKET_MULTI_ERROR,
	BUCKET_CONSISTENT_ERROR
};

static const char *status_text[] =
{
	"",
	"Success",
	"Changed",
	"Missing Files",
	"Exception",
	"Fatal Error",
	"Failed Validity Check",
	"Other Unknown Error"
};

static const char *status_color[] =
{
	"",
	"bgcolor=\"#00A000\"",
	"bgcolor=\"#E0E000\"",
	"bgcolor=\"#8000C0\"",
	"bgcolor=\"#C00000\"",
	"bgcolor=\"#C00000\"",
	"bgcolor=\"#C06000\"",
	"bgcolor=\"#C00000\"",
	"bgcolor=\"#C00000\"",
};



/***************************************************************************
    PROTOTYPES
***************************************************************************/

/* summary parsing */
static int read_summary_log(const char *filename, int index);
static summary_file *parse_driver_tag(char *linestart, int index);
static summary_file *get_file(const char *filename);
static int CLIB_DECL compare_file(const void *file0ptr, const void *file1ptr);
static summary_file *sort_file_list(void);

/* HTML helpers */
static FILE *create_file_and_output_header(const char *filename, const char *title, const char *subtitle);
static void output_footer_and_close_file(FILE *file);
static int copy_file(const char *srcdir, const char *srcname, const char *dstdir, const char *dstname);

/* report generators */
static void output_report(const char *dirname, summary_file *filelist);
static int compare_screenshots(summary_file *curfile);
static int generate_png_diff(const summary_file *curfile, const char *destdir, const char *destname);
static void create_linked_file(const char *dirname, const summary_file *curfile, const summary_file *prevfile, const summary_file *nextfile, const char *pngfile);
static void append_driver_list_table(const char *header, const char *dirname, FILE *indexfile, const summary_file *listhead);



/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    alloc_filename - combine directory name
    and remainder into full path, living in
    allocated memory
-------------------------------------------------*/

INLINE const char *alloc_filename(const char *dirname, const char *remainder)
{
	int count = strlen(dirname) + 1 + strlen(remainder) + 1;
	char *result;

	result = malloc(count);
	if (result == NULL)
		return NULL;
	sprintf(result, "%s" PATH_SEPARATOR "%s", dirname, remainder);
	return result;
}


/*-------------------------------------------------
    trim_string - trim leading/trailing spaces
    from a string
-------------------------------------------------*/

INLINE char *trim_string(char *string)
{
	int length;

	/* trim leading spaces */
	while (*string != 0 && isspace(*string))
		string++;

	/* trim trailing spaces */
	length = strlen(string);
	while (length > 0 && isspace(string[length - 1]))
		string[--length] = 0;

	return string;
}


/*-------------------------------------------------
    get_unique_index - get the unique bitmap
    index for a given entry
-------------------------------------------------*/

INLINE int get_unique_index(const summary_file *curfile, int index)
{
	int listnum, curindex = 0;

	/* if we're invalid, just return that */
	if (curfile->matchbitmap[index] == 0xff)
		return -1;

	/* count unique elements up to us */
	for (listnum = 0; listnum < curfile->matchbitmap[index]; listnum++)
		if (curfile->matchbitmap[listnum] == listnum)
			curindex++;
	return curindex;
}



/***************************************************************************
    MAIN
***************************************************************************/

/*-------------------------------------------------
    main - main entry point
-------------------------------------------------*/

int main(int argc, char *argv[])
{
	const char *dirname;
	int listnum;
    int result;

    /* first argument is the directory */
    if (argc < 3)
    {
    	fprintf(stderr, "Usage:\nsummcomp <outputdir> <summary1> [<summary2> [<summary3> ...]]\n");
    	return 1;
    }
    dirname = argv[1];
    list_count = argc - 2;

    /* loop over arguments and read the files */
    for (listnum = 0; listnum < list_count; listnum++)
    {
        result = read_summary_log(argv[listnum + 2], listnum);
        if (result != 0)
            return result;
    }

    /* output the summary */
    output_report(dirname, sort_file_list());
    return 0;
}



/***************************************************************************
    SUMMARY PARSING
***************************************************************************/

/*-------------------------------------------------
    get_file - lookup a driver name in the hash
    table and return a pointer to it; if none
    found, allocate a new entry
-------------------------------------------------*/

static summary_file *get_file(const char *filename)
{
    summary_file *file;

    /* use the first two characters as a lookup */
    for (file = filehash[filename[0] & 0x7f][filename[1] & 0x7f]; file != NULL; file = file->next)
        if (strcmp(filename, file->name) == 0)
            return file;

    /* didn't find one -- allocate */
    file = malloc(sizeof(*file));
    if (file == NULL)
        return NULL;
    memset(file, 0, sizeof(*file));

    /* set the name so we find it in the future */
    strcpy(file->name, filename);

    /* add to the head of the list */
    file->next = filehash[filename[0] & 0x7f][filename[1] & 0x7f];
    filehash[filename[0] & 0x7f][filename[1] & 0x7f] = file;
    return file;
}


/*-------------------------------------------------
    read_summary_log - read a summary.log file
    and build entries for its data
-------------------------------------------------*/

static int read_summary_log(const char *filename, int index)
{
    summary_file *curfile = NULL;
    char linebuffer[1024];
    char *linestart;
    int drivers = 0;
    FILE *file;

    /* open the logfile */
    file = fopen(filename, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Error: file '%s' not found\n", filename);
        return 1;
    }

    /* parse it */
    while (fgets(linebuffer, sizeof(linebuffer), file) != NULL)
    {
    	/* trim the leading/trailing spaces */
    	linestart = trim_string(linebuffer);

        /* is this one of our specials? */
        if (strncmp(linestart, "@@@@@", 5) == 0)
        {
        	/* advance past the signature */
        	linestart += 5;

			/* look for the driver= tag */
			if (strncmp(linestart, "driver=", 7) == 0)
			{
				curfile = parse_driver_tag(linestart + 7, index);
				if (curfile == NULL)
					goto error;
				drivers++;
			}

			/* look for the source= tag */
			else if (strncmp(linestart, "source=", 7) == 0)
			{
				/* error if no driver yet */
				if (curfile == NULL)
				{
					fprintf(stderr, "Unexpected @@@@@source= tag\n");
					goto error;
				}

				/* copy the string */
				strcpy(curfile->source, trim_string(linestart + 7));
			}

			/* look for the dir= tag */
			else if (strncmp(linestart, "dir=", 4) == 0)
			{
				char *dirname = trim_string(linestart + 4);

				/* allocate a copy of the string */
				lists[index].dir = malloc(strlen(dirname) + 1);
				if (lists[index].dir == NULL)
					goto error;
				strcpy(lists[index].dir, dirname);
				fprintf(stderr, "Directory %s\n", lists[index].dir);
			}
		}

        /* if not, consider other options */
        else if (curfile != NULL)
        {
            int foundchars = 0;
            char *curptr;

            /* look for the pngcrc= tag */
			if (strncmp(linestart, "pngcrc: ", 7) == 0)
            {
            }

			/* otherwise, accumulate the text */
			else
			{
				/* find the end of the line and normalize it with a CR */
				for (curptr = linestart; *curptr != 0 && *curptr != '\n' && *curptr != '\r'; curptr++)
					if (!isspace(*curptr))
						foundchars = 1;
				*curptr++ = '\n';
				*curptr = 0;

				/* ignore blank lines */
				if (!foundchars)
					continue;

				/* see if we have enough room */
				if (curfile->textsize[index] + (curptr - linestart) + 1 >= curfile->textalloc[index])
				{
					curfile->textalloc[index] = curfile->textsize[index] + (curptr - linestart) + 256;
					curfile->text[index] = realloc(curfile->text[index], curfile->textalloc[index]);
					if (curfile->text[index] == NULL)
					{
						fprintf(stderr, "Unable to allocate memory for text\n");
						goto error;
					}
				}

				/* append our text */
				strcpy(curfile->text[index] + curfile->textsize[index], linestart);
				curfile->textsize[index] += curptr - linestart;
			}
		}

		/* look for the M.A.M.E. header */
		else if (strncmp(linestart, "M.A.M.E. v", 10) == 0)
		{
			char *start = linestart + 10;
			char *end;

			/* find the end */
			for (end = start; !isspace(*end); end++) ;
			*end = 0;
			strcpy(lists[index].version, start);
			fprintf(stderr, "Parsing results from version %s\n", lists[index].version);
		}
    }

    fclose(file);
    fprintf(stderr, "Parsed %d drivers\n", drivers);
    return 0;

error:
    fclose(file);
    return 1;
}


/*-------------------------------------------------
    parse_driver_tag - parse the status info
    from a driver tag
-------------------------------------------------*/

static summary_file *parse_driver_tag(char *linestart, int index)
{
	summary_file *curfile;
	char *colon;

	/* find the colon separating name from status */
	colon = strchr(linestart, ':');
	if (colon == NULL)
	{
		fprintf(stderr, "Unexpected text after @@@@@driver=\n");
		return NULL;
	}

	/* NULL terminate at the colon and look up the file */
	*colon = 0;
	curfile = get_file(trim_string(linestart));
	if (curfile == NULL)
	{
		fprintf(stderr, "Unable to allocate memory for driver\n");
		return NULL;
	}

	/* clear out any old status for this file */
	curfile->status[index] = STATUS_NOT_PRESENT;
	if (curfile->text[index] != NULL)
		free(curfile->text[index]);
	curfile->text[index] = NULL;
	curfile->textsize[index] = 0;
	curfile->textalloc[index] = 0;

	/* strip leading/trailing spaces from the status */
	colon = trim_string(colon + 1);

	/* convert status into statistics */
	if (strcmp(colon, "Success") == 0)
		curfile->status[index] = STATUS_SUCCESS;
	else if (strcmp(colon, "Missing files") == 0)
		curfile->status[index] = STATUS_MISSING_FILES;
	else if (strcmp(colon, "Exception") == 0)
		curfile->status[index] = STATUS_EXCEPTION;
	else if (strcmp(colon, "Fatal error") == 0)
		curfile->status[index] = STATUS_FATAL_ERROR;
	else if (strcmp(colon, "Failed validity check") == 0)
		curfile->status[index] = STATUS_FAILED_VALIDITY;
	else
		curfile->status[index] = STATUS_OTHER;

	return curfile;
}


/*-------------------------------------------------
    compare_file - compare two files, sorting
    first by source filename, then by driver name
-------------------------------------------------*/

static int CLIB_DECL compare_file(const void *file0ptr, const void *file1ptr)
{
	summary_file *file0 = *(summary_file **)file0ptr;
	summary_file *file1 = *(summary_file **)file1ptr;
	int result = strcmp(file0->source, file1->source);
	if (result == 0)
		result = strcmp(file0->name, file1->name);
	return result;
}


/*-------------------------------------------------
    sort_file_list - convert the hashed lists
    into a single, sorted list
-------------------------------------------------*/

static summary_file *sort_file_list(void)
{
	summary_file *listhead, **tailptr, *curfile, **filearray;
	int numfiles, filenum;
    int c0, c1;

    /* count the total number of files */
    numfiles = 0;
    for (c0 = 0; c0 < 128; c0++)
	    for (c1 = 0; c1 < 128; c1++)
	    	for (curfile = filehash[c0][c1]; curfile != NULL; curfile = curfile->next)
	    		numfiles++;

	/* allocate an array of files */
	filearray = malloc(numfiles * sizeof(*filearray));
	if (filearray == NULL)
	{
		fprintf(stderr, "Out of memory!\n");
		return NULL;
	}

	/* populate the array */
    numfiles = 0;
    for (c0 = 0; c0 < 128; c0++)
	    for (c1 = 0; c1 < 128; c1++)
	    	for (curfile = filehash[c0][c1]; curfile != NULL; curfile = curfile->next)
	    		filearray[numfiles++] = curfile;

	/* sort the array */
	qsort(filearray, numfiles, sizeof(filearray[0]), compare_file);

	/* now regenerate a single list */
    listhead = NULL;
    tailptr = &listhead;
    for (filenum = 0; filenum < numfiles; filenum++)
    {
    	*tailptr = filearray[filenum];
    	tailptr = &(*tailptr)->next;
    }
    *tailptr = NULL;
    free(filearray);

    return listhead;
}



/***************************************************************************
    HTML OUTPUT HELPERS
***************************************************************************/

/*-------------------------------------------------
    create_file_and_output_header - create a new
    HTML file with a standard header
-------------------------------------------------*/

static FILE *create_file_and_output_header(const char *filename, const char *title, const char *subtitle)
{
	FILE *file;

	/* create the indexfile */
	file = fopen(filename, "w");
	if (file == NULL)
		return NULL;

	/* print a header */
	fprintf(file,
		"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
		"\n"
		"<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
		"<head>\n"
		"\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n"
		"\t<title>%s</title>\n"
		"\t<link rel=\"stylesheet\" href=\"http://mamedev.org/styles-site.css\" type=\"text/css\" />\n"
		"</head>\n"
		"\n"
		"<body>\n"
		"\t<div id=\"outer\">\n"
		"\n"
		"\t<div id=\"banner\">\n"
		"\t<h1>%s</h1>\n"
		"\t<h2>%s</h2>\n"
		"\t</div>\n"
		"\n"
		"\t<div id=\"left\">\n"
		"\t<div class=\"sidebar\">\n"
		"\t<!--#include virtual=\"/links.txt\" -->\n"
		"\t</div>\n"
		"\t</div>\n"
		"\n"
		"\t<div id=\"center\">\n"
		"\t<div class=\"content\">\n"
		"\n",
		title, title, (subtitle == NULL) ? "&nbsp;" : subtitle
	);

	/* return the file */
	return file;
}


/*-------------------------------------------------
    output_footer_and_close_file - write a
    standard footer to an HTML file and close it
-------------------------------------------------*/

static void output_footer_and_close_file(FILE *file)
{
	fprintf(file,
		"\n"
		"\t</div>\n"
		"\t</div>\n"
		"\t</div>\n"
		"</body>"
		"\n"
		"</html>\n"
	);
	fclose(file);
}


/*-------------------------------------------------
    copy_file - copy a file from source directory
    to the destination directory
-------------------------------------------------*/

static int copy_file(const char *srcdir, const char *srcname, const char *dstdir, const char *dstname)
{
	const char *srcfilename = alloc_filename(srcdir, srcname);
	const char *dstfilename = alloc_filename(dstdir, dstname);
	FILE *srcfile, *dstfile;
	void *buffer;
	int bytes;

	/* open the source for read */
	srcfile = fopen(srcfilename, "rb");
	if (srcfile == NULL)
	{
		free((void *)srcfilename);
		free((void *)dstfilename);
		return 1;
	}
	free((void *)srcfilename);

	/* open the destination for write */
	dstfile = fopen(dstfilename, "wb");
	if (dstfile == NULL)
	{
		fprintf(stderr, "Could not create file '%s'\n", dstfilename);
		free((void *)dstfilename);
		fclose(srcfile);
		return 1;
	}
	free((void *)dstfilename);

	/* determine source size */
	fseek(srcfile, 0, SEEK_END);
	bytes = ftell(srcfile);
	fseek(srcfile, 0, SEEK_SET);
	buffer = malloc(bytes);
	if (buffer == NULL)
	{
		fprintf(stderr, "Couldn't allocate %d bytes for file copy\n", bytes);
		fclose(srcfile);
		fclose(dstfile);
		return 1;
	}

	/* read/write the data */
	fread(buffer, 1, bytes, srcfile);
	fwrite(buffer, 1, bytes, dstfile);
	fclose(srcfile);
	fclose(dstfile);
	return 0;
}



/***************************************************************************
    REPORT GENERATORS
***************************************************************************/

/*-------------------------------------------------
    output_report - generate the summary
    report HTML files
-------------------------------------------------*/

static void output_report(const char *dirname, summary_file *filelist)
{
	summary_file *buckethead[BUCKET_COUNT], **buckettailptr[BUCKET_COUNT];
	summary_file *curfile;
	const char *indexname;
	int listnum, bucknum;
	FILE *indexfile;
	int count = 0, total;

	/* initialize the lists */
	for (bucknum = 0; bucknum < BUCKET_COUNT; bucknum++)
	{
		buckethead[bucknum] = NULL;
		buckettailptr[bucknum] = &buckethead[bucknum];
	}

	/* compute the total number of files */
	total = 0;
	for (curfile = filelist; curfile != NULL; curfile = curfile->next)
		total++;

	/* first bucketize the games */
	for (curfile = filelist; curfile != NULL; curfile = curfile->next)
	{
		int statcount[STATUS_COUNT] = { 0 };
		int bucket = BUCKET_UNKNOWN;
		int unique_codes = 0;
		int first_valid;

		/* print status */
		if (++count % 100 == 0)
			fprintf(stderr, "Processing file %d/%d\n", count, total);

		/* find the first valid entry */
		for (first_valid = 0; curfile->status[first_valid] == STATUS_NOT_PRESENT; first_valid++) ;

		/* do we need to output anything? */
		for (listnum = first_valid; listnum < list_count; listnum++)
			if (statcount[curfile->status[listnum]]++ == 0)
				unique_codes++;

		/* were we consistent? */
		if (unique_codes == 1)
		{
			/* were we consistently ok? */
			if (curfile->status[first_valid] == STATUS_SUCCESS)
				bucket = compare_screenshots(curfile);

			/* must have been consistently erroring */
			else
				bucket = BUCKET_CONSISTENT_ERROR;
		}

		/* ok, we're not consistent; could be a number of things */
		else
		{
			/* were we ok at the start and end but not in the middle? */
			if (curfile->status[first_valid] == STATUS_SUCCESS && curfile->status[list_count - 1] == STATUS_SUCCESS)
				bucket = BUCKET_GOOD_BUT_CHANGED;

			/* did we go from good to bad? */
			else if (curfile->status[first_valid] == STATUS_SUCCESS)
				bucket = BUCKET_REGRESSED;

			/* did we go from bad to good? */
			else if (curfile->status[list_count - 1] == STATUS_SUCCESS)
				bucket = BUCKET_IMPROVED;

			/* must have had multiple errors */
			else
				bucket = BUCKET_MULTI_ERROR;
		}

		/* add us to the appropriate list */
		*buckettailptr[bucket] = curfile;
		buckettailptr[bucket] = &curfile->next;
	}

	/* terminate all the lists */
	for (bucknum = 0; bucknum < BUCKET_COUNT; bucknum++)
		*buckettailptr[bucknum] = NULL;

	/* output header */
	indexname = alloc_filename(dirname, "index.html");
	indexfile = create_file_and_output_header(indexname, "MAME&trade; Regressions", NULL);
	free((void *)indexname);
	if (indexfile == NULL)
	{
		fprintf(stderr, "Error creating file '%s'\n", indexname);
		return;
	}

	/* iterate over buckets and output them */
	for (bucknum = 0; bucknum < ARRAY_LENGTH(bucket_output_order); bucknum++)
	{
		int curbucket = bucket_output_order[bucknum];

		if (buckethead[curbucket] != NULL)
		{
			fprintf(stderr, "Outputting bucket: %s\n", bucket_name[curbucket]);
			append_driver_list_table(bucket_name[curbucket], dirname, indexfile, buckethead[curbucket]);
		}
	}

	/* output footer */
	output_footer_and_close_file(indexfile);
}


/*-------------------------------------------------
    compare_screenshots - compare the screenshots
    for all the games in a file
-------------------------------------------------*/

static int compare_screenshots(summary_file *curfile)
{
	bitmap_t *bitmaps[MAX_COMPARES];
	int unique[MAX_COMPARES];
	int numunique = 0;
	int listnum;

	/* iterate over all files and load their bitmaps */
	for (listnum = 0; listnum < list_count; listnum++)
	{
		bitmaps[listnum] = NULL;
		if (curfile->status[listnum] == STATUS_SUCCESS)
		{
			file_error filerr;
			const char *fullname;
			char imgname[100];
			core_file *file;

			/* get the filename for the image */
			sprintf(imgname, "snap" PATH_SEPARATOR "%s" PATH_SEPARATOR "final.png", curfile->name);
			fullname = alloc_filename(lists[listnum].dir, imgname);

			/* open the file */
			filerr = core_fopen(fullname, OPEN_FLAG_READ, &file);
			free((void *)fullname);

			/* if that failed, look in the old location */
			if (filerr != FILERR_NONE)
			{
				/* get the filename for the image */
				sprintf(imgname, "snap" PATH_SEPARATOR "_%s.png", curfile->name);
				fullname = alloc_filename(lists[listnum].dir, imgname);

				/* open the file */
				filerr = core_fopen(fullname, OPEN_FLAG_READ, &file);
				free((void *)fullname);
			}

			/* if that worked, load the file */
			if (filerr == FILERR_NONE)
			{
				png_read_bitmap(file, &bitmaps[listnum]);
				core_fclose(file);
			}
		}
	}

	/* now find all the different bitmap types */
	for (listnum = 0; listnum < list_count; listnum++)
	{
		curfile->matchbitmap[listnum] = 0xff;
		if (bitmaps[listnum] != NULL)
		{
			bitmap_t *this_bitmap = bitmaps[listnum];
			int compnum;

			/* compare against all unique bitmaps */
			for (compnum = 0; compnum < numunique; compnum++)
			{
				bitmap_t *base_bitmap = bitmaps[unique[compnum]];
				int bitmaps_differ;
				int x, y;

				/* if the sizes are different, we differ; otherwise start off assuming we are the same */
				bitmaps_differ = (this_bitmap->width != base_bitmap->width || this_bitmap->height != base_bitmap->height);

				/* compare scanline by scanline */
				for (y = 0; y < this_bitmap->height && !bitmaps_differ; y++)
				{
					UINT32 *base = BITMAP_ADDR32(base_bitmap, y, 0);
					UINT32 *curr = BITMAP_ADDR32(this_bitmap, y, 0);

					/* scan the scanline */
					for (x = 0; x < this_bitmap->width; x++)
						if (*base++ != *curr++)
							break;
					bitmaps_differ = (x != this_bitmap->width);
				}

				/* if we matched, remember which listnum index we matched, and stop */
				if (!bitmaps_differ)
				{
					curfile->matchbitmap[listnum] = unique[compnum];
					break;
				}

				/* if different from the first unique entry, adjust the status */
				if (bitmaps_differ && compnum == 0)
					curfile->status[listnum] = STATUS_SUCCESS_DIFFERENT;
			}

			/* if we're unique, add ourselves to the list */
			if (compnum >= numunique)
			{
				unique[numunique++] = listnum;
				curfile->matchbitmap[listnum] = listnum;
				continue;
			}
		}
	}

	/* free the bitmaps */
	for (listnum = 0; listnum < list_count; listnum++)
		if (bitmaps[listnum] != NULL)
			bitmap_free(bitmaps[listnum]);

	/* if all screenshots matched, we're good */
	if (numunique == 1)
		return BUCKET_GOOD;

	/* if the last screenshot matched the first unique one, we're good but changed */
	if (curfile->matchbitmap[listnum - 1] == unique[0])
		return BUCKET_GOOD_BUT_CHANGED_SCREENSHOTS;

	/* otherwise we're just changed */
	return BUCKET_CHANGED;
}


/*-------------------------------------------------
    generate_png_diff - create a new PNG file
    that shows multiple differing PNGs side by
    side with a third set of differences
-------------------------------------------------*/

static int generate_png_diff(const summary_file *curfile, const char *destdir, const char *destname)
{
	const char *dstfilename = alloc_filename(destdir, destname);
	bitmap_t *bitmaps[MAX_COMPARES] = { NULL };
	bitmap_t *finalbitmap = NULL;
	int width, height, maxwidth;
	int bitmapcount = 0;
	int listnum, bmnum;
	core_file *file = NULL;
	char srcimgname[50];
	file_error filerr;
	png_error pngerr;
	int error = -1;
	int starty;

	/* generate the common source filename */
	sprintf(srcimgname, "snap" PATH_SEPARATOR "%s" PATH_SEPARATOR "final.png", curfile->name);

	/* open and load all unique bitmaps */
	for (listnum = 0; listnum < list_count; listnum++)
		if (curfile->matchbitmap[listnum] == listnum)
		{
			const char *filename = alloc_filename(lists[listnum].dir, srcimgname);

			/* open the source image */
			filerr = core_fopen(filename, OPEN_FLAG_READ, &file);
			free((void *)filename);
			if (filerr != FILERR_NONE)
				goto error;

			/* load the source image */
			pngerr = png_read_bitmap(file, &bitmaps[bitmapcount++]);
			core_fclose(file);
			if (pngerr != FILERR_NONE)
				goto error;
		}

	/* if there's only one unique bitmap, skip it */
	if (bitmapcount <= 1)
		goto error;

	/* determine the size of the final bitmap */
	height = width = 0;
	maxwidth = bitmaps[0]->width;
	for (bmnum = 1; bmnum < bitmapcount; bmnum++)
	{
		int curwidth;

		/* determine the maximal width */
		maxwidth = MAX(maxwidth, bitmaps[bmnum]->width);
		curwidth = bitmaps[0]->width + BITMAP_SPACE + maxwidth + BITMAP_SPACE + maxwidth;
		width = MAX(width, curwidth);

		/* add to the height */
		height += MAX(bitmaps[0]->height, bitmaps[bmnum]->height);
		if (bmnum != 1)
			height += BITMAP_SPACE;
	}

	/* allocate the final bitmap */
	finalbitmap = bitmap_alloc(width, height, BITMAP_FORMAT_ARGB32);
	if (finalbitmap == NULL)
		goto error;

	/* now copy and compare each set of bitmaps */
	starty = 0;
	for (bmnum = 1; bmnum < bitmapcount; bmnum++)
	{
		bitmap_t *bitmap1 = bitmaps[0];
		bitmap_t *bitmap2 = bitmaps[bmnum];
		int curheight = MAX(bitmap1->height, bitmap2->height);
		int x, y;

		/* iterate over rows in these bitmaps */
		for (y = 0; y < curheight; y++)
		{
			UINT32 *src1 = (y < bitmap1->height) ? BITMAP_ADDR32(bitmap1, y, 0) : NULL;
			UINT32 *src2 = (y < bitmap2->height) ? BITMAP_ADDR32(bitmap2, y, 0) : NULL;
			UINT32 *dst1 = BITMAP_ADDR32(finalbitmap, starty + y, 0);
			UINT32 *dst2 = BITMAP_ADDR32(finalbitmap, starty + y, bitmap1->width + BITMAP_SPACE);
			UINT32 *dstdiff = BITMAP_ADDR32(finalbitmap, starty + y, bitmap1->width + BITMAP_SPACE + maxwidth + BITMAP_SPACE);

			/* now iterate over columns */
			for (x = 0; x < maxwidth; x++)
			{
				int pix1 = -1, pix2 = -2;

				if (src1 != NULL && x < bitmap1->width)
					pix1 = dst1[x] = src1[x];
				if (src2 != NULL && x < bitmap2->width)
					pix2 = dst2[x] = src2[x];
				dstdiff[x] = (pix1 != pix2) ? 0xffffffff : 0xff000000;
			}
		}

		/* update the starting Y position */
		starty += BITMAP_SPACE + MAX(bitmap1->height, bitmap2->height);
	}

	/* write the final PNG */
	filerr = core_fopen(dstfilename, OPEN_FLAG_WRITE | OPEN_FLAG_CREATE, &file);
	if (filerr != FILERR_NONE)
		goto error;
	pngerr = png_write_bitmap(file, NULL, finalbitmap, 0, NULL);
	core_fclose(file);
	if (pngerr != FILERR_NONE)
		goto error;

	/* if we get here, we are error free */
	error = 0;

error:
	if (dstfilename != NULL)
		free((void *)dstfilename);
	if (finalbitmap != NULL)
		bitmap_free(finalbitmap);
	for (bmnum = 0; bmnum < bitmapcount; bmnum++)
		if (bitmaps[bmnum] != NULL)
			bitmap_free(bitmaps[bmnum]);
	if (error)
		osd_rmfile(dstfilename);
	return error;
}


/*-------------------------------------------------
    create_linked_file - create a comparison
    file between differing versions
-------------------------------------------------*/

static void create_linked_file(const char *dirname, const summary_file *curfile, const summary_file *prevfile, const summary_file *nextfile, const char *pngfile)
{
	const char *linkname;
	char filename[100];
	char title[100];
	FILE *linkfile;
	int listnum;

	/* create the filename */
	sprintf(filename, "%s.html", curfile->name);

	/* output header */
	sprintf(title, "%s Regressions", curfile->name);
	linkname = alloc_filename(dirname, filename);
	linkfile = create_file_and_output_header(linkname, title, NULL);
	free((void *)linkname);
	if (linkfile == NULL)
	{
		fprintf(stderr, "Error creating file '%s'\n", filename);
		return;
	}

	/* link to the previous/next entries */
	fprintf(linkfile, "\t<p>\n");
	fprintf(linkfile, "\t<table width=\"100%%\">\n");
	fprintf(linkfile, "\t\t<td align=\"left\" width=\"40%%\" style=\"border:none\">");
	if (prevfile != NULL)
		fprintf(linkfile, "<a href=\"%s.html\"><< %s (%s)</a>", prevfile->name, prevfile->name, prevfile->source);
	fprintf(linkfile, "</td>\n");
	fprintf(linkfile, "\t\t<td align=\"center\" width=\"20%%\" style=\"border:none\"><a href=\"index.html\">Home</a></td>\n");
	fprintf(linkfile, "\t\t<td align=\"right\" width=\"40%%\" style=\"border:none\">");
	if (nextfile != NULL)
		fprintf(linkfile, "<a href=\"%s.html\">%s (%s) >></a>", nextfile->name, nextfile->name, nextfile->source);
	fprintf(linkfile, "</td>\n");
	fprintf(linkfile, "\t</table>\n");
	fprintf(linkfile, "\t</p>\n");

	/* output data for each one */
	for (listnum = 0; listnum < list_count; listnum++)
	{
		int imageindex = -1;

		/* generate the HTML */
		fprintf(linkfile, "\n\t<h2>%s</h2>\n", lists[listnum].version);
		fprintf(linkfile, "\t<p>\n");
		fprintf(linkfile, "\t<b>Status:</b> %s\n", status_text[curfile->status[listnum]]);
		if (pngfile != NULL)
			imageindex = get_unique_index(curfile, listnum);
		if (imageindex != -1)
			fprintf(linkfile, " [%d]", imageindex);
		fprintf(linkfile, "\t</p>\n");
		if (curfile->text[listnum] != NULL)
		{
			fprintf(linkfile, "\t<p>\n");
			fprintf(linkfile, "\t<b>Errors:</b>\n");
			fprintf(linkfile, "\t<pre>%s</pre>\n", curfile->text[listnum]);
			fprintf(linkfile, "\t</p>\n");
		}
	}

	/* output link to the image */
	if (pngfile != NULL)
	{
		fprintf(linkfile, "\n\t<h2>Screenshot Comparisons</h2>\n");
		fprintf(linkfile, "\t<p>\n");
		fprintf(linkfile, "\t<img src=\"%s\" />\n", pngfile);
		fprintf(linkfile, "\t</p>\n");
	}

	/* output footer */
	output_footer_and_close_file(linkfile);
}


/*-------------------------------------------------
    append_driver_list_table - append a table
    of drivers from a list to an HTML file
-------------------------------------------------*/

static void append_driver_list_table(const char *header, const char *dirname, FILE *indexfile, const summary_file *listhead)
{
	const summary_file *curfile, *prevfile;
	int width = 100 / (2 + list_count);
	int listnum;

	/* output a header */
	fprintf(indexfile, "\t<h2>%s</h2>\n", header);
	fprintf(indexfile, "\t<div style=\"padding-left:20px;\">\n");

	/* start the table */
	fprintf(indexfile, "\t<p><table width=\"90%%\">\n");
	fprintf(indexfile, "\t\t<tr>\n\t\t\t<th width=\"%d%%\">Source</th><th width=\"%d%%\">Driver</th>", width, width);
	for (listnum = 0; listnum < list_count; listnum++)
		fprintf(indexfile, "<th width=\"%d%%\">%s</th>", width, lists[listnum].version);
	fprintf(indexfile, "\n\t\t</tr>\n");

	/* if nothing, print a default message */
	if (listhead == NULL)
	{
		fprintf(indexfile, "\t\t<tr>\n\t\t\t");
		fprintf(indexfile, "<td colspan=\"%d\" align=\"center\">(No regressions detected)</td>", list_count + 2);
		fprintf(indexfile, "\n\t\t</tr>\n");
	}

	/* iterate over files */
	for (prevfile = NULL, curfile = listhead; curfile != NULL; prevfile = curfile, curfile = curfile->next)
	{
		int rowspan = 0, uniqueshots = 0;
		char pngdiffname[40];

		/* if this is the first entry in this source file, count how many rows we need to span */
		if (prevfile == NULL || strcmp(prevfile->source, curfile->source) != 0)
		{
			const summary_file *cur;
			for (cur = curfile; cur != NULL; cur = cur->next)
				if (strcmp(cur->source, curfile->source) == 0)
					rowspan++;
				else
					break;
		}

		/* create screenshots if necessary */
		pngdiffname[0] = 0;
		for (listnum = 0; listnum < list_count; listnum++)
			if (curfile->matchbitmap[listnum] == listnum)
				uniqueshots++;
		if (uniqueshots > 1)
		{
			sprintf(pngdiffname, "compare_%s.png", curfile->name);
			if (generate_png_diff(curfile, dirname, pngdiffname) != 0)
				pngdiffname[0] = 0;
		}

		/* create a linked file */
		create_linked_file(dirname, curfile, prevfile, curfile->next, (pngdiffname[0] == 0) ? NULL : pngdiffname);

		/* create a row */
		fprintf(indexfile, "\t\t<tr>\n\t\t\t");
		if (rowspan > 0)
			fprintf(indexfile, "<td rowspan=\"%d\">%s</td>", rowspan, curfile->source);
		fprintf(indexfile, "<td><a href=\"%s.html\">%s</a></td>", curfile->name, curfile->name);
		for (listnum = 0; listnum < list_count; listnum++)
		{
			int unique_index = -1;

			if (pngdiffname[0] != 0)
				unique_index = get_unique_index(curfile, listnum);
			if (unique_index != -1)
				fprintf(indexfile, "<td %s>%s [<a href=\"%s\" target=\"blank\">%d</a>]</td>", status_color[curfile->status[listnum]], status_text[curfile->status[listnum]], pngdiffname, unique_index);
			else
				fprintf(indexfile, "<td %s>%s</td>", status_color[curfile->status[listnum]], status_text[curfile->status[listnum]]);
		}
		fprintf(indexfile, "\n\t\t</tr>\n");

		/* also print the name and source file */
		printf("%s %s\n", curfile->name, curfile->source);
	}

	/* end of table */
	fprintf(indexfile, "</table></p>\n");
	fprintf(indexfile, "</div>\n");
}
