// ATU-10 tuning algorithm, prototype Tom Court, AC0NY
// Linear search, one axis at a time with MRU 0.2
// Implements something like this https://en.wikipedia.org/wiki/Coordinate_descent

#include "stdio.h"
#include "stdbool.h"
#include "tune.h"
extern int _getch();

// this allows us to have two 'tune' functions, an new one here and another old one in tune.c
#define tune() tune2()

 
// Wild a$$ guesses, in order of most likely to least likely to hit
static const char wagSw[72] = {  1,  0,  1,  0,  1,  1,  1,  0,  1,  0,  1,  0,  1,  1,  0,  1,  1,  1,  1,  0,  0,  0,  1,  1,  1,  0,  1,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  0,  1,  1,  1,  0,  1,  1,  0,  1,  1,  1,  1,  1,  0,  0,  1,  1,  1,  1,  1,  0,  1,  1,  1,  1,  0,  1,  1,  1,  1,};
static const char wagL[72]  = { 41, 60,  9, 69,  5, 37, 11, 24, 54,  7,  9,  3, 56, 76, 92,  3, 15, 12, 25,  6,  4, 22, 22,  7,121, 12,  8, 28, 25, 64,  2, 15, 26, 81,106, 42,  5, 14, 17, 30, 79, 10,  3,  6, 30, 32, 51, 13, 19, 11, 64,125, 10, 16, 85, 11, 16,  4,  6, 28, 41, 95, 78, 12, 32, 55,119,  4,  3, 11, 16, 46,};
static const char wagC[72]  = { 34, 51,  3,  7,  2,  2,115,  3,  6, 13,  1,  1,  1, 20, 16,  3,  7,  1, 13,  2,  4,  8,  1,  1,  1,120,  1,  4, 24, 14,  5,  1,  3,  2, 10,  7,  1,  2, 15, 10,  4,  1,  3,  1,  1,  8,  2,  1,  1,  6, 26,  9,  7,  1, 11, 15,  5,  1,  2,  3, 22, 10,  4,  2,  1,  5,  8, 43,  1,  1,  4,  2,};


static int testSwLC(char l, char c, char swCap)
{
   ind = l;
   cap = c;
   SW = swCap;
   Relay_set(ind, cap, SW);
   delay_ms(5);
   get_SWR();
   return SWR;
}


// Most Recently Used (MRU) SWRs, use stored SWR reading for most recent tune attempts during hill a hill climb
static char MRU_l[8];
static char MRU_c[8];
static int MRU_swr[8];
static char n_MRUs = 0;


// Add to start of the MRU list, will push older MRUs down by 1 stopping at length
static int add_to_MRU(char l, char c, int swrt, char length)
{
   for (int i=length; i>0; i--) {
      MRU_l[i] = MRU_l[i-1];
      MRU_c[i] = MRU_c[i-1];
      MRU_swr[i] = MRU_swr[i-1];
   }
   MRU_l[0] = ind = l;
   MRU_c[0] = cap = c;
   MRU_swr[0] = SWR = swrt;
   return swrt;
}


// Get the SWR for a L,C point. Avoid re-tuning the same points by maintaining a most recently used list
static int SWRgetMRU(char l, char c)
{
   for (int i=0; i<n_MRUs; i++)
      if (MRU_l[i] == l && MRU_c[i] == c) {
         // found a matching entry, moving entry to front, adding will overwrite the older copy    
         return add_to_MRU(MRU_l[i], MRU_c[i], MRU_swr[i], i);
      }
   if (++n_MRUs > sizeof(MRU_l)-1)
      n_MRUs = sizeof(MRU_l)-1;        // TODO this -1 math seems wrong, likely wasting an MRU entry, but needs fixing in add_to_MRU or will overrun
   return add_to_MRU(l, c, testSwLC(l, c, SW), n_MRUs);
}


// Try new point and update globals if better, true if no change (isBest)
static bool bestSWR(char l, char c, bool isEqual)
{
   if (l==(char)128 || l==(char)255 || c==(char)128 || c==(char)255)
      return true;
   int oldSWR = SWR;
   char oldInd = ind;
   char oldCap = cap;
   int swrt = SWRgetMRU(l, c);
   if (swrt < oldSWR) 
      return false;
   if (isEqual && swrt == oldSWR)
      return false;
   SWR = oldSWR;
   ind = oldInd;
   cap = oldCap;
   return true;
}


// Hill climb along an axis by finding best of current and dist to either direction along axis (true ind, false cap)
static bool hillClimbAxis(char l, char c, char dist, bool axis)
{
   char vary = (axis ? l : c);
   // less dist or half the distance to the edge
   int minus = (vary>=dist) ? vary-dist : vary/2;   
   int plus = vary + dist;

   bool isUpTest = true;
   bool isDownTest = true;
   if (plus < 128 && vary != plus)
      isUpTest = (axis ? bestSWR(plus, c, false) : bestSWR(l, plus, false));
   if (vary != minus)
      isDownTest = (axis ? bestSWR(minus, c, true) : bestSWR(l, minus, true));
   return isUpTest && isDownTest;
}


// Found a hill, try hill climbing to the top
// first do a course search (0) and then a fine search (1)
// (this reduces hill climbing switching)
static void hillClimb4Way() 
{
   for (int i=0; i<2; i++) {
      char startL;
      char startC;
      do {
         startL = ind;
         startC = cap;
         // Optimize each axis
         for (int j=0; j<2; j++) {
            int dist = 32;
            do {
               if (hillClimbAxis(ind, cap, dist, (j==0))) {
                  // start with course moves only 
                  if (i == 0)
                     break;
                  dist /= 2;
               }
            } while (dist);
         }
         // keep going until moving no longer causes improvement
      } while (ind != startL || cap != startC);
   }
}


// Found a hill, try hill climbing to the top
// Try stepping in all 8 directions, but only one step at a time
// (as this is inefficent, only do so if we couldn't optimize it earlier)
static void hillClimb8Way() 
{
   char startL;
   char startC;
   do {
      startL = ind;
      startC = cap;
      bestSWR(startL+1, startC+1, false);
      bestSWR(startL+1, startC,   false);
      bestSWR(startL,   startC+1, false);
      bestSWR(startL-1, startC+1, false);
      bestSWR(startL+1, startC-1, false);
      bestSWR(startL,   startC-1, false);
      bestSWR(startL-1, startC,   false);
      bestSWR(startL-1, startC-1, false);
      // keep going until moving no longer causes improvement
   } while (ind != startL || cap != startC);
   testSwLC(ind, cap, SW);
}


void tune() 
{
   char avoidSw = 2;       // start by not avoiding either cap setting
   char saveInd;
   char saveCap;
   int saveSWR = 1001;
   for (size_t i=0; i<sizeof(wagSw); i++)
      if (wagSw[i] != avoidSw && testSwLC(wagL[i], wagC[i], wagSw[i]) < 999) {
         SW = wagSw[i];
         n_MRUs = 0;       // clear the MRU (because it only covers one cap switch side)
         hillClimb4Way();  // hill climb from the starting point provided
         if (SWR < 120 || avoidSw != 2) 
            break;

         // tried this cap switch side, limit search to other side
         avoidSw = SW;
         saveInd = ind;
         saveCap = cap;
         saveSWR = SWR;
      }
   
   // need to tune back to best found from either cap side
   if (saveSWR < SWR)
      testSwLC(saveInd, saveCap, avoidSw);
   else
      testSwLC(ind, cap, SW);
   if (SWR >= 120 && SWR < 999) 
      hillClimb8Way();
}