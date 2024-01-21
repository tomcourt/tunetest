#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <complex.h>
#include <assert.h>
extern int _getch();

// Fow Windows only, to allow console to use ANSI, add the following to the Registry
// HKEY_CURRENT_USER\Console\ add the DWORD VirtualTerminalLevel to equal 1 

// For PC comment out the following section:
// For Mac or Linux, uncomment out this following section:
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
int _getch(void) {
    char chbuf;
    int nread;
    struct termios oldstate, newstate;
    tcgetattr(0, &oldstate);
    newstate = oldstate;
    newstate.c_lflag &= ~ICANON;
    newstate.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &newstate);
    do {
        nread = read(0, &chbuf, 1);
    } while (nread <= 0);
    tcsetattr(0, TCSANOW, &oldstate);
    return chbuf;
}


#define ANSI_CLEAR_SCREEN_AND_HOME "\033[2J\033[H"
// #define ANSI_HIGHLIGHT "\033[30;47m"         // gray background
#define ANSI_HIGHLIGHT "\033[7m"             // reverse video
#define ANSI_NORMAL "\033[0m"

#define N_RESISTANCES sizeof(resistances)/sizeof(double)
#define N_REACTANCES sizeof(reactances)/sizeof(double)
#define N_FREQS sizeof(freqs)/sizeof(double)
#define NATIVE sizeof(freqs)/sizeof(double)


double freqs[] = {1.82E6, 3.54E6, 7.02E6, 14.1E6, 18.1E6, 21.4E6, 24.9E6, 28.3E6};
double resistances[21];
double reactances[41];
int capacitors[128];
int inductors[128];

double bestSWRs[N_FREQS][N_RESISTANCES][N_REACTANCES];
double tunedSWRs[2][N_FREQS][N_RESISTANCES][N_REACTANCES];
double nativeSWRs[N_RESISTANCES][N_REACTANCES];
int tuneCounts[2][N_FREQS][N_RESISTANCES][N_REACTANCES];
int digitMap[N_RESISTANCES][N_REACTANCES];
int histogram[12];      // [0] = '-' for < 0        [11] = '+' for >= 10
double scale[] = {0.1, 0.2, 0.5, 1.0};
double tunedMapSWR[2][128][128];
char tunedMapHit[2][128][128]; 


int calc_capacitors(int values)
{
    int c[] = {22, 47, 100, 220, 470, 1000, 2220};      // ATU-10 C array in parralel, pF
    int sum = 0;
    for (int i=0; i<sizeof(c)/sizeof(int); i++)
        if (values & (1<<i))
            sum += c[i];
    return sum;
}


double calc_inductors(int values)
{
    int l[] = {100, 220, 450, 1000, 2200, 4500, 10000};  // ATU-10 L array in series, nH
    int sum = 0;
    for (int i=0; i<sizeof(l)/sizeof(int); i++)
        if (values & (1<<i))
            sum += l[i];
    return sum;
}


double calcSWR(double complex imp)
{
    double gamma = cabs((imp - 50.0) / (imp + 50.0));
    if (gamma > 0.999999 && gamma < 1.000001)
        return INFINITY;
    double n = (1.0+gamma) / (1.0-gamma);
    if (n != n)     // NaN
        n = INFINITY;
    assert(n >= 1.0);
    return n;
}


//  High-pass L Step up
//   ┌──Zc───┬───>
//  Zin      Zl   <─┐
//   └───────┴───>  ZhpLsu
double complex ZhpLsu(double freq, double complex Zin, int L, int C)
{
    double omega = 2.0*M_PI*freq;
    double complex Zc = 1.0/(I*omega*C*1E-12);
    double complex Zl = I*omega*L*1E-9;
    // Zout = (Zin+Zc)*Zl / ((Zin+Zc)+Zl)
    double complex Z1 = Zin + Zc;
    double complex Zout = Z1*Zl / (Z1+Zl);
    return Zout;
} 


//  High-pass L Step down
//   ┌───┬──Zc───>
//  Zin  Zl       <─┐
//   └───┴───────>  ZhpLsd
double complex ZhpLsd(double freq, double complex Zin, int L, int C)
{
    double omega = 2.0*M_PI*freq;
    double complex Zc = 1.0/(I*omega*C*1E-12);
    double complex Zl = I*omega*L*1E-9;
    double complex Zout = Zin*Zl / (Zin+Zl) + Zc;
    return Zout;
}


