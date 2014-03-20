/**************************************************/
/* PDF_read                                       */
/* Read .pdf file and restore as text.            */
/* - Support HTS version 2.2 (engine-1.03/4/5/6)  */
/* - This is the version contain most info.       */
/**************************************************/
/* Author : Xingyu Na                             */
/* Date   : January 2013                          */
/**************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NSTATE 5

#if defined(_WIN32)
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define MODELEXT ".pdf"
#define MODELEXT_LEN strlen(MODELEXT)

/* Usage: output usage */
void Usage(void)
{
   fprintf(stderr, "\n");
   fprintf(stderr, "PDF_read - Read .pdf file and restore as text.\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "  usage:\n");
   fprintf(stderr, "       PDF_read [ options ] \n");
   fprintf(stderr, "  options:                                                       [  def]\n");
   fprintf(stderr, "    -m pdf         : model file                                  [  N/A]\n");
   fprintf(stderr, "    -n int         : number of states                            [  %3d]\n", NSTATE);
   fprintf(stderr, "    -o txt         : output text file name                       [  N/A]\n");
   fprintf(stderr, "    -h             : show this help message\n");
   fprintf(stderr, "  note:\n");
   fprintf(stderr, "    pdf file name is the name of feature.\n");
   fprintf(stderr, "\n");

   exit(0);
}

/** Safe calloc routine. */
static void* xcalloc(size_t num, size_t size) {
   void *ptr;
   ptr = calloc(num,size);
   if (ptr == NULL) {
	fprintf(stderr, "PDF_read: Out of memory\n");
	exit(-1);
   }
   return ptr;
}

static int PDF_byte_swap(void *p, const int size, const int block)
{
   char *q, tmp;
   int i, j;

   q = (char *) p;

   for (i = 0; i < block; i++) {
      for (j = 0; j < (size / 2); j++) {
         tmp = *(q + j);
         *(q + j) = *(q + (size - 1 - j));
         *(q + (size - 1 - j)) = tmp;
      }
      q += size;
   }

   return i;
}

/** Splits a file name and returns a new char* with the directory.*/
static char* get_directory(const char *fn) {
   char *tmp;
   char *directory =NULL;
   tmp = strrchr(fn, PATH_SEP); /* pointer to the last pathsep*/
   if (tmp != NULL) {
      tmp++;
      directory = xcalloc(tmp-fn+1,sizeof(char));
      strncpy(directory, fn, tmp-fn);
      directory[tmp-fn] = '\0';
   } else {
      directory = xcalloc(1,sizeof(char));
      directory[0] = '\0';
   }
   return directory;
}

/* Returns ".pdf" if fn ends with .pdf, returns "" otherwise */
static char* get_model_extension(const char *fn) {
   char *extension = NULL;
   if (strncmp(MODELEXT, &(fn[strlen(fn)-MODELEXT_LEN]),MODELEXT_LEN) == 0) {
      extension = xcalloc(MODELEXT_LEN+1,sizeof(char));
      strcpy(extension,MODELEXT);
      extension[MODELEXT_LEN] = '\0';
   } else {
      extension = xcalloc(1,sizeof(char));
      extension[0] = '\0';
   }
   return extension;
}

static char* get_filename(const char *fn, const char *dir, const char *ext) {
   char *filename = NULL;
   size_t fn_len;
   fn_len = strlen(fn)-strlen(dir)-strlen(ext);
   filename = xcalloc(fn_len, sizeof(char));
   strncpy(filename, fn + strlen(dir),fn_len);
   return filename;
}

int PDF_fread(void *p, const int size, const int num, FILE * fp)
{
   const int block = fread(p, size, num, fp);

   PDF_byte_swap(p, size, block);
   return block;
}

int main (int argc, char** argv)
{
	FILE *fpdf = NULL, *ftxt = NULL;
	int *npdfs, nstate = NSTATE, nvec, iMSD, nstream;
	char *fnpdf = NULL, *fntxt = NULL;
	float ***pdfs;
	int i, j, k;
	int free_fntxt = 1;
	/* Assuming fnpdf is given as: "../bar/foo/dur.pdf":
           fnpdf_dir: Directory where fnpdf is: "../bar/foo/"
           fnpdf_fn: File name without extension: "dur"
           fnpdf_ext: Extension: ".pdf"
	*/
	char *fnpdf_dir = NULL, *fnpdf_fn = NULL, *fnpdf_ext = NULL;


	/* parse command line */
   if (argc == 1 || argc %2 == 0)
      Usage();

	while (--argc) {
		if (**++argv == '-') {
			switch (*(*argv + 1)) {
			case 'm':
				fnpdf = *++argv;
				--argc;
				break;
			case 'n':
				nstate = atoi(*++argv);
				--argc;
				break;
			case 'o':
				fntxt = *++argv;
				free_fntxt = 0;
				--argc;
				break;
			case 'h':
				Usage();
			default:
				printf("PDF_read: Invalid option '-%c'.\n", *(*argv + 1));
				exit(0);
			}
		}
	}
	if (fnpdf == NULL) {
		printf("PDF_read: PDF file name required\n");
		exit(-1);
	}
	if((fpdf = fopen(fnpdf, "rb")) == NULL) {
		printf("PDF_read: Cannot open %s.\n", fnpdf);
		exit(-1);
	}

	/* Split fnpdf in fnpdf_dir, fnpdf_fn, fnpdf_ext */
	fnpdf_ext = get_model_extension(fnpdf);
	fnpdf_dir = get_directory(fnpdf);
	fnpdf_fn = get_filename(fnpdf, fnpdf_dir, fnpdf_ext);


	printf("Analyzing \"%s\" model.\n", fnpdf_fn);

	if(fntxt == NULL) {
		fntxt = xcalloc(strlen(fnpdf_dir) + strlen(fnpdf_fn) +4+1, sizeof(char));
		sprintf(fntxt, "%s%s.txt", fnpdf_dir, fnpdf_fn);
	}
	if((ftxt = fopen(fntxt, "w")) == NULL) {
		printf("PDF_read: Creating %s failed.\n", fntxt);
		exit(-1);
	}

	/* load MSD flag */
	PDF_fread(&iMSD, sizeof(int), 1, fpdf);

	/* load stream size */
	PDF_fread(&nstream, sizeof(int), 1, fpdf);

	/* load vector size */
	PDF_fread(&nvec, sizeof(int), 1, fpdf);

	/* load number of pdfs */
	npdfs = xcalloc(nstate, sizeof(int));
	for(i = 0; i < nstate; i++)
		PDF_fread(&npdfs[i], sizeof(int), 1, fpdf);

	/* load pdfs */
	pdfs = xcalloc(nstate, sizeof(float **));
	for(i = 0; i < nstate; i++) {
		pdfs[i] = xcalloc(npdfs[i], sizeof(float *));
		for(j = 0; j < npdfs[i]; j++) {
			if(iMSD == 0)
				pdfs[i][j] = xcalloc(nstream * nvec * 2, sizeof(float));
			else
				pdfs[i][j] = xcalloc(nstream * nvec * 4, sizeof(float));
		}
	}
	for(i = 0; i < nstate; i++) {
		for(j = 0; j < npdfs[i]; j++) {
			for(k = 0; k < nvec; k++) {
				/* load mean element */
				PDF_fread(&pdfs[i][j][k], sizeof(float), 1, fpdf);
				/* load variance element */
				PDF_fread(&pdfs[i][j][k + nstream * nvec], sizeof(float), 1, fpdf);
				if(iMSD == 1) {
					/* load MSD value */
					PDF_fread(&pdfs[i][j][k + nstream * nvec * 2], sizeof(float), 1, fpdf);
					/* load counter MSD value */
					PDF_fread(&pdfs[i][j][k + nstream * nvec * 3], sizeof(float), 1, fpdf);
				}
			}
		}
	}
	fclose(fpdf);

	/* output text */
	fprintf(ftxt, "Number of states: %d\t", nstate);
	if(iMSD == 0)
		fprintf(ftxt, "Non-MSD model\n");
	else
		fprintf(ftxt, "MSD model\n");
	fprintf(ftxt, "Number of streams: %d\n", nstream);
	fprintf(ftxt, "Vector length: %d\n", nvec);
	for(i = 0; i < nstate; i++) {
		fprintf(ftxt, "State %s_s%d\tNumber of nodes %d\n", fnpdf_fn, i+2, npdfs[i]);
		for(j = 0; j < npdfs[i]; j++) {
			fprintf(ftxt, "\tNode %s_s%d_%d\n", fnpdf_fn, i+2, j+1);
			fprintf(ftxt, "\tMean");
			for(k = 0; k < nvec; k++) {
				fprintf(ftxt, "\t%6f", pdfs[i][j][k]);
			}
			fprintf(ftxt, "\n");
			fprintf(ftxt, "\tVariance");
			for(k = 0; k < nvec; k++) {
				fprintf(ftxt, "\t%6f", pdfs[i][j][k + nstream * nvec]);
			}
			fprintf(ftxt, "\n");
			if(iMSD == 1) {
				fprintf(ftxt, "\tMSD");
				for(k = 0; k < nvec; k++) {
					fprintf(ftxt, "\t%6f", pdfs[i][j][k + nstream * nvec * 2]);
				}
				fprintf(ftxt, "\n");
				fprintf(ftxt, "\tCounter-MSD");
				for(k = 0; k < nvec; k++) {
					fprintf(ftxt, "\t%6f", pdfs[i][j][k + nstream * nvec * 3]);
				}
				fprintf(ftxt, "\n");
			}
		}
	}
	fclose(ftxt);
	free(fnpdf_dir);
	free(fnpdf_fn);
	free(fnpdf_ext);
	if (free_fntxt)	free(fntxt);
	for(i = 0; i < nstate; i++) {
		for(j = 0; j < npdfs[i]; j++)
			free(pdfs[i][j]);
		free(pdfs[i]);
	}
	free(pdfs);
	free(npdfs);
	return 0;
}
