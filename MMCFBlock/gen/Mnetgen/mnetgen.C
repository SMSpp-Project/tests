/*--------------------------------------------------------------------------*/
/*---------------------------- File mnetgen.C ------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

/*-------------------------------- MNETGEN -----------------------------------

  MNETGN WAS DEVELOPED AT SOUTHERN METHODIST UNIVERSITY, DALLAS, TX, DURING
  AUTUMN 1976 BY DR J. L. KENNINGTON AND A. I. ALI.

  REVISED    JUNE 1986     JLK

  - - - - - translated by f2c (version of 23 April 1993  18:34:30) - - - - - -

  C++ polished and enhanced by

         Antonio Frangioni
			  Dipartimento di Informatica
         Universita' di Pisa

  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  DATA STRUCTURE FOR MULTICOMMODITY GENERATOR - MNETGN

  FOR EACH PROBLEM TO BE GENERATED THE FOLLOWING DATA MUST
  BE PROVIDED:

  ISEED   - SEED FOR RANDOM NO GENERATOR, NONZERO
  NPROB   - PROBLEM NUMBER
  NODES   - NUMBER OF NODES
  DENS    - (minimum) NUMBER OF ARCS (if it is necessary to make the problem
            feasible, more than DENS arcs may be generated)
  NSORC   - NUMBER OF SOURCES (PURE AND TRANSHIPMENT)
  NSINK   - NUMBER OF SINKS (PURE AND TRANSHIPMENT)
  NCOMM   - NUMBER OF COMMODITIES --- N O T E --- NCOMM MUST BE .LE. NODES
  NTSORC  - NUMBER OF TRANSHIPMENT SOURCES
  NTSINK  - NUMBER OF TRANSHIPMENT SINKS
  LCHAIN  - LENGTH OF LONGEST CHAIN IN SKELETON
  MINCST  - LOWER BOUND ON COST
  MAXCST  - UPPER BOUND ON COST
  MINSUP  - LOWER BOUND ON TOTAL SUPPLY
  MAXSUP  - UPPER BOUND ON TOTAL SUPPLY
  MINBND  - LOWER BOUND ON INDIVIDUAL ARC BOUNDS
  MAXBND  - UPPER BOUND ON INDIVIDUAL ARC BOUNDS
  MINCAP  - LOWER BOUND ON MUTUAL ARC CAPACITIES
  MAXCAP  - UPPER BOUND ON MUTUAL ARC CAPACITIES
  BHICST  - PERCENT OF ARCS WITH HIGH COST
  BBND    - PERCENT OF ARCS WITH INDIVIDUAL ARC BOUNDS
  BCAP    - PERCENT OF ARCS WITH MUTUAL ARC CAPACITIES

  The following compile-time switch(es) are provided:

            / 0  =>  use the original built-in random number generator
  W_RND  =  |
            \ 1  =>  use the standard drand48() function of stdlib

	   / 0 if the standard 3-files mnetgen output format is used
  FOUR_F = |
           | 1 if a fourth file *.nod is added with the same meaning of
	   |   that in JL format, i.e., <commodities> , <nodes>, <arcs>
	   |   <capacitated arcs>: in this case, the informations about
	   |   node supplies are contained in the file *.sup (as in the
	   \   JL standard) rather than in the *.nod

  The output file formats are (each record, fields tab-separated):

  Mutual capacity file (*.mut): 

  < mutual capacity pointer > , < mutual capacity >

  Arc file (*.arc):

  < arc name > , < from node > , < to node > , < commodity > , < cost > ,
  < capacity > , < mutual capacity pointer >

  Arc name is an integer between 1 and the number of arcs (differently from
  the original mnetgen format), that is necessary to distinguish between
  multiple instances of an arc (i, j) for the same commodity, that are
  permitted

  Node supply file (*.nod if FOUR_F == 0, *.sup otherwise):

  < node > , < commodity > , < supply >

  Problem description file (*.nod, only if FOUR_F == 1)

  < commodities > , < nodes > , < arcs > , < capacitated arcs >

----------------------------------------------------------------------------*/

#define W_RND 1

#define FOUR_F 1

#define Number double

// all the floating point numbers of the problem are generically defined
// of type "Number": by setting this macro as double rather than as float,
// one can change the precision of all numbers used within the program

/*--------------------------------------------------------------------------*/

#if( W_RND )

 #define rand_( i ) drand48()

#else