double exaustiveSearch(double freq, double complex Zin)
{
    double smallest = INFINITY;
    for (int linx=0; linx<sizeof(inductors)/sizeof(int); linx++) {
        for (int cinx=0; cinx<sizeof(capacitors)/sizeof(int); cinx++) {
            double swr = calcSWR(ZhpLsu(freq, Zin, inductors[linx], capacitors[cinx]));
            if (swr < smallest)
                smallest = swr;
            assert(swr >= 1);
            swr = calcSWR(ZhpLsd(freq, Zin, inductors[linx], capacitors[cinx]));
            if (swr < smallest)
                smallest = swr;
            assert(swr >=1);
        }
    }
    return smallest;
}


extern int SWR;
extern char ind, cap, SW;
double complex tuneImp = 50.0;
double tuneFreq = 0;
double SWRexact = 1.0;
int tuneCount;
int lastTuneC, lastTuneL, lastTuneCsw;

void delay_ms(int ms)
{
}

void get_SWR()
{ 
    if (SWRexact >= 10)
        SWR = 999;
    else
    {
        SWR = (int)(100.0 * SWRexact + 0.5);
        if (SWR > 999)
            SWR = 999;
    }
}

void Relay_set(char l, char c, char i)
{
    assert(l >= 0);
    assert(c >= 0);
    if (i) // TODO - which side should the C be on the LC?, guessing for now, probably doesn't matter
        SWRexact = calcSWR(ZhpLsu(tuneFreq, tuneImp, inductors[l], capacitors[c]));
    else
        SWRexact = calcSWR(ZhpLsd(tuneFreq, tuneImp, inductors[l], capacitors[c]));

    tuneCount++;
    tunedMapHit[i][l][c]++;
    lastTuneC = c;
    lastTuneL = l;
    lastTuneCsw = i;
}


void resetTune(double freq, double complex Zin)
{
    tuneFreq = freq;
    tuneImp = Zin;
    tuneCount = 0;
    ind = 0;
    cap = 0;
    SW = 0;
    SWR = 999;
    SWRexact = 10;
}


