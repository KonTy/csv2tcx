/*
 * copyright (c) 2015 Sam C. Lin
 * convert GPS Master CSV file to TCX
 *
 * 20150126 SCL v0.1 first rev
 * 20150126 SCL v0.2 add max,avg HR
 * 20150126 SCL v0.3 don't need to specify outfile
 * 20150126 SCL v0.4 convert CSV from local (computer) time to GMT
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <ctime>
#include <iostream>

#define MAX_PATH 256
#define VERSTR "Lincomatic GPS Master CSV to GPX Converter v0.4\n\n"

#define MAX_PTS 32768

class Datum {
  char *buf;
public:
  char *time;
  char *satcnt;
  char *hr;
  char *speed;
  char *lon;
  char *lat;
  char *alt;
  char *heading;
  char *dist;
  Datum() { buf = NULL; }
  ~Datum() { if (buf) delete [] buf; }
  int Set(char *line);
};

int Datum::Set(char *line)
{
  int slen = strlen(line);
  buf = new char[slen+1];
  if (buf) {
    strcpy(buf,line);
    buf[slen] = 0;

    char *s = buf;
    s = strchr(s,',');
    *(s++) = 0;
    this->time = s;
    s = strchr(s,',');
    *(s++) = 0;
    this->satcnt = s;
    s = strchr(s,',');
    *(s++) = 0;
    this->hr = s;
    s = strchr(s,',');
    *(s++) = 0;
    this->speed = s;
    s = strchr(s,',');
    *(s++) = 0;
    this->lon = s;
    s = strchr(s,',');
    *(s++) = 0;
    this->lat = s;
    s = strchr(s,',');
    *(s++) = 0;
    this->alt = s;
    s = strchr(s,',');
    *(s++) = 0;
    this->heading = s;
    s = strchr(s,',');
    *(s++) = 0;
    this->dist = s;
    
    return 0;
  }

  return 1;
}


Datum pts[MAX_PTS];
int ptCnt;
char line[4096];
// read line and convert from UTF-16
int readLine(FILE *fp)
{
  int i=0;
  for (;;) {
    char c0,c1;
    if ((fread(&c0,1,1,fp) == 1) && 
	(fread(&c1,1,1,fp) == 1)) {
      if (c1 != 0) {
	return 2;
      }
      if (c0 == 0x0a) {
	line[i++] = '\0';
	return 0;
      }
      if (c0 == '\t') {
	line[i++] = ',';
      }
	  else if ((c0 != 0x0d) && (c0 != '"')) {
	line[i++] = c0;
      }

    }
    else {
      return 1;
    }
  }
}


int readCSV(char *fn)
{
  FILE *fp = fopen(fn,"rb");
  if (fp) {
    // throw away header FFFE
    if (fread(line,2,1,fp) != 1) {
      return 1;
    }
	readLine(fp); // throw away header line
    while (!readLine(fp)) {
      if (pts[ptCnt++].Set(line)) {
	printf("Out of Memory! point %d\n",ptCnt);
	return 2;
      }
    }
    fclose(fp);
	return 0;
  }
  return 1;
}

double ft2m(char *sft)
{
  double ft;
  sscanf(sft,"%lf",&ft);
  return ft * .30480;
}

time_t csv2time(const char *csvtime)
{
  struct tm stm;
  int i;
  char buf[80];
  strcpy(buf,csvtime);
  char *s = buf;
  s = strtok(buf,"-");
  sscanf(s,"%d",&i);
  stm.tm_year = i - 1900;
  s = strtok(NULL,"-");
  sscanf(s,"%d",&i);
  stm.tm_mon = i - 1;
  s = strtok(NULL," ");
  sscanf(s,"%d",&i);
  stm.tm_mday = i;
  s = strtok(NULL,":");
  sscanf(s,"%d",&i);
  stm.tm_hour = i;
  s = strtok(NULL,":");
  sscanf(s,"%d",&i);
  stm.tm_min = i;
  s = strtok(NULL,":");
  sscanf(s,"%d",&i);
  stm.tm_sec = i;
  stm.tm_isdst = -1;
  return mktime(&stm);
}

void makeTrackPoint(Datum *dt)
{
  char bpm[80];

  if (*dt->hr == '0') {
    bpm[0] = 0;
  }
  else {
    sprintf(bpm,"    <HeartRateBpm>\n     <Value>%s</Value>\n    </HeartRateBpm>\n",dt->hr);
  }

  time_t csvtime = csv2time(dt->time);
  char stime[80];
  // strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
  // this will work too, if your compiler doesn't support %F or %T:
  strftime(stime, sizeof(stime), "%Y-%m-%dT%H:%M:%SZ", gmtime(&csvtime));

  sprintf(line,"   <Trackpoint>\n    <Time>%s</Time>\n    <Position>\n     <LatitudeDegrees>%s</LatitudeDegrees>\n     <LongitudeDegrees>%s</LongitudeDegrees>\n    </Position>\n    <AltitudeMeters>%lf</AltitudeMeters>\n%s   </Trackpoint>\n",stime,dt->lat,dt->lon,ft2m(dt->alt),bpm);
}
int writeTCX(char *sport,char *fn)
{
  FILE *fp = fopen(fn,"wb");
  if (fp) {
    int i;
    double avgHR=0.0,maxHR=0.0;
    int ptcnt = 0;
    for (i=0;i < ptCnt;i++) {
      double hr = strtod(pts[i].hr,NULL);
      if (hr) {
	if (hr > maxHR) {
	  maxHR = hr;
	}
	avgHR += hr;
	ptcnt++;
      }
    }

    int iavgHR = (int)(ceil(avgHR /= (double)ptcnt));
    int imaxHR = (int) maxHR;

    printf("Avg HR: %d\n",iavgHR);
    printf("Max HR: %d\n",imaxHR);
    printf("Trackpoints %d\n", ptCnt);

    time_t csvtime = csv2time(pts[0].time);
    char stime[80];
    // strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
    // this will work too, if your compiler doesn't support %F or %T:
    strftime(stime, sizeof(stime), "%Y-%m-%dT%H:%M:%SZ", gmtime(&csvtime));


    // write header
    fprintf(fp,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n\
<TrainingCenterDatabase xmlns=\"http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2 http://www.garmin.com/xmlschemas/TrainingCenterDatabasev2.xsd\">\n<Activities>\n <Activity Sport=\"%s\">\n",sport);
    fprintf(fp,"  <Id>%s</Id>\n",stime);
    fprintf(fp,"  <Lap StartTime=\"%s\">\n",stime);
    if (avgHR)
      fprintf(fp,"  <AverageHeartRateBpm>\n   <Value>%d</Value>\n  </AverageHeartRateBpm>\n",iavgHR);
    if (maxHR)
      fprintf(fp,"  <MaximumHeartRateBpm>\n   <Value>%d</Value>\n  </MaximumHeartRateBpm>\n",imaxHR);
    //fprintf(fp,"  <TotalTimeSeconds>0</TotalTimeSeconds>\n"); // dummy
    //fprintf(fp,"  <DistanceMeters>0.000000</DistanceMeters>\n"); // dummy
    //fprintf(fp,"  <Calories>0</Calories>\n"); // dummy
    fprintf(fp,"  <Track>\n");

    for (i=0;i < ptCnt;i++) {
      makeTrackPoint(&pts[i]);
      // write track point
      if (fprintf(fp,line) != strlen(line)) {
	return 2;
      }
    }

    // write footer
    fprintf(fp,"  </Track>\n  </Lap>\n </Activity>\n</Activities>\n</TrainingCenterDatabase>\n");

    fclose(fp);
    return 0;
  }
  return 1;
}

int main(int argc,char *argv[])
{
  printf(VERSTR);
  if (argc != 2) {
    printf("Usage: csv2tcx infile\n");
    return 1;
  }

  char outfn[MAX_PATH];
  int slen = strlen(argv[1]);
  strcpy(outfn,argv[1]);
  if (outfn[slen-4] == '.') {
    strcpy(outfn + slen -3,"tcx");
  }
  else {
    strcat(outfn,".tcx");
  }
  printf("Converting %s -> %s\n",argv[1],outfn);

  ptCnt = 0;
  if (!readCSV(argv[1])) {
    writeTCX("Other",outfn);
  }

  return 0;
}