/* THIS GENERATOR IS BASED ON THEOREM A, PAGE 15, VOL. 2 OF
   KNUTH, THE ART OF COMPUTER PROGRAMMING; NOTE - 2**23 - 1 = 8388607 */

double rand_( long &i )
{
 i = ( i * 5 + 7 ) % 8388607;
 return( Number( i ) / Number( 8388607 ) );
 }

#endif

/*--------------------------------------------------------------------------*/

inline long min( long x , long y )
{ 
 return( x <= y ? x : y );
 }

/*--------------------------------------------------------------------------*/

inline long max( long x , long y )
{
 return( x >= y ? x : y );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 if( argc < 3 ) {
  cerr << "Usage: mnetgen <input file> <output file>" << endl;
  exit( 1 );
  }

 fstream Mut;
 Mut.open( argv[ 1 ] , ios::in );

 if( ! Mut ) {
  cerr << "Error opening file " << argv[ 1 ] << endl;
  exit( 1 );
  }

 long iseed , nprob , nodes , dens , nsorc , nsink , ncomm , ntsorc , ntsink ,
      lchain , minbnd , maxbnd , mincap , maxcap , mincst , maxcst , minsup ,
      maxsup;

 Number bhicst , bbnd , bcap;

 // INPUT OF ISEED AND INITIALIZATION OF THE RANDOM NO. GENERATOR

 Mut >> iseed;

 long i;

 #if( W_RND )
  srand48( iseed );
 #else
  // THE FOLLOWING SEGMENT WARMS UP THE GENERATOR

  iseed = iseed % 500 + 150;
  for( i = iseed ; i-- ; )
   rand_( iseed );
 #endif

 Mut >> nprob;

 // input of problem parameters

 Mut >> nodes;
 Mut >> dens;
 Mut >> nsorc;
 Mut >> nsink;
 Mut >> ncomm;
 Mut >> ntsorc;
 Mut >> ntsink;
 Mut >> lchain;

 Mut >> mincst;
 Mut >> maxcst;
 Mut >> minsup;
 Mut >> maxsup;
 Mut >> minbnd;
 Mut >> maxbnd;
 Mut >> mincap;
 Mut >> maxcap;
 Mut >> bhicst;
 Mut >> bbnd;
 Mut >> bcap;

 Mut.close();

 // INPUT CHECKS AND ERROR MESSAGES

 if( nsorc + nsink > nodes ) {
  cerr << "INPUT ERROR - SOURCES  +  SINKS .GT. NODES" << endl;
  exit( 1 );
  }

 if( ntsorc > nsorc ) {
  cerr << "INPUT ERROR - NTSORC GREATER THAN NSORC" << endl;
  exit( 1 );
  }

 if( ntsink > nsink ) {
  cerr << "INPUT ERROR - NTSINK GREATER THAN NSINK" << endl;
  exit( 1 );
  }

 if( ( nodes < 0 ) || ( dens < 0 ) || ( nsorc < 0 ) || ( nsink < 0 ) ||
     ( ncomm < 0 ) || ( ntsorc < 0 ) || ( ntsink < 0 )
    ) {
  cerr << "INPUT ERROR - A VARIABLE IN INPUT" << endl;
  exit( 1 );
  }

 if( mincst > maxcst ) {
  cerr << "INPUT ERROR - MINCST.GT.MAXCST" << endl;
  exit( 1 );
  }

 if( minsup > maxsup ) {
  cerr << "INPUT ERROR - MINSUP.GT.MAXSUP" << endl;
  exit( 1 );
  }

 if( minbnd > maxbnd ) {
  cerr << "INPUT ERROR - MINBND.GT.MAXBND" << endl;
  exit( 1 );
  }

 if( mincap > maxcap ) {
  cerr << "INPUT ERROR - MINCAP.GT.MAXCAP" << endl;
  exit( 1 );
  }

 if( ( bhicst < 0 ) || ( bhicst > 100 ) ||
     ( bbnd < 0 ) || ( bbnd > 100 ) || ( bcap < 0 ) || ( bcap > 100 ) ) {
  cerr << "INPUT ERROR - BHICST OR BBND OR BCAP NOT IN (0,100)" << endl;
  exit( 1 );
  }

 if( minsup < nsorc ) {
  cerr << "INPUT ERROR - MINSUP.LT.NSORC" << endl;
  exit( 1 );
  }

 long **isup, **idem;

 long *muts, *iflag, *ipred, *lsinks, *mut , *tsup;

 // initializing memory

 idem = new long*[ nsink ];

 for( i = 0 ; i < nsink ; i++ )
  idem[ i ] = new long[ ncomm ];

 // IDEM( NSINK , NCOMM )

 isup = new long*[ nsorc ];

 for( i = 0 ; i < nsorc ; )
  isup[ i++ ] = new long[ ncomm + 1 ];

 // ISUP( NSORC , NCOMM + 1 )

 mut   = new long[ max( lchain , nodes ) ];
 muts  = new long[ nsink ];
 iflag = new long[ nodes ];
 ipred = new long[ nodes ];

 lsinks = new long[ nsink ];
 tsup   = new long[ ncomm ];

 // SET CONSTANTS

 long npsorc = nsorc - ntsorc;
 long npsink = nsink - ntsink;
 long lpsor = nsorc - ntsorc;
 long itsor = lpsor + 1;
 long lpt = nodes - nsink;
 long itsin = lpt + 1;
 long ltsin = nodes - nsink + ntsink;
 long ipsin = ltsin + 1;
 long ntrans = lpt - nsorc;
 long ktran = ltsin - lpsor;

 Number pcap = bcap / 100;
 Number phicst = bhicst / 100;
 Number pbnd = bbnd / 100;
 Number ibddif = maxbnd - minbnd;
 Number ibsdif = maxsup - minsup;
 Number ictdif = maxcst - mincst;
 Number icpdif = maxcap - mincap;
 Number icbdif = maxbnd - minbnd;

 long is = 0;
 long narc = 0;
 long arcc = 0;

 long j , l , last , komm , nocom , namelen , lsorc ,
      nsksr , len , nf , nt , iss , ks , ksp;

 //  SET AVERAGE CHAIN LENGTH AND NUMBER OF SINKS CONNECTED TO EACH

 Number fnave1 = Number( ktran ) / Number( nsorc );
 Number fnave2 = Number( nsink ) / Number( nsorc );

 long k = 0;

 for( ; k < ncomm ; k++ ) {
  // GENERATE TOTAL SUPPLY FOR EACH COMMODITY

  tsup[ k ] = long( rand_( iseed ) * ibsdif ) + minsup;

  // DISTRIBUTE TOTAL SUPPLY AMONG SOURCES

  ks = tsup[ k ] / nsorc;

  // INITIALIZE SUPPLY FOR NODE I, COMMODITY K

  for( i = 0 ; i < nsorc ; )
   isup[ i++ ][ k ] = 0;

  for( i = 0 ; i < nsorc ; ) {
   ksp = long( ks * rand_( iseed ) + 1 );
   j = long( nsorc * rand_( iseed ) );

   isup[ i++ ][ k ] += ksp;
   isup[ j ][ k ] += ks - ksp;
   }

  j = long( nsorc * rand_( iseed ) );

  isup[ j ][ k ] += tsup[ k ] - ks * nsorc;

  // Computing MAX

  for( i = 0 ; i < nsorc ; i++ )
   isup[ i ][ k ] = max( isup[ i ][ k ] , nsink );

  } // end for( nocomm )

 // ADD TOTAL SUPPLIES AT EACH NODE I AND STORE SUM IN ISUP(I,NCOMMM)

 for( i = 0 ; i < nsorc ; ) {
  long itotal = 0;

  for( k = 0 ; k < ncomm ; )
   itotal += isup[ i ][ k++ ];

  isup[ i++ ][ ncomm ] = itotal;
  }

 // RESET FLAGS AND BEFORE SETS

 for( i = nodes ; i-- ; )
  iflag[ i ] = 0;

 // INITIALIZE DEMANDS

 for( i = nsink ; i-- ; )
  for( k = ncomm ; k-- ; )
   idem[ i ][ k ] = 0;

 namelen = strlen( argv[ 2 ] );

 char *Name = new char[ namelen + 5 ];

 strcpy( Name , argv[ 2 ] );
 strcpy( Name + namelen , ".mut" );

 Mut.open( Name , ios::out );

 if( ! Mut ) {
  cerr << "Error opening file " << Name << endl;
  exit( 1 );
  }

 Mut.precision( 4 );

 strcpy( Name + namelen , ".arc" );
 fstream Arc( Name , ios::out );

 if( ! Arc ) {
  cerr << "Error opening file " << Name << endl;
  exit( 1 );
  }

 Arc.precision( 4 );

 // main loop

 for( lsorc = 1 ; lsorc <= nsorc ; lsorc++ ) {
  for( i = lpt ; i < nodes ; )
   iflag[ i++ ] = 0;

  // DETERMINE CHAIN LENGTH

  len = 0;
  last = lsorc;

  if( npsorc + npsink != nodes ) {
   len = min( long( fnave1 * 3 * rand_( iseed ) + 1 ) , lchain );

   // SELECT TRANSSHIPMENT NODES           

   for( i = 0 ; i < len ; ) {
    do
     l = long( ktran * rand_( iseed ) ) + itsor;
    while( ( l == last ) || ( l == lsorc ) || iflag[ l - 1 ] );

    ipred[ i++ ] = last = l;

    if( l < itsin )
     iflag[ l - 1 ] = 1;

    }  // end for( i )

   // CONNECT ALL PURE TRANSSHIPMENT NODES NOT CONNECTED TO LAST CHAIN

   if( lsorc == nsorc )
    for( i = nsorc ; i < lpt ; )
     if( ! iflag[ i++ ] )
      ipred[ len++ ] = last = i;

   }  // end if( npsorc + npsink != nodes )

  // RANDOMLY SELECT THE NUMBER OF SINKS TO BE CONNECTED TO THIS CHAIN

  nsksr = min( long( fnave2 * 3 * rand_( iseed ) + 1 ) , nsink );

  // SELECT SINKS AT RANDOM

  for( i = 0 ; i < nsksr ; ) {
   do
    l = long( nsink * rand_( iseed ) ) + itsin;
   while( ( l == last ) || iflag[ l - 1 ] );

   lsinks[ i++ ] = l;
   iflag[ l - 1 ] = 1;
   }

  if( lsorc == nsorc ) {
   // CONNECT ALL REMAINING SINKS TO THE LAST CHAIN

   for( i = lpt ; i < nodes ; )
    if( ++i != last )
     if( ! iflag[ i - 1 ] )
      lsinks[ nsksr++ ] = i;
   }

  // DETERMINE ARCS TO BE MUTUALLY CAPACITATED AND ASSIGN NUMBERS
  // AND CAPACITIES

  if( npsorc + npsink != nodes ) {
   for( i = 0 ; i < len ; )
    if( rand_( iseed ) > pcap )
     mut[ i++ ] = 0;
    else
     Mut << ( mut[ i++ ] = ++is ) << "\t" << isup[ lsorc - 1 ][ ncomm ]
         << endl;
   }

  for( i = 0 ; i < nsksr ; )
   if( rand_( iseed ) > pcap )
    muts[ i++ ] = 0;
   else
    Mut << ( muts[ i++ ] = ++is ) << "\t" << isup[ lsorc - 1 ][ ncomm ]
        << endl;

  // GENERATE DEMANDS AND COSTS FOR EACH COMMODITY

  for( komm = 0 ; komm < ncomm ; komm++ ) {
   long tmpan = arcc;

   ks = isup[ lsorc - 1 ][ komm ] / nsksr;

   for( k = 0 ; k < nsksr ; k++ ) {
    j = long( nsksr * rand_( iseed ) );
    ksp = long( ks * rand_( iseed ) + 1 );

    idem[ lsinks[ k ] - lpt - 1 ][ komm ] += ksp;
    idem[ lsinks[ j ] - lpt - 1 ][ komm ] += ks - ksp;
    }

   j = long( nsksr * rand_( iseed ) );
   idem[ lsinks[ j ] - lpt - 1 ][ komm ] += isup[ lsorc - 1 ][ komm ]
                                          - ks * nsksr;

   // DEVELOP COST AND CAPACITIES FOR ARCS

   nf = lsorc;

   if( npsorc + npsink != nodes )
    for( i = 0 ; i < len ; ) {
     nt = ipred[ i ];

     // DETERMINE IF ARC GETS HIGH COST

     Number cost = maxcst;

     if( rand_( iseed ) > phicst )
      cost = mincst + rand_( iseed ) * ictdif;

     // DETERMINE IF ARC HAS INDIVIDUAL CAPACITY

     Number cap = -1;

     if( rand_( iseed ) < pbnd )
      cap = Number( isup[ lsorc - 1 ][ komm ] );

     Arc << ( ++tmpan ) << "\t" << nf << "\t" << nt << "\t" << komm + 1
         << "\t" << cost << "\t" << cap << "\t" << mut[ i++ ] << endl;

     nf = nt;
     }

   for( i = 0 ; i < nsksr ; ) {
    nt = lsinks[ i ];

    // DETERMINE IF ARC GETS HIGH COST

    Number cost = maxcst;

    if( rand_( iseed ) > phicst )
     cost = mincst + rand_( iseed ) * ictdif;

    // DETERMINE IF ARC HAS INDIVIDUAL CAPACITY

    Number cap = -1;

    if( rand_( iseed ) < pbnd )
     cap = Number( isup[ lsorc - 1 ][ komm ] );

    Arc << ( ++tmpan ) << "\t" << last << "\t" << nt << "\t" << komm + 1
        << "\t" << cost << "\t" << cap << "\t" << muts[ i++ ] << endl;
    }

   narc += len + nsksr;

   }  // end for( komm )

  arcc += len + nsksr;

  }  // end for( lsorc )

 // ADD OTHER ARCS IF NARC .LT. DENS

 while( narc < dens ) {
  // SELECT FROM NODE AND TO NODE AT RANDOM
  // GET TO NODE

  nt = long( ( nodes - lpsor ) * rand_( iseed ) + itsor );

  // get from node

  do {
   nf = long( nodes * rand_( iseed ) );
   } while( ( nf == nt ) || ( nf == 0 ) );

  // SELECT NUMBER OF COMMODITIES AT RANDOM

  nocom = long( ncomm * rand_( iseed ) + 1 );

  // use tsup as a flag vector: never use twice the same commodity
  // for an arc with the current name

  for( i = 0 ; i < ncomm ; )
   tsup[ i++ ] = 0;

  // DETERMINE IF ARC HAS MUTUAL CAPACITY

  iss = 0;

  if( rand_( iseed ) <= pcap )
   Mut << ( iss = ++is ) << "\t" << mincap + long( rand_( iseed ) * icpdif )
       << endl;

  arcc++;

  for( i = 0 ; i < nocom ; i++ ) {
   // SELECT COMMODITY AT RANDOM

   do {
    komm = long( ncomm * rand_( iseed ) );
   } while( tsup[ komm ] );

   tsup[ komm ] = 1;

   // DETERMINE COST AND CAPACITY

   Number cost = mincst + rand_( iseed ) * ictdif;
   Number cap = -1;

   if( rand_( iseed ) < pbnd )
    cap = minbnd + long( rand_( iseed ) * icbdif );

   Arc << arcc << "\t" << nf << "\t" << nt << "\t" << komm + 1 << "\t"
       << cost << "\t" << cap << "\t" << iss << endl;
   }

  narc += nocom;

  }  // end while( narc < dens )

 Arc.close();
 Mut.close();

 // IS THE TOTAL NO OF ARCS WITH MUTUAL ARC CAPACITY

 // OUTPUT (node) REQUIREMENTS

 strcpy( Name + namelen , ".nod" );
 Arc.open( Name , ios::out );

 if( ! Arc ) {
  cerr << "Error opening file " << Name << endl;
  exit( 1 );
  }

 #if( FOUR_F )
  Arc << ncomm << endl << nodes << endl << arcc << endl << is << endl;
  Arc.close();

  strcpy( Name + namelen , ".sup" );
  Arc.open( Name , ios::out );

  if( ! Arc ) {
   cerr << "Error opening file " << Name << endl;
   exit( 1 );
   }
 #endif

 for( i = 1 ; i <= nsorc ; i++ )
  for( k = 1 ; k <= ncomm ; k++ )
   Arc << i << "\t" << k << "\t" << isup[ i - 1 ][ k - 1 ] << endl;

 for( i = 1 ; i <= nsink ; i++ )
  for( k = 1 ; k <= ncomm ; k++ )
   Arc << i + lpt << "\t" << k << "\t" << - idem[ i - 1 ][ k - 1 ] << endl;

 Arc.close();

 // deallocating memory

 delete[] Name;

 delete[] tsup;
 delete[] lsinks;
 delete[] ipred;
 delete[] iflag;
 delete[] muts;
 delete[] mut;

 for( i = 0 ; i < nsorc ; i++ )
  delete[] isup[ i ];

 delete[] isup;

 for( i = 0 ; i < nsink ; i++ )
  delete[] idem[ i ];

 delete[] idem;

 return( 0 );

 } // end main()

/*--------------------------------------------------------------------------*/
/*------------------------- End File mnetgen.C -----------------------------*/
/*--------------------------------------------------------------------------*/