void viewLCTuneMap(int tuneChoice, double freq, int resistanceInx, int reactanceInx)
{
    int xsize = 64, ysize = 32;
    int lcsize = 128;
    int xoff = 0, yoff = 0, capsw = 0;
    int sinx = 0, zoom = 4, relocate = 0;
    double complex Zin = 0;

retune:
    Zin = resistances[resistanceInx] + I*reactances[reactanceInx];

    for (int linx=0; linx<lcsize; linx++) {
        for (int cinx=0; cinx<lcsize; cinx++) {
            tunedMapSWR[0][linx][cinx] = calcSWR(ZhpLsd(freq, Zin, inductors[linx], capacitors[cinx]));
            tunedMapSWR[1][linx][cinx] = calcSWR(ZhpLsu(freq, Zin, inductors[linx], capacitors[cinx]));
        }
    }

    resetTune(freq, Zin);  
    memset(tunedMapHit, 0, sizeof(tunedMapHit));
    extern void tune1();
    extern void tune2();
    if (tuneChoice == 1)
        tune1();
    else
        tune2();    
    
    for (;;) {
        if (zoom == 4)
            xsize = 32;
        else
            xsize = 64;
        double curScale = scale[sinx];
        printf(ANSI_CLEAR_SCREEN_AND_HOME); 
        printf("y=%d, x=%d, cSw=%d, zoom=%d   %.1f%+.1fj   tunes=%d   #=%.2f SWR\n", yoff, xoff, capsw, zoom, creal(Zin), cimag(Zin), tuneCount, SWR/100.0);
        for (int y=yoff, r=0; y<yoff+ysize*zoom; y+=zoom, r++) {
            for (int x=xoff; x<xoff+xsize*zoom; x+=zoom) {
                // find lowest value in zoom x zoom square
                double swr = 1000;
                int wasHit = 0;
                int wasPrime = 0;
                for (int y1=y; y1<y+zoom; y1++) 
                    for (int x1=x; x1<x+zoom; x1++) {
                        double t = tunedMapSWR[capsw][y1][x1];
                        if (t < swr)
                            swr = t;
                        if (tunedMapHit[capsw][y1][x1] != 0)
                            wasHit = 1;
                        if (lastTuneC == x1 && lastTuneL == y1 && lastTuneCsw == capsw)
                            wasPrime = 1;
                    }
                if (swr < -1000)
                    swr = -1000;
                if (swr > 1000)
                    swr = 1000;   
                swr = swr - 1.0;
                int n = (int)floor(swr / curScale);
                if (wasHit)
                    printf(ANSI_HIGHLIGHT);
                printf("%c", (wasPrime) ? '#' : ((n < 10) ? ('0' + n) : '+'));
                if (wasHit)
                    printf(ANSI_NORMAL);
            }
            const char *format = "  %c =%c%4.1f%c";
            if (r < 10)
                printf(format, '0'+r, ' ', 1.0+curScale*r, '+');
            else if (r == 10)
                printf(format, '+', ' ', 1.0+curScale*r, '+');

            switch(r)
            {
            case 12:    printf(relocate ? "  RELOCATE" : "  SCROLL");               break;
            case 13:    printf("    "  ANSI_HIGHLIGHT "i" ANSI_NORMAL);             break;
            case 14:    printf("  " ANSI_HIGHLIGHT "j" ANSI_NORMAL " + " ANSI_HIGHLIGHT "k" ANSI_NORMAL);   break;
            case 15:    printf("    "  ANSI_HIGHLIGHT "m" ANSI_NORMAL);             break;
            case 17:    printf("  PAGE");                                           break;
            case 18:    printf("    "  ANSI_HIGHLIGHT "I" ANSI_NORMAL);             break;
            case 19:    printf("  " ANSI_HIGHLIGHT "J" ANSI_NORMAL " + " ANSI_HIGHLIGHT "K" ANSI_NORMAL);   break;
            case 20:    printf("    "  ANSI_HIGHLIGHT "M" ANSI_NORMAL);             break;
            case 22:    printf("  "    ANSI_HIGHLIGHT "c" ANSI_NORMAL "ap switch"); break;
            case 23:    printf("  "    ANSI_HIGHLIGHT "z" ANSI_NORMAL "oom+");      break;
            case 24:    printf("  "    ANSI_HIGHLIGHT "Z" ANSI_NORMAL "oom-");      break;
            case 25:    printf("  "    ANSI_HIGHLIGHT "s" ANSI_NORMAL "cale+");     break; 
            case 26:    printf("  "    ANSI_HIGHLIGHT "s" ANSI_NORMAL "cale-");     break; 
            case 27:    printf("  "    ANSI_HIGHLIGHT "r" ANSI_NORMAL "elocate");   break;
            case 28:    printf("  "    ANSI_HIGHLIGHT "q" ANSI_NORMAL "uit");       break;
            }
            printf("\n");
        }

        char ch = _getch();
        if (relocate) {
            switch (ch) {
                case 'i': 
                    if (resistanceInx > 0)
                        resistanceInx--;
                    goto retune;
                 case 'm':
                    if (resistanceInx < sizeof(resistances)/sizeof(resistances[0])-1)
                        resistanceInx++;
                    goto retune;
                case 'j': 
                    if (reactanceInx > 0)
                        reactanceInx--;
                    goto retune;
                case 'k':
                    if (reactanceInx < sizeof(reactances)/sizeof(reactances[0])-1)
                        reactanceInx++;
                    goto retune;
            }
        }
        switch (ch) {
            case 'c':
                capsw = !capsw;
                break;
            case 'i': 
                yoff -= zoom;
                break;
            case 'I': 
                yoff -= ysize*zoom/2;
                break;
            case 'm':
                yoff += zoom;
                break;
            case 'M':
                yoff += ysize*zoom/2;
                break;
            case 'j': 
                xoff -= zoom;
                break;
            case 'J': 
                xoff -= xsize*zoom/2;
                break;
            case 'k':
                xoff += zoom;
                break;
            case 'K':
                xoff += xsize*zoom/2;
                break;
            case 'S':
                if (--sinx < 0)
                    sinx = sizeof(scale)/sizeof(double) - 1;
                break;
            case 's':
                if (++sinx >= sizeof(scale)/sizeof(double))
                    sinx = 0;   
                break;
            case 'r':
                relocate = !relocate;
                break;
            case 'q':
                return;
            case 'z':
                if (zoom < 4)
                    zoom *= 2;
                break;
           case 'Z':
                if (zoom > 1)
                    zoom /= 2;
                break;
        }
        if (xoff < 0)
            xoff = 0;
        if (yoff < 0)
            yoff = 0;
        if (xoff > lcsize-xsize*zoom)
            xoff = lcsize-xsize*zoom;
        if (yoff > lcsize-ysize*zoom)
            yoff = lcsize-ysize*zoom;
    }
}


