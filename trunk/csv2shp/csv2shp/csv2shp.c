#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "csv.h"
#include "shapefil.h"

struct parameters
{
	double x;
	double y;

	unsigned int field;
	unsigned int record;

	SHPHandle shpFile;
	DBFHandle dbfFile;
};

void field_cb(void* data, size_t length, struct parameters* params)
{
	char* value;

	value = malloc(length + 1);		
	if (value == NULL)
    {
        printf("ERROR: Unable to allocate %Iu bytes of memory for the field value at field %u, record %u.\n", length + 1, params->field, params->record);
		exit(EXIT_FAILURE);
    }

	memset(value, 0, length + 1);
	memcpy(value, data, length);
	
	if (params->record > 0)
	{

		if (params->field == 0)
		{
			if (sscanf(value, "%lf", &params->x) != 1)
            {
                printf("ERROR: Unable to convert X coordinate at field %u, record %u.\n", params->field, params->record);
		        exit(EXIT_FAILURE);
            }
		}
		else if (params->field == 1)
		{
			if (sscanf(value, "%lf", &params->y) != 1)
            {
                printf("ERROR: Unable to convert Y coordinate at field %u, record %u.\n", params->field, params->record);
		        exit(EXIT_FAILURE);
            }
		}

		if (!DBFWriteStringAttribute(params->dbfFile, params->record - 1, params->field, value))
        {
            printf("ERROR: Unable to write attribute to DBF file at field %u, record %u.\n", params->field, params->record);
	        exit(EXIT_FAILURE);
        }
	}
	else
	{
		if (DBFAddField(params->dbfFile, value, FTString, 255, 0) < 0)
        {
            printf("ERROR: Unable to add field to DBF file at field %u, record %u.\n", params->field, params->record);
	        exit(EXIT_FAILURE);
        }
	}

	free(value);
	value = NULL;

	params->field++;
}

void record_cb(int result, struct parameters* params)
{
	SHPObject* point;

	point = NULL;

	if (params->record > 0)
	{
		if (params->field < 2)
        {
            printf("ERROR: Need at least X and Y coordinates at  record %u.\n", params->record);
	        exit(EXIT_FAILURE);
        }

		point = SHPCreateSimpleObject(SHPT_POINT, 1, &params->x, &params->y, NULL);
		if (point == NULL)
        {
            printf("ERROR: Unable to create point object for SHP file at  record %u.\n", params->record);
	        exit(EXIT_FAILURE);
        }

		if (SHPWriteObject(params->shpFile, -1, point) < 0)
        {
            printf("ERROR: Unable to write point object to SHP file at  record %u.\n", params->record);
	        exit(EXIT_FAILURE);
        }
		SHPDestroyObject(point);
		point = NULL;
	}

	params->field = 0;
	params->record++;
}

void freeParameters(struct parameters* params)
{
	params->shpFile = NULL;
	params->dbfFile = NULL;
}

int main(int argc, char *argv[])
{
	FILE* csvFile;
	SHPHandle shpFile;
	DBFHandle dbfFile;
	char* fileName;
	char* fileNameDot;
	size_t fileNameLength;
	struct csv_parser csvParser;
	struct parameters params;
	char buf[1024];
	size_t bytesRead;

	csvFile = NULL;
	fileName = NULL;
	fileNameDot = NULL;
	shpFile = NULL;
	dbfFile = NULL;

	memset(buf, 0, sizeof(buf)); 
	memset(&csvParser, 0, sizeof(csvParser));
	memset(&params, 0, sizeof(params));

	if (argc < 2)
    {
        printf("Usage: csv2shp filename\n");
        exit(EXIT_FAILURE);
    }

	if (csv_init(&csvParser, 0) != 0)
    {
        printf("ERROR: Unable to initialise the CSV file parser.\n");
        exit(EXIT_FAILURE);
    }

	
	csvFile = fopen(argv[1], "rb");
	if (csvFile == NULL)
    {
        printf("ERROR: Unable to open the CSV file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
	
	fileNameDot = strrchr(argv[1], '.');
	if (fileNameDot != NULL)
		fileNameLength = fileNameDot - argv[1];
	else
		fileNameLength = strlen(argv[1]);
	fileName = malloc(fileNameLength + 1);
	if (fileName == NULL)
    {
        printf("ERROR: Unable to allocate %Iu bytes of memory for shapefile filename.\n", fileNameLength + 1);
		exit(EXIT_FAILURE);
    }
	memset(fileName, 0, fileNameLength + 1);
	strncpy(fileName, argv[1], fileNameLength);
	
	shpFile = SHPCreate(fileName, SHPT_POINT);
	if (shpFile == NULL)
    {
        printf("ERROR: Unable to create the SHP file.\n");
        exit(EXIT_FAILURE);
    }
	dbfFile = DBFCreate(fileName);
	if (dbfFile == NULL)
    {
        printf("ERROR: Unable to create the DBF file.\n");
        exit(EXIT_FAILURE);
    }

	params.shpFile = shpFile;
	params.dbfFile = dbfFile;

	free(fileName);
	fileName = NULL;

	while ((bytesRead = fread(buf, sizeof(*buf), sizeof(buf) / sizeof(*buf), csvFile)) > 0)
	{
		if (csv_parse(&csvParser, buf, bytesRead, field_cb, record_cb, &params) != bytesRead)
        {
            printf("ERROR: Unable parse CSV file: %s\n", csv_strerror(csv_error(&csvParser)));
            exit(EXIT_FAILURE);
        }
	}
	if (ferror(csvFile) != 0)
    {
        printf("ERROR: Unable to open the CSV file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	if (csv_fini(&csvParser, field_cb, record_cb, &params) != 0)
    {
        printf("ERROR: Unable finish parsing CSV file: %s\n", csv_strerror(csv_error(&csvParser)));
        exit(EXIT_FAILURE);
    }
	freeParameters(&params);

	DBFClose(dbfFile);
	dbfFile = NULL;

	SHPClose(shpFile);
	shpFile = NULL;

	fclose(csvFile);
	csvFile = NULL;

	csv_free(&csvParser);

	exit(EXIT_SUCCESS);
	return 0;
}