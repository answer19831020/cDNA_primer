/************************************************************************************\
*                                                                                    *
* Copyright (c) 2014, Dr. Eugene W. Myers (EWM). All rights reserved.                *
*                                                                                    *
* Redistribution and use in source and binary forms, with or without modification,   *
* are permitted provided that the following conditions are met:                      *
*                                                                                    *
*  · Redistributions of source code must retain the above copyright notice, this     *
*    list of conditions and the following disclaimer.                                *
*                                                                                    *
*  · Redistributions in binary form must reproduce the above copyright notice, this  *
*    list of conditions and the following disclaimer in the documentation and/or     *
*    other materials provided with the distribution.                                 *
*                                                                                    *
*  · The name of EWM may not be used to endorse or promote products derived from     *
*    this software without specific prior written permission.                        *
*                                                                                    *
* THIS SOFTWARE IS PROVIDED BY EWM ”AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,    *
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND       *
* FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL EWM BE LIABLE   *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS  *
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY      *
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING     *
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN  *
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                      *
*                                                                                    *
* For any issues regarding this software and its use, contact EWM at:                *
*                                                                                    *
*   Eugene W. Myers Jr.                                                              *
*   Bautzner Str. 122e                                                               *
*   01099 Dresden                                                                    *
*   GERMANY                                                                          *
*   Email: gene.myers@gmail.com                                                      *
*                                                                                    *
\************************************************************************************/

/*******************************************************************************************
 *
 *  Utility for displaying the overlaps in a .las file in a variety of ways including
 *    a minimal listing of intervals, a cartoon, and a full out alignment.
 *
 *  Author:    Gene Myers
 *  Creation:  July 2013
 *  Last Mod:  Jan 2015
 *
 *******************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "DB.h"
#include "align.h"

int ORDER(const void *l, const void *r)
{ int x = *((int32 *) l);
  int y = *((int32 *) r);
  return (x-y);
}

static char *Usage[] =
    { "[-carUF] [-i<int(4)>] [-w<int(100)>] [-b<int(10)>] ",
      "         [<src1:db|dam> [ <src2:db|dam> ] <align:las> [ <reads:range> ... ]"
    };

#define LAST_READ_SYMBOL  '$'

/*static char *Usage[] =
    { "[-mcoU] [-(a|r|f):<db>] [-i<int(4)>] [-w<int(100)>] [-b<int(10)>] ",
      "       <align:las> [ <reads:range> ... ]"
    };
*/