void viewResistReactMaps()
{
    // all count graphs have to be at the end of this list after count1_gr
    enum { tuned1_less_best, tuned2_less_best, tuned2_less_tuned1, tuned1_less_tuned2, best_gr, count1_gr, count2_gr, count2_less_count1, count1_less_count2 };
    const char *graphs[] = { "Tuned1 - Best", "Tuned2 - Best", "Tuned2 - Tuned1", "Tuned1 - Tuned2", "Best", "Count1", "Count2", "Count2 - Count1", "Count1 - Count2" };
    int finx = 0;
    int sinx = 0;
    int graph = 0;
    int zoom = 0;

    for (;;) {
        double curScale = scale[sinx];
        for (int i=0; i<sizeof(histogram)/sizeof(histogram[0]); i++)
            histogram[i] = 0;
        double cumulative = 100.0;

        for (int r=0; r<N_RESISTANCES; r++) {
            for (int j=0; j<N_REACTANCES; j++) {
                double swr = 0; 
                double tuned1 = tunedSWRs[0][finx][r][j];
                double tuned2 = tunedSWRs[1][finx][r][j];
                double best =  bestSWRs[finx][r][j];
                double count1 = (double)(tuneCounts[0][finx][r][j]) / 10;
                double count2 = (double)(tuneCounts[1][finx][r][j]) / 10;
                
                if (finx == NATIVE)
                    swr = nativeSWRs[r][j] - 1.0;
                else {
                    switch (graph) {
                    case tuned1_less_best:      swr = tuned1 - best;    break;
                    case tuned2_less_best:      swr = tuned2 - best;    break;
                    case tuned2_less_tuned1:    swr = tuned2 - tuned1;  break;
                    case tuned1_less_tuned2:    swr = tuned1 - tuned2;  break;
                    case best_gr:               swr = best - 1.0;       break;   
                    case count1_gr:             swr = count1;           break;
                    case count2_gr:             swr = count2;           break; 
                    case count2_less_count1:    swr = count2 - count1;  break;
                    case count1_less_count2:    swr = count1 - count2;  break;
                    }
                }
                
                // make sure swr is in a safe range before converting to an integer
                if (swr < -1000)
                    swr = -1000;
                if (swr > 1000)
                    swr = 1000;   
                int n = (int)floor(swr / curScale);

                digitMap[r][j] = n;
                if (n < 0)
                    histogram[0]++;
                else if (n >= 10)
                    histogram[11]++;
                else
                    histogram[n+1]++;
            }
        }

        printf(ANSI_CLEAR_SCREEN_AND_HOME); 
        if (zoom) {
            for (int n=0; n<2; n++) {
                printf("     ");
                for (int j=0; j<N_REACTANCES; j++) {
                    char s[10];
                    sprintf(s, "%d  ", abs((int)j));
                    if (j%2)
                        printf(ANSI_HIGHLIGHT);
                    printf("%c", s[n]);
                    if (j%2)
                        printf(ANSI_NORMAL);
                }
                printf("\n");
            }
        }
        else {
            if (finx == NATIVE)
                printf("     Native\n");
            else
                printf("     %5.2f MHz     %s\n", freqs[finx]/1E6, graphs[graph]);
            printf("     -500      -50       0         +50       +500\n");
        }

        for (int r=0; r<N_RESISTANCES; r++) {
            // show a digit map of the complex impedance
            if (zoom)
                printf("%2d   ", r);
            else
                printf("%3.0f  ", resistances[r]);
            for (int j=0; j<N_REACTANCES; j++) {
                int n = digitMap[r][j];
                if (zoom && j%2)
                    printf(ANSI_HIGHLIGHT);
                printf("%c", (n < 0) ? '-' : ((n < 10) ? ('0' + n) : '+'));
                if (zoom && j%2)
                    printf(ANSI_NORMAL);
            }

            // show the digits legend and histogram on the right of the digit map
            double offset = (graph==best_gr) ? 1.0 : 0.0;
            double legendScale = (graph>=count1_gr) ? 10*curScale : curScale;
            const char *format = (graph>=count1_gr || curScale==1.0) ? "    %c =%c%4.0f%c" : "    %c =%c%4.1f%c";
            if (r == 0)
                printf(format, '-', '<', offset, '-');
            else if (r < 11)
                printf(format, '0'+r-1, ' ', offset+legendScale*(r-1), '+');
            else if (r == 11)
                printf(format, '+', ' ', offset+legendScale*(r-1), '+');

            switch (r)
            {
                case 13:   printf("    " ANSI_HIGHLIGHT "f" ANSI_NORMAL "requency+");  break;
                case 14:   printf("    " ANSI_HIGHLIGHT "F" ANSI_NORMAL "requency-");  break;
                case 15:   printf("    " ANSI_HIGHLIGHT "g" ANSI_NORMAL "raph+");      break;
                case 16:   printf("    " ANSI_HIGHLIGHT "G" ANSI_NORMAL "raph+");      break;
                case 17:   printf("    " ANSI_HIGHLIGHT "s" ANSI_NORMAL "cale+");       break;
                case 18:   printf("    " ANSI_HIGHLIGHT "S" ANSI_NORMAL "cale-");       break;
                case 19:   printf("    " ANSI_HIGHLIGHT "1" ANSI_NORMAL "tune");        break;
                case 20:   printf("    " ANSI_HIGHLIGHT "2" ANSI_NORMAL "tune");        break;
            }

            // show the digits legend and histogram on the right of the digit map
            double percent = 100.0*histogram[r]/(N_RESISTANCES*N_REACTANCES);
            cumulative -= percent;
            if (r <= 11)
                printf(" %5.1f%%  %5.1f%%\n", percent, cumulative);
            else           
                printf("\n");
        }        

        if (zoom) {
            printf("y, x: ");
            fflush(stdout);
            int y, x;
            scanf("%d,%d", &y, &x);
            viewLCTuneMap(zoom, freqs[finx], y, x);
            zoom = 0;
            continue;
        }
        char ch = _getch();
        switch (ch) {
            case 'F': 
                if (--finx < 0)
                    finx = N_FREQS-1;
                break;
            case 'f':
                if (++finx > N_FREQS)
                    finx = 0;
                break;
            case 'S':
                if (--sinx < 0)
                    sinx = sizeof(scale)/sizeof(double) - 1;
                break;
            case 's':
                if (++sinx >= sizeof(scale)/sizeof(double))
                    sinx = 0;   
                break;
            case 'G':
                if (--graph < 0)
                    graph = sizeof(graphs)/sizeof(graphs[0])-1;
                break;
            case 'g':
                if (++graph >= sizeof(graphs)/sizeof(graphs[0]))
                    graph = 0;
                break;
            case '1':
                zoom = 1;
                break;
            case '2':
                zoom = 2;
                break;
        }
    }
}


