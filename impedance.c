#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>

#define ANSI_CLEAR_SCREEN_AND_HOME "\033[2J\033[H"

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


int getch(void) {
    char chbuf;
    struct termios oldstate, newstate;

    tcgetattr(0, &oldstate);
    newstate = oldstate;
    newstate.c_lflag &= ~ICANON;
    newstate.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &newstate);
    read(0, &chbuf, 1);
    tcsetattr(0, TCSANOW, &oldstate);
    return chbuf;
}


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


double complex imp2smith(double complex imp)
{
    return (imp - 50.0) / (imp + 50.0);
    // inverse if needed is -50*(s+1)/(s-1)
}


double calcSWR(double complex imp)
{
    double smith = cabs(imp2smith(imp));
    return (1.0+smith) / (1.0-smith);
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


double exaustiveSearch(double freq, complex double Zin)
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
double complex tuneImp = 50.0;
int tuneFreqInx = 0;
double SWRexact = 1.0;
int tuneCount;

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
    if (i) // TODO - which side should the C be on the LC?, guessing for now, probably doesn't matter
        SWRexact = calcSWR(ZhpLsu(freqs[tuneFreqInx], tuneImp, inductors[l], capacitors[c ^ 0x7f]));
    else
        SWRexact = calcSWR(ZhpLsd(freqs[tuneFreqInx], tuneImp, inductors[l], capacitors[c ^ 0x7f]));
    assert(SWRexact >= 1.0);
    tuneCount++;
}


int main()
{
    double scale[] = {0.1, 0.2, 0.5, 1.0};
    // all count graphs have to be at the end of this list after count1_gr
    enum { tuned1_less_best, tuned2_less_best, tuned2_less_tuned1, best_gr, count1_gr, count2_gr, count2_less_count1 };
    const char *graphs[] = { "Tuned1 - Best", "Tuned2 - Best", "Tuned2 - Tuned1", "Best", "Count1", "Count2", "Count2 - Count1" };
    int finx = 0;
    int sinx = 0;
    int graph = 0;
    int negate = 0;

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

   for (tuneFreqInx=0; tuneFreqInx<N_FREQS; tuneFreqInx++) {
        for (int r=0; r<N_RESISTANCES; r++) {
            for (int j=0; j<N_REACTANCES; j++) {
                tuneImp = resistances[r]+I*reactances[j];

                tuneCount = 0;
                extern void tune1();
                tune1(); 
                tunedSWRs[0][tuneFreqInx][r][j] = SWRexact;
                tuneCounts[0][tuneFreqInx][r][j] = tuneCount;

                tuneCount = 0;
                extern void tune2();
                tune2(); 
                tunedSWRs[1][tuneFreqInx][r][j] = SWRexact;
                tuneCounts[1][tuneFreqInx][r][j] = tuneCount;
            }
        }
    }

    for (;;) {
        double curScale = scale[sinx];
        if (negate)
            curScale = -curScale;
        for (int i=0; i<sizeof(histogram)/sizeof(histogram[0]); i++)
            histogram[i] = 0;
        double cumulative = 0.0;

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
                    case best_gr:               swr = best - 1.0;       break;   
                    case count1_gr:             swr = count1;           break;
                    case count2_gr:             swr = count2;           break; 
                    case count2_less_count1:    swr = count2 - count1;  break;
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
        if (finx == NATIVE)
            printf("Native\n");
        else
            printf("%5.2f MHz     %s\n", freqs[finx]/1E6, graphs[graph]);
        printf("     -500      -50       0         +50       +500\n");

        char low_ch = '-';
        char high_ch = '+';
        if (negate) {
            low_ch = '+';
            high_ch = '-';
        }
    
        for (int r=0; r<N_RESISTANCES; r++) {
            // show a digit map of the complex impedance
            printf("%3.0f  ", resistances[r]);
            for (int j=0; j<N_REACTANCES; j++) {
                int n = digitMap[r][j];
                printf("%c", (n < 0) ? low_ch : ((n < 10) ? ('0' + n) : high_ch));
            }

            // show the digits legend and histogram on the right of the digit map
            double offset = (graph==best_gr) ? 1.0 : 0.0;
            double legendScale = (graph>=count1_gr) ? 10*curScale : curScale;
            const char *format = (graph>=count1_gr || curScale==1.0) ? "    %c =%c%4.0f %c" : "    %c =%c%4.1f %c";
            if (r == 0)
                printf(format, low_ch, negate?'>':'<', offset, low_ch);
            else if (r < 11)
                printf(format, '0'+r-1, ' ', offset+legendScale*(r-1), high_ch);
            else if (r == 11)
                printf(format, high_ch, ' ', offset+legendScale*(r-1), high_ch);

            // show the digits legend and histogram on the right of the digit map
            double percent = 100.0*histogram[r]/(N_RESISTANCES*N_REACTANCES);
            cumulative += percent;
            if (r <= 11)
                printf("  %5.1f%%  %5.1f%%\n", percent, cumulative);
            else           
                printf("\n");
        }
        printf("f)frequency+, F)frequency-, g)raph+, G)raph-, s)cale+, S)cale-, n)egate\n");

        char ch = getch();
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
            case 'n':
                negate = !negate;
        }
    }

    return 0;
}