int main(int argc, char *argv[])
{ HITS_DB   _db1, *db1 = &_db1; 
  HITS_DB   _db2, *db2 = &_db2;
  Overlap   _ovl, *ovl = &_ovl;
  Alignment _aln, *aln = &_aln;

  FILE   *input;
  int64   novl;
  int     tspace, tbytes, small;
  int     reps, *pts;

  int     ALIGN, FALCON, CARTOON, OVERLAP, REFERENCE, FLIP;
  int     ICE_FL;  // used to denote whether to filter for only FL-to-FL alignments
  int     INDENT, WIDTH, BORDER, UPPERCASE;
  int     M4OVL;
  int     SEED_MIN;
  int     ISTWO;

  //  Process options

  { int    i, j, k;
    int    flags[128];
    char  *eptr;

    ARG_INIT("LA4Ice")

    ALIGN     = 0;
    M4OVL     = 0;
    REFERENCE = 0;
    FALCON    = 0;
    ICE_FL    = 0; // if 1, means that only display records/alignments for FL-to-FL alignment
    INDENT    = 4;
    WIDTH     = 100;
    BORDER    = 10;
    SEED_MIN  = 8000;
    ISTWO     = 0; // if 1, db1 and db2 are different

    j = 1;
    for (i = 1; i < argc; i++)
      if (argv[i][0] == '-')
        switch (argv[i][1])
        { default:
            ARG_FLAGS("carmUF")
            break;
          case 'i':
            ARG_NON_NEGATIVE(INDENT,"Indent")
            break;
          case 'w':
            ARG_POSITIVE(WIDTH,"Alignment width")
            break;
          case 'b':
            ARG_NON_NEGATIVE(BORDER,"Alignment border")
            break;
          case 'H':
            ARG_POSITIVE(SEED_MIN,"seed threshold (in bp.s)")
            break;
          case 'r':
            REFERENCE = 1;
          case 'a':
            ALIGN = 1;
          case 'f':
            FALCON = 1;
            if (argv[i][2] != ':')
              { fprintf(stderr,"%s: Unrecognizable option %s\n",Prog_Name,argv[i]);
                exit (1);
              }
            if (Open_DB(argv[i]+3,db1))
              exit (1);
            Trim_DB(db1);
            break;
          case 'q':
            if (argv[i][2] != ':')
              { fprintf(stderr,"%s: Unrecognizable option %s\n",Prog_Name,argv[i]);
                exit (1);
              }
            //fprintf(stderr, "reading second db: %s\n", argv[i]);
            if (Open_DB(argv[i]+3,db2))
              exit (1);
            Trim_DB(db2);
            ISTWO = 1;
            break;
        }
      else
        argv[j++] = argv[i];
    argc = j;

    if (ISTWO == 0) {
        db2 = db1;
    }

    UPPERCASE = flags['U'];
    ALIGN     = flags['a'];
    CARTOON   = flags['c'];
    REFERENCE = flags['r'];
    FLIP      = flags['F']
    M4OVL = flags['m'];
    ICE_FL = flags['E'];



    if (argc <= 1)
      { fprintf(stderr,"Usage: %s %s\n",Prog_Name,Usage[0]);
        fprintf(stderr,"       %*s %s\n",(int) strlen(Prog_Name),"",Usage[1]);
        exit (1);
      }
  }

  //  Process read index arguments into a sorted list of read ranges

  pts  = (int *) Malloc(sizeof(int)*2*argc,"Allocating read parameters");
  if (pts == NULL)
    exit (1);

  reps = 0;
  if (argc > 2)
    { int   c, b, e;
      char *eptr, *fptr;

      for (c = 2; c < argc; c++)
        { if (argv[c][0] == '#')
            { fprintf(stderr,"%s: # is not allowed as range start, '%s'\n",
                      Prog_Name,argv[c]);
              exit (1);
            }
          else
            { b = strtol(argv[c],&eptr,10);
              if (b < 1)
                { fprintf(stderr,"%s: Non-positive index?, '%d'\n",Prog_Name,b);
                  exit (1);
                }
            }
          if (eptr > argv[c])
            { if (*eptr == '\0')
                { pts[reps++] = b;
                  pts[reps++] = b;
                  continue;
                }
              else if (*eptr == '-')
                { if (eptr[1] == '#')
                    { e = INT32_MAX;
                      fptr = eptr+2;
                    }
                  else
                    e = strtol(eptr+1,&fptr,10);
                  if (fptr > eptr+1 && *fptr == 0 && eptr[1] != '-')
                    { pts[reps++] = b;
                      pts[reps++] = e;
                      if (b > e)
                        { fprintf(stderr,"%s: Empty range '%s'\n",Prog_Name,argv[c]);
                          exit (1);
                        }
                      continue;
                    }
                }
            }
          fprintf(stderr,"%s: argument '%s' is not an integer range\n",Prog_Name,argv[c]);
          exit (1);
        }

      qsort(pts,reps/2,sizeof(int64),ORDER);

      b = 0;
      for (c = 0; c < reps; c += 2)
        if (b > 0 && pts[b-1] >= pts[c]-1) 
          { if (pts[c+1] > pts[b-1])
              pts[b-1] = pts[c+1];
          }
        else
          { pts[b++] = pts[c];
            pts[b++] = pts[c+1];
          }
      pts[b++] = INT32_MAX;
      reps = b;
    }
  else
    { pts[reps++] = 1;
      pts[reps++] = INT32_MAX;
    }

  //  Initiate file reading and read (novl, tspace) header
  
  { char  *over, *pwd, *root;

    pwd   = PathTo(argv[1]);
    root  = Root(argv[1],".las");
    over  = Catenate(pwd,"/",root,".las");
    input = Fopen(over,"r");
    if (input == NULL)
      exit (1);

    fread(&novl,sizeof(int64),1,input);
    fread(&tspace,sizeof(int),1,input);

    if (tspace <= TRACE_XOVR)
      { small  = 1;
        tbytes = sizeof(uint8);
      }
    else
      { small  = 0;
        tbytes = sizeof(uint16);
      }

    if (!(FALCON || M4OVL)) 
      { printf("\n%s: ",root);
        Print_Number(novl,0,stdout);
        printf(" records\n");
      }

    free(pwd);
    free(root);
  }

  //  Read the file and display selected records
  
  { int        j;
    uint16    *trace;
    Work_Data *work;
    int        tmax;
    int        in, npt, idx, ar;
    int64      tps;
    int64      p_aread = -1;
    char buffer[65536];

    if (ALIGN || FALCON)
      { work = New_Work_Data();
  
        aln->path = &(ovl->path);
        aln->aseq = New_Read_Buffer(db2);
        aln->bseq = New_Read_Buffer(db1);
      }
    else
      work = NULL;

    tmax  = 1000;
    trace = (uint16 *) Malloc(sizeof(uint16)*tmax,"Allocating trace vector");
    if (trace == NULL)
      exit (1);

    in  = 0;
    npt = pts[0];
    idx = 1;

    //  For each record do

    for (j = 0; j < novl; j++)

       //  Read it in

      { Read_Overlap(input,ovl);
        if (ovl->path.tlen > tmax)
          { tmax = 1.2*ovl->path.tlen + 100;
            trace = (uint16 *) Realloc(trace,sizeof(uint16)*tmax,"Allocating trace vector");
            if (trace == NULL)
              exit (1);
          }
        ovl->path.trace = (void *) trace;
        Read_Trace(input,ovl,tbytes);

        //  Determine if it should be displayed

        ar = ovl->aread+1;
        if (in)
          { while (ar > npt)
              { npt = pts[idx++];
                if (ar < npt)
                  { in = 0;
                    break;
                  }
                npt = pts[idx++];
              }
          }
        else
          { while (ar >= npt)
              { npt = pts[idx++];
                if (ar <= npt)
                  { in = 1;
                    break;
                  }
                npt = pts[idx++];
              }
          }
        if (!in)
          continue;

        if (OVERLAP && !FALCON)
          { if (ovl->path.abpos != 0 && ovl->path.bbpos != 0)
              continue;
            if (ovl->path.aepos != ovl->alen && ovl->path.bepos != ovl->blen)
              continue;
          }

        if (OVERLAP && FALCON)
          { if (ovl->path.abpos > 1000 && ovl->path.bbpos > 1000)
              continue;
            if (ovl->alen - ovl->path.aepos > 1000 && ovl->blen - ovl->path.bepos > 1000)
              continue;
            if (ovl->alen < SEED_MIN)
              continue;

          }

        // move calculation of sStart and sEnd (bbpos, bepos) up here since both ICE and M4OVL uses it
        int64 bbpos, bepos;
        if (COMP(ovl->flags)) {
            bbpos = (int64) ovl->blen - (int64) ovl->path.bepos;
            bepos = (int64) ovl->blen - (int64) ovl->path.bbpos;
        } else {
            bbpos = (int64) ovl->path.bbpos;
            bepos = (int64) ovl->path.bepos;
        }

        if (ICE_FL)
        {
            // only contiue if it is a full-length-to-full-length mapping, as in:
            // (1) qStart < 200 and sStart < 200
            // (2) qEnd + 50 > qLen and sEnd + 50 > qLen
            if (ovl->path.abpos > 200 || bbpos > 200)
                continue;
            if (ovl->path.aepos + 50 < ovl->alen)
                continue;
            if (bepos + 50 < ovl->blen)
                continue;
        }
        

        //  Display it

        if (M4OVL) {
            double acc;

            tps = ((ovl->path.aepos-1)/tspace - ovl->path.abpos/tspace);
            aln->alen  = ovl->alen;
            aln->blen  = ovl->blen;
            aln->flags = ovl->flags;


            /*if (COMP(ovl->flags)) {
                bbpos = (int64) ovl->blen - (int64) ovl->path.bepos;
                bepos = (int64) ovl->blen - (int64) ovl->path.bbpos;
            } else {
                bbpos = (int64) ovl->path.bbpos;
                bepos = (int64) ovl->path.bepos;
            }*/
            acc = 100-(200. * ovl->path.diffs)/( ovl->path.aepos - ovl->path.abpos + ovl->path.bepos - ovl->path.bbpos);

            printf("%09lld %09lld %lld %5.2f ", (int64) ovl->aread, (int64) ovl->bread,  (int64) bbpos - (int64) bepos, acc);
            printf("0 %lld %lld %lld ", (int64) ovl->path.abpos, (int64) ovl->path.aepos, (int64) ovl->alen);
            printf("%d %lld %lld %lld ", COMP(ovl->flags), bbpos, bepos, (int64) ovl->blen);
            if ( ((int64) ovl->blen < (int64) ovl->alen) && ((int64) ovl->path.bbpos < 1) && ((int64) ovl->blen - (int64) ovl->path.bepos < 1) ) 
              {
                printf("contains\n");
              }
            else if ( ((int64) ovl->alen < (int64) ovl->blen) && ((int64) ovl->path.abpos < 1) && ((int64) ovl->alen - (int64) ovl->path.aepos < 1) ) 
              {
                printf("contained\n");
              }
            else 
              {
                printf("overlap\n");
              }

        }
        /*
        if (FALCON) {

            tps = ((ovl->path.aepos-1)/tspace - ovl->path.abpos/tspace);
            aln->alen  = ovl->alen;
            aln->blen  = ovl->blen;
            aln->flags = ovl->flags;

            
            if (p_aread == -1) {
                Load_Read(db1,ovl->aread,aln->aseq,2);
                printf("%08d %s\n", ovl->aread, aln->aseq);
                p_aread = ovl->aread;
            }
            if (p_aread != ovl -> aread ) {
                printf("+ +\n");
                Load_Read(db1,ovl->aread,aln->aseq,2);
                printf("%08d %s\n", ovl->aread, aln->aseq);
                p_aread = ovl->aread;
            }

            Load_Read(db1,ovl->bread,aln->bseq,0);
            p_aread = ovl->aread;
            if (COMP(aln->flags))
              Complement_Seq(aln->bseq);
            Upper_Read(aln->bseq);
            strncpy( buffer, aln->bseq + ovl->path.bbpos, (int64) ovl->path.bepos - (int64) ovl->path.bbpos );
            buffer[ (int64) ovl->path.bepos - (int64) ovl->path.bbpos - 1] = '\0';
            printf("%08d %s\n", ovl->bread, buffer);

        }
        */

        if (CARTOON || ALIGN)
            printf("\n");
        if (!(FALCON || M4OVL)) 
          {
            Print_Number((int64) ovl->aread+1,10,stdout);
            printf("  ");
            Print_Number((int64) ovl->bread+1,9,stdout);
            if (COMP(ovl->flags))
              printf(" c");
            else
              printf(" n");
            printf("   [");
            Print_Number((int64) ovl->path.abpos,6,stdout);
            printf("..");
            Print_Number((int64) ovl->path.aepos,6,stdout);
            printf("] x [");
            Print_Number((int64) ovl->path.bbpos,6,stdout);
            printf("..");
            Print_Number((int64) ovl->path.bepos,6,stdout);
            printf("]");
         }
/*
{ int u;
  if (small)
    for (u = 0; u < ovl->path.tlen-1; u++)
    // for (u = 1; u < ovl->path.tlen-2; u += 2)
      printf("  %3d\n",((uint8 *) ovl->path.trace)[u]);
  else
    for (u = 0; u < ovl->path.tlen-1; u++)
    // for (u = 1; u < ovl->path.tlen-2; u += 2)
      printf("  %3d\n",((uint16 *) ovl->path.trace)[u]);
}
*/

        // tps = ((ovl->path.aepos-1)/tspace - ovl->path.abpos/tspace) + 1;
        tps = ((ovl->path.aepos-1)/tspace - ovl->path.abpos/tspace);
        if (ALIGN)
          { if (small)
              Decompress_TraceTo16(ovl);
            aln->alen  = ovl->alen;
            aln->blen  = ovl->blen;
            aln->flags = ovl->flags;
            Load_Read(db2,ovl->aread,aln->aseq,0);
            Load_Read(db1,ovl->bread,aln->bseq,0);
            if (COMP(aln->flags))
              Complement_Seq(aln->bseq);
            Compute_Trace_PTS(aln,work,tspace);
            /*
            if (CARTOON)
              { printf("  (");
                Print_Number(tps,3,stdout);
                printf(" trace pts)\n\n");
                Print_ACartoon(stdout,aln,INDENT);
              }
            else
              {
                printf(" :   = ");
                Print_Number((int64) ovl->path.diffs,6,stdout);
                printf(" diffs  (");
                Print_Number(tps,3,stdout);
                printf(" trace pts)\n");
              }
             */
            if (REFERENCE)
              Print_Reference(stdout,aln,work,INDENT,WIDTH,BORDER,UPPERCASE,5);
            else
              Print_Alignment(stdout,aln,work,INDENT,WIDTH,BORDER,UPPERCASE,5);
          }
        else if (CARTOON)
          { printf("  (");
            Print_Number(tps,3,stdout);
            printf(" trace pts)\n\n");
            Print_OCartoon(stdout,ovl,INDENT);
          }
        if (!(FALCON || M4OVL) ) 
          {
            printf(" :   < ");
            Print_Number((int64) ovl->path.diffs,6,stdout);
            printf(" diffs  (");
            Print_Number(tps,3,stdout);
            printf(" trace pts)\n");
          }
          
      }
    if (FALCON) {
        printf("+ +\n");
        printf("- -\n");
    }
    free(trace);
    if (ALIGN)
      { free(aln->bseq-1);
        free(aln->aseq-1);
        Free_Work_Data(work);
      }
  }

  exit (0);
}
