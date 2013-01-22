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

/* Usage: output usage */
void Usage(void)
{
   fprintf(stderr, "\n");
   fprintf(stderr, "PDF_read - Read .pdf file and restore as text.\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "  usage:\n");
   fprintf(stderr, "       PDF_read [ options ] \n");
   fprintf(stderr, "  options:                                                                   [  def]\n");
   fprintf(stderr, "    -m pdf         : model file                                              [  N/A]\n");
   fprintf(stderr, "    -n int         : number of states                                        [   %d]\n", NSTATE);
   fprintf(stderr, "    -o txt         : output text file name                                   [  N/A]\n");
   fprintf(stderr, "  note:\n");
   fprintf(stderr, "    pdf file name is the name of feature.\n");
   fprintf(stderr, "\n");

   exit(0);
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

int PDF_fread(void *p, const int size, const int num, FILE * fp)
{
   const int block = fread(p, size, num, fp);

   PDF_byte_swap(p, size, block);
   return block;
}

void main (int argc, char** argv)
{
	FILE *fpdf = NULL, *ftxt = NULL;
	int *npdfs, nstate = NSTATE, nvec, iMSD, nstream;
	char *fnpdf = NULL, *fntxt = NULL, *pattern = NULL;
	float ***pdfs;
	int i, j, k;

	/* parse command line */
   if (argc == 1)
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
				--argc;
				break;
			default:
				printf("PDF_read: Invalid option '-%c'.\n", *(*argv + 1));
				exit(0);
			}
		}
	}
	if((fpdf = fopen(fnpdf, "rb")) == NULL) {
		printf("PDF_read: Cannot open %s.\n", fnpdf);
		exit(-1);
	}
	pattern = strtok(fnpdf, ".pdf");
	printf("Anylizing \"%s\" model.\n", pattern);

	if(fntxt == NULL) {
		fntxt = (char *)calloc(strlen(fnpdf), sizeof(char));
		sprintf(fntxt, "%s.txt", pattern);
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
	npdfs = (int *)calloc(nstate, sizeof(int));
	for(i = 0; i < nstate; i++)
		PDF_fread(&npdfs[i], sizeof(int), 1, fpdf);

	/* load pdfs */
	pdfs = (float ***)calloc(nstate, sizeof(float **));
	for(i = 0; i < nstate; i++) {
		pdfs[i] = (float **)calloc(npdfs[i], sizeof(float *));
		for(j = 0; j < npdfs[i]; j++) {
			if(iMSD == 0)
				pdfs[i][j] = (float *)calloc(nstream * nvec * 2, sizeof(float));
			else
				pdfs[i][j] = (float *)calloc(nstream * nvec * 4, sizeof(float));
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
		fprintf(ftxt, "State %s_s%d\tNumber of nodes %d\n", pattern, i+2, npdfs[i]);
		for(j = 0; j < npdfs[i]; j++) {
			fprintf(ftxt, "\tNode %s_s%d_%d\n", pattern, i+2, j+1);
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
	for(i = 0; i < nstate; i++) {
		for(j = 0; j < npdfs[i]; j++)
			free(pdfs[i][j]);
		free(pdfs[i]);
	}
	free(pdfs);
	free(npdfs);
}