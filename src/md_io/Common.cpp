#include "Common.h"
#include <time.h>

string gettimestring() {
   time_t rawtime;
   struct tm * timeinfo;
   char buffer [80];
   time ( &rawtime );
   timeinfo = localtime ( &rawtime );

   strftime (buffer,80,"%Y-%m-%d_%H-%M-%S",timeinfo);
   string date = buffer;

   return date+".out";
}