int main()
{
    printf(ANSI_CLEAR_SCREEN_AND_HOME "Searching for best L/C matches at all points...\n");

    for (int i=0; i<N_RESISTANCES; i++)
        resistances[i] = 5.0*pow(10.0,i/10.0);

    int r2size = N_REACTANCES/2;
    for (int i=1; i<=r2size; i++) {
        reactances[r2size-i] = -5.0*pow(10.0,i/10.0);
        reactances[r2size+i] = 5.0*pow(10.0,i/10.0);
    }
    for (int i=0; i<sizeof(capacitors)/sizeof(int); i++)
        capacitors[i] = calc_capacitors(i);
    capacitors[0] = 1;
    for (int i=0; i<sizeof(inductors)/sizeof(int); i++)
        inductors[i] = calc_inductors(i);
    inductors[0] = 1;

    for (int r=0; r<N_RESISTANCES; r++) 
        for (int j=0; j<N_REACTANCES; j++) 
            nativeSWRs[r][j] = calcSWR(resistances[r]+I*reactances[j]);

    for (int f=0; f<N_FREQS; f++) 
        for (int r=0; r<N_RESISTANCES; r++) 
            for (int j=0; j<N_REACTANCES; j++) 
                bestSWRs[f][r][j]  = exaustiveSearch(freqs[f], resistances[r]+I*reactances[j]);

   for (int i=0; i<N_FREQS; i++) {
        for (int r=0; r<N_RESISTANCES; r++) {
            for (int j=0; j<N_REACTANCES; j++) {
                resetTune(freqs[i], resistances[r]+I*reactances[j]);
                extern void tune1();
                tune1(); 
                tunedSWRs[0][i][r][j] = SWRexact;
                tuneCounts[0][i][r][j] = tuneCount;

                resetTune(freqs[i], resistances[r]+I*reactances[j]);
                extern void tune2();
                tune2(); 
                tunedSWRs[1][i][r][j] = SWRexact;
                tuneCounts[1][i][r][j] = tuneCount;
            }
        }
    }

    viewResistReactMaps();
    return 0;
}