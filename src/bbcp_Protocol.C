/******************************************************************************/
/*                                                                            */
/*                       b b c p _ P r o t o c o l . C                        */
/*                                                                            */
/*(c) 2002-17 by the Board of Trustees of the Leland Stanford, Jr., University*/
/*      All Rights Reserved. See bbcp_Version.C for complete License Terms    */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* bbcp is free software: you can redistribute it and/or modify it under      */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* bbcp is distributed in the hope that it will be useful, but WITHOUT        */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with bbcp in a file called COPYING.LESSER (LGPL license) and file    */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <map>
#include "bbcp_Config.h"
#include "bbcp_Emsg.h"
#include "bbcp_Headers.h"
#include "bbcp_NetAddr.h"
#include "bbcp_Network.h"
#include "bbcp_Node.h"
#include "bbcp_Protocol.h"
#include "bbcp_Pthread.h"
#include "bbcp_Version.h"
 
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class bbcp_Login_Stream
{
public:

bbcp_Node *np;

      bbcp_Login_Stream(bbcp_Link *Net) 
           {np = new bbcp_Node(Net);}
     ~bbcp_Login_Stream()
           {if (np) delete np;}
};

/******************************************************************************/
/*                         L o c a l   O b j e c t s                          */
/******************************************************************************/

namespace
{

struct Cmp_Key
{

   bool operator()(char const *a, char const *b) const
   {
//cerr <<"Comp " <<a <<' ' <<b <<endl;
      return strcmp(a, b) < 0;
   }

};

std::map<char const *, bbcp_FileSpec *, Cmp_Key> CopySet;

}
  
/******************************************************************************/
/*                      E x t e r n a l   O b j e c t s                       */
/******************************************************************************/
  
extern bbcp_Config   bbcp_Cfg;

extern bbcp_Network  bbcp_Net;

extern bbcp_Version  bbcp_Version;

extern "C" {extern void *bbcp_FileSpecIndex(void *pp);}
  
/******************************************************************************/
/*                              S c h e d u l e                               */
/******************************************************************************/
  
int bbcp_Protocol::Schedule(bbcp_Node *Fnode, bbcp_FileSpec *Ffs, 
                                 char *Fcmd,           char *Ftype,
                            bbcp_Node *Lnode, bbcp_FileSpec *Lfs, 
                                 char *Lcmd,           char *Ltype,
                            bbcp_Node *Tnode)
{
   int retc;

   char *cbhost, *addOpt[2], *hDest;
   bool fcbh = false, noDNS = (bbcp_Cfg.Options & bbcp_NODNS) != 0;

// Start-up the first node
//
   if ((retc = Fnode->Run(Ffs->username, Ffs->hostname, Fcmd, Ftype)))
      return retc;

// Determine additional options
//
   if (Ftype[1] == 'R')
      {addOpt[0] = bbcp_Cfg.CopyOSrc;
       addOpt[1] = bbcp_Cfg.CopyOTrg;
      } else {
       addOpt[0] = bbcp_Cfg.CopyOTrg;
       addOpt[1] = bbcp_Cfg.CopyOSrc;
      }

// Send the arguments
//
   if ((retc = SendArgs(Fnode, Ffs, (char *)"none", 0, addOpt[0]))) return retc;

// Get the callback port from the first host
//
   if ((retc = getCBPort(Fnode, hDest))) return retc;

// Start the second node
//
   if ((retc = Lnode->Run(Lfs->username, Lfs->hostname, Lcmd, Ltype)))
      return retc;

// Compute callback hostname and reset callback port
//
        if (hDest && (noDNS || !(bbcp_Cfg.Options & bbcp_VERBOSE)))
           {cbhost = hDest; hDest = 0; fcbh = true;}
   else if (!(Ffs->hostname)) cbhost = bbcp_Cfg.MyAddr;
   else if (noDNS && !bbcp_NetAddrInfo::isHostName(Ffs->hostname))
           cbhost = Ffs->hostname;
   else fcbh = (cbhost = bbcp_Net.FullHostName(Ffs->hostname,(noDNS?1:0))) !=0;

// If the node send us it's actual host name use it preferentially and notify
// the user if we switched hosts if we need to do that.
//
   if (hDest)
      {if (cbhost)
          {if (!noDNS && (bbcp_Cfg.Options & bbcp_VERBOSE)
           &&  strcmp(cbhost, hDest)) Chk4Redir(cbhost, hDest);
           if (fcbh) free(cbhost);
          }
       cbhost = hDest;
       fcbh = true;
      }

// After all of this, verify we really do have a callback host
//
   if (!cbhost)
      {bbcp_Fmsg("Protocol", "Unable to determine the callback host!");
       return EADDRNOTAVAIL;
      }

// Send the arguments
//
   retc = SendArgs(Lnode, Lfs, cbhost, bbcp_Cfg.CBport, addOpt[1]);
   if (fcbh) free(cbhost);
   if (retc) return retc;

// The the final ending
//
   getEnd(Tnode);

// Wait for the any node to finish
//
   retc = Fnode->Wait(Lnode);

// All done
//
   sleep(1);     // Delay the Stop just/s e n to allow errors to be reported
   Fnode->Stop(retc==0);
   Lnode->Stop(retc==0);
   return retc;
}

/******************************************************************************/
/*                             g e t C B P o r t                              */
/******************************************************************************/

int bbcp_Protocol::getCBPort(bbcp_Node *Node, char *&hDest)
{
   char *wp;
   int  pnum;

// The remote program should hve started, get the call back port. New versions
// will also supply their host name which we always use if present.
//
   if ((wp = Node->GetLine()))
      {if ((wp = Node->GetToken()) && !strcmp(wp, "200")
       &&  (wp = Node->GetToken()) && !strcmp(wp, "Port:")
       &&  (wp = Node->GetToken())
       &&  bbcp_Cfg.a2n("callback port", wp, pnum, 0, 65535) == 0)
          {bbcp_Cfg.CBport = pnum;
           if ((wp = Node->GetToken()) && !strcmp(wp, "Host:")
           && (wp = Node->GetToken())) hDest = strdup(wp);
              else hDest = 0;
           return 0;
          }
      }

// Invalid response
//
   return bbcp_Fmsg("Protocol", "bbcp unexpectedly terminated on",
                     Node->NodeName());
}
  
/******************************************************************************/
/*                             s e t C B P o r t                              */
/******************************************************************************/
  
int bbcp_Protocol::setCBPort(int pnum)
{

// The port number and our host name simply get sent via standard out
//
   cout <<"200 Port: " <<pnum <<" Host: " <<bbcp_Cfg.MyHost <<endl;
   return 0;
}

/******************************************************************************/
/*                                g e t E n d                                 */
/******************************************************************************/
  
void bbcp_Protocol::getEnd(bbcp_Node *Node)
{
   char *wp, csVal[64];
   int n;

// Preset values
//
   Node->TotFiles = 0;
   Node->TotBytes = 0;

// The remote program should hve started, get checksums or ending tag
//
   while((wp = Node->GetLine())
      && (wp = Node->GetToken()) && !strcmp(wp, "200")
      && (wp = Node->GetToken()))
        {if (!strcmp(wp, "cks:"))
            {if (!(wp=Node->GetToken()) || (n = strlen(wp)) > sizeof(csVal))
                continue;
             strcpy(csVal, wp);
             if (!(wp = Node->GetToken())) continue;
             putCSV(Node->NodeName(), wp, csVal, n);
             continue;
            }

         if (strcmp(wp, "End:") || !(wp = Node->GetToken())) break;
         errno = 0; Node->TotFiles = strtol(wp, (char **)NULL, 10);
         if (errno) {Node->TotFiles = 0; break;}
         if (!(wp = Node->GetToken()))   break;
         errno = 0; Node->TotBytes = strtoll(wp, (char **)NULL, 10);
         if (errno) Node->TotBytes = 0;
         break;
        }
    DEBUG("At end files=" <<Node->TotFiles <<" bytes=" <<Node->TotBytes);
}
  
/******************************************************************************/
/* Source:                       P r o c e s s                                */
/******************************************************************************/
  
int bbcp_Protocol::Process(bbcp_Node *Node)
{
   bbcp_FileSpec *fp = bbcp_Cfg.srcSpec;
   pthread_t Tid;
   int rc, NoGo = 0;
   char *cp;

// If there is a r/t lock file, make sure it exists
//
   if ((bbcp_Cfg.Options & bbcp_RTCSRC) && bbcp_Cfg.rtLockf
   &&  (bbcp_Cfg.rtLockd = open(bbcp_Cfg.rtLockf, O_RDONLY)) < 0)
      {rc = errno, NoGo = 1;
       bbcp_Emsg("Config", rc, "opening lock file", bbcp_Cfg.rtLockf);
      }

// Make sure all of the source files exist at this location. If there is an
// error, defer exiting until after connecting to prevent a hang-up. We
// make sure that we are not trying to copy a directory.
//
   while(fp)
        {NoGo |= fp->Stat();
         if (fp->Info.Otype == 'd' && !(bbcp_Cfg.Options & bbcp_RECURSE)
         &&  fp->Info.size)
            {bbcp_Fmsg("Source", fp->pathname, "is a directory.");
             NoGo = 1; break;
            }

         fp = fp->next;
        }

// If this is a recursive list, do it in the bacground while we try to connect.
// This avoids time-outs when large number of files are enumerated.
//
   if (!NoGo && bbcp_Cfg.Options & bbcp_RECURSE)
      if ((rc = bbcp_Thread_Start(bbcp_FileSpecIndex, 0, &Tid)) < 0)
         {bbcp_Emsg("Protocol", rc, "starting file enumeration thread.");
          NoGo = 1;
         }

// Establish all connections
//
   if (Node->Start(this, (bbcp_Cfg.Options & bbcp_CON2SRC))
   ||  Node->getBuffers(0)) return 2;
   Local = Node;

// At this point, if we're running with the -r recursive option, our list of
// file specs (bbcp_Cfg.srcSpec) is being extended recursively to include
// all subdirs and their contents. We must wait for the thread to finish.
//
   if (!NoGo && bbcp_Cfg.Options & bbcp_RECURSE)
      NoGo = (bbcp_Thread_Wait(Tid) != 0);

// If there was a fatal error, we can exit now, the remote side will exit
//
   if (NoGo)
      {char buff[8];
       strcpy(buff, "eol\n");
       Remote->Put(buff, (ssize_t)4);
       Node->Stop();
       return 2;
      }
   rc = 0;

// Process all control connection requests and return
//
   while(!rc && Remote->GetLine())
      {if (!(cp = Remote->GetToken())) continue;
            if (!strcmp(cp, "flist")) rc = Process_flist();
       else if (!strcmp(cp, "get"))   rc = Process_get();
       else if (!strcmp(cp, "exit")) {rc = Process_exit(); break;}
       else {bbcp_Fmsg("Process", "Invalid command, '", cp, "'.");
             rc = 1;
            }
      }

// Dismantle this node and return
//
   Node->Stop();
   if (cp) return rc;
   bbcp_Fmsg("Source", "Unexpected end of control stream from",
                             Remote->NodeName());
   return 32;
}
 
/******************************************************************************/
/*                       P r o c e s s   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                          P r o c e s s _ e x i t                           */
/******************************************************************************/
  
int bbcp_Protocol::Process_exit()
{
    int  retc;
    char *wp;

// Get the return code (none indicates erro)
//
   if (!(wp = Remote->GetToken()))
      {bbcp_Fmsg("Process_exit", "missing return code."); retc = 22;}
      else if (bbcp_Cfg.a2n("return code", wp, retc, 0, 255)) retc = 22;

// Return to caller
//
   return (int)retc;
}
  
/******************************************************************************/
/*                         P r o c e s s _ f l i s t                          */
/******************************************************************************/
  
int bbcp_Protocol::Process_flist()
{
   bbcp_FileSpec *dp = bbcp_Cfg.srcPath;
   bbcp_FileSpec *fp = bbcp_Cfg.srcSpec;
   char buff[4096];
   int blen;
   const char eoltag[] = "eol\n";
   const int  eoltsz   = strlen(eoltag);

// Simply go through the list of paths and report them back to the caller

   while(dp)
      {if (!(dp->isEmpty))
          {if ((blen = dp->Encode(buff,(size_t)sizeof(buff))) < 0) return -1;
           if (Remote->Put(buff, blen)) return -1;
          }
       dp = dp->next;
      }

// Simply go through the list of files and report them back to the caller

   while(fp) 
      {if ((fp->Info.Otype == 'd' && !(fp->isEmpty))
       ||  (fp->Info.Otype != 'd' && *(fp->filename)))
          {if ((blen = fp->Encode(buff,(size_t)sizeof(buff))) < 0) return -1;
           if (Remote->Put(buff, blen)) return -1;
          }
       fp = fp->next;
      }

// Indicate that this is the end of the list
//
   return Remote->Put(eoltag, eoltsz);
}
 
/******************************************************************************/
/*                           P r o c e s s _ g e t                            */
/******************************************************************************/
  
int bbcp_Protocol::Process_get()
{
   bbcp_FileSpec *pp = 0, *fp = bbcp_Cfg.srcSpec;
   char *wp;
   int retc;
   int   fnum, snum;
   long long foffset = 0;
   const char getack[] = "204 getok\n";
   const int  getaln = strlen(getack);

// Get the file number and name from "get <strm> <fnum> <fname> [<offset>]"
//
   if (!(wp = Remote->GetToken()))
      {bbcp_Fmsg("Process_get", "missing stream number."); return 19;}
   if (bbcp_Cfg.a2n("stream number", wp, snum, 0, 255))  return 19;
   if (!(wp = Remote->GetToken()))
      {bbcp_Fmsg("Process_get", "missing file number."); return 19;}
   if (bbcp_Cfg.a2n("file number", wp, fnum, 0, 0x7ffffff))  return 19;
   if (!(wp = Remote->GetToken()))
      {bbcp_Fmsg("Process_get", "missing file name.");   return 19;}

// Locate the file
//
   while(fp && (fp->seqno != fnum || strcmp(wp, fp->filereqn)))
        {pp = fp; fp=fp->next;}
   if (!fp)
      {char buff[64];
       sprintf(buff, "%d", fnum);
       bbcp_Fmsg("Process_get", "file '", buff, wp, "' not found.");
       return 2;
      }

// Unchain the file specification (get allowed only once)
//
   if (pp) pp->next = fp->next;
      else bbcp_Cfg.srcSpec = fp->next;
   fp->next = 0;

// Get the optional offset
//
   if ((wp = Remote->GetToken()))
      {if (bbcp_Cfg.a2ll("file offset", wp, foffset, 0, -1)) return 22;
       if (foffset > fp->Info.size)
          {char buff[128];
           sprintf(buff, "(%lld>%lld)", foffset, fp->Info.size);
           bbcp_Fmsg("Process_get","Invalid offset",buff,"for",fp->pathname);
           return 29;
          }
       fp->targetsz = foffset;
      }

// Send the response
//
   if (Remote->Put(getack, getaln)) return 1;

// Send the file changing the sequence number to that requested by the caller
//
   fp->seqno = snum;
   retc = Local->SendFile(fp);
   delete fp;
   return retc;
}
  
/******************************************************************************/
/*                         P r o c e s s _ l o g i n                          */
/******************************************************************************/

// The following class is here to allow graceful error exits
//
int bbcp_Protocol::Process_login(bbcp_Link *Net)
{
   char buff[256], *tp, *bp, *vp, *wp, *id;
   int retc, blen, respWS;
   bbcp_Login_Stream loginStream(Net);
   bbcp_Node *np = loginStream.np;

// Get the first line of the login stream
//
   if (!(np->GetLine()))
      {if ((retc = np->LastError()))
          return bbcp_Emsg("Process_Login", retc, "processing login from",
                                 Net->LinkName());
       return bbcp_Fmsg("Process_Login", "Bad login from", Net->LinkName());
     }

// Determine the id we want (the control stream must login first)
//
   id = (Remote ? (char *)"data" : (char *)"ctlr");

// Process the login request: login <id> <password>
//
   if (!(wp = np->GetToken()) || strcmp(wp, "login")
   ||  !(wp = np->GetToken()) || strcmp(wp, id)
   ||  !(wp = np->GetToken()) || strcmp(wp, bbcp_Cfg.SecToken))
      return bbcp_Fmsg("Process_Login", "Invalid login from", Net->LinkName());

// We are all done if this is not a control stream
//
   if (*id != 'c') {np->Detach(); return 0;}

// Pickup all parameters.
//
   bp = vp = wp = 0;
   while((tp = np->GetToken()))
        {     if (!strcmp(tp, "wsz:"))
                 {if (!(wp = np->GetToken()))
                     return bbcp_Fmsg("Login", "Window size is missing.");
                 }
         else if (!strcmp(tp, "ver:"))
                 {if (!(vp = np->GetToken()))
                     return bbcp_Fmsg("Login", "Version is missing.");
                 }
         else if (!strcmp(tp, "bsz:"))
                 {if (!(bp = np->GetToken()))
                     return bbcp_Fmsg("Login", "Buffer size is missing.");
                 }
        }

// Verify that our version is the same on the other side
//
   if (vp) bbcp_Version.Verify(Net->LinkName(), vp);
      else bbcp_Version.Verify(Net->LinkName(),(char *)"02.01.12.00.0");

// We can now do a window/buffer adjustment
//
   if (!wp) respWS = bbcp_Cfg.Wsize;
      else if (!(respWS = AdjustWS(wp, bp, 0))) return -1;

// Respond to this login request (control only gets a response)
//
   blen = sprintf(buff, "204 loginok wsz: %d %d\n",respWS,bbcp_Cfg.RWBsz);
   if ((retc = np->Put(buff, blen)) < 0) return -1;

// All done
//
   Remote = np;
   loginStream.np = 0;
   return 1;
}

/******************************************************************************/
/* Target:                       R e q u e s t                                */
/******************************************************************************/
  
int bbcp_Protocol::Request(bbcp_Node *Node)
{
   long long totsz=0;
   bbcp_FileSpec *dp, *fp;
   const char *dRM = 0;
   char buff[1024];
   int  retc, numfiles, numlinks, texists;
   int  outDir  = (bbcp_Cfg.Options & bbcp_OUTDIR) != 0;
   bool dotrim  = false;
   bool doPcopy = (bbcp_Cfg.Options & bbcp_PCOPY) != 0;
   bool setDMode= (bbcp_Cfg.ModeD != bbcp_Cfg.ModeDC) != 0;
   bool isAppend= (bbcp_Cfg.Options & bbcp_APPEND) != 0;

// Establish all connections
//
   if (Node->Start(this, !(bbcp_Cfg.Options & bbcp_CON2SRC))
   ||  Node->getBuffers(1)) return 2;
   Local = Node;

// Determine if the target exists
//
   texists = !bbcp_Cfg.snkSpec->Stat(0);
   fs_obj  = bbcp_Cfg.snkSpec->FSys();

// Make sure we have a filesystem here
//
   if (!fs_obj)
      {bbcp_Fmsg("Request","Target directory", bbcp_Cfg.snkSpec->pathname,
                 "is not in a known file system");
       return Request_exit(2, 0);
      }

// If the target does not exist and we are doing a recursive copy, then we
// presume that the target should be a directory and we should create it.
//
   if (!texists && (outDir || (bbcp_Cfg.Options & bbcp_RECURSE))
   &&  (bbcp_Cfg.Options & bbcp_AUTOMKD))
      {retc = fs_obj->MKDir(bbcp_Cfg.snkSpec->pathname, bbcp_Cfg.ModeDC);
       if (retc) Request_exit(retc);
       texists = !bbcp_Cfg.snkSpec->Stat(0);
       dotrim = !outDir; dRM = bbcp_Cfg.snkSpec->pathname;
//cerr <<"dotrim=" <<dotrim <<" outd=" <<outDir <<" dRM=" <<dRM<<endl;
      }

// Establish the target directory
//
   if (texists && bbcp_Cfg.snkSpec->Info.Otype == 'd')
       tdir = bbcp_Cfg.snkSpec->pathname;
      else {int  plen;
            if ((plen = bbcp_Cfg.snkSpec->filename-bbcp_Cfg.snkSpec->pathname))
               strncpy(buff, bbcp_Cfg.snkSpec->pathname, plen-1);
               else {buff[0] = '.'; plen = 2;}
            tdir = buff; buff[plen-1] = '\0';
           }

// Generate a target directory ID. This will also uncover a missing directory.
//
   if (texists &&  bbcp_Cfg.snkSpec->Info.Otype == 'd')
      tdir_id = bbcp_Cfg.snkSpec->Info.fileid;
      else {bbcp_FileInfo Tinfo;
            if (!fs_obj || ((!(retc = fs_obj->Stat(tdir, &Tinfo))
            && Tinfo.Otype != 'd') && outDir)) retc = ENOTDIR;
            if (retc) {bbcp_Fmsg("Request","Target directory",
                                 bbcp_Cfg.snkSpec->pathname,"not found");
                       return Request_exit(2, dRM);
                      }
            tdir_id = Tinfo.fileid;
           }

// The first step is to perform an flist to get the list of files
//
   numfiles = Request_flist(totsz, numlinks, dotrim);
   if (numfiles  < 0) return Request_exit(22, dRM);
   if (numfiles == 0 && numlinks == 0 && !(bbcp_Cfg.Options &  bbcp_AUTOMKD))
      {cout <<"200 End: 0 0" <<endl;
       return Request_exit(0, dRM);
      }

// If we have a number files, the target had better be a directory
//
   if (numfiles > 1 || numlinks > 1 || outDir)
      {if (!texists)
          {bbcp_Fmsg("Request", "Target directory",
                     bbcp_Cfg.snkSpec->pathname, "not found.");
           return Request_exit(2);
          }
       if (bbcp_Cfg.snkSpec->Info.Otype != 'd')
          {bbcp_Fmsg("Request", "Target", bbcp_Cfg.snkSpec->pathname,
                     "is not a directory.");
           return Request_exit(20);
          }
       bbcp_Cfg.Options |= bbcp_OUTDIR;
      }

// Make sure we have enough space in the filesystem
//
   DEBUG("Preparing to copy " <<numlinks <<" links(s) "
         <<numfiles <<" file(s); bytes=" <<totsz);
   if (!(bbcp_Cfg.Options & bbcp_NOSPCHK) && !fs_obj->Enough(totsz, numfiles))
      {bbcp_Fmsg("Sink", "Insufficient space to copy all the files from",
                               Remote->NodeName());
       return Request_exit(28, dRM);
      }

// Get each source file that isn't inside a directory (non-recursive copy)
//
   fp = bbcp_Cfg.srcSpec;
   while(fp && !(retc=Request_get(fp)) && !(retc=Local->RecvFile(fp,Remote)))
        {if (isAppend) totsz -= fp->targetsz;
         fp = fp->next;
        }
   if (retc) return Request_exit(retc);

// Iterate through the copy set performing all required operations (recursive)
//
   std::map<char const *, bbcp_FileSpec *, Cmp_Key>::iterator it;
   for (it = CopySet.begin(); it != CopySet.end(); ++it)
       {dp = it->second;
        if ((retc = dp->Create_Path())) return Request_exit(retc);
        fp = dp->next;
        while(fp)
             {if (fp->Info.Otype == 'l')
                 {if ((retc = fp->Create_Link())) return Request_exit(retc);
                 } else {
                  if ((retc = Request_get(fp))
                  ||  (retc = Local->RecvFile(fp,Remote)))
                     return Request_exit(retc);
                  if (isAppend) totsz -= fp->targetsz;
                 }
              fp = fp->next;
             }
        if (doPcopy) dp->setStat(bbcp_Cfg.ModeD);
           else if (setDMode) dp->setMode(bbcp_Cfg.ModeD);
       }

// Report back how many files and bytes were received
//
   cout <<"200 End: " <<numfiles <<' ' <<totsz <<endl;

// All done
//
   return Request_exit(retc);
}

/******************************************************************************/
/*                       R e q u e s t   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                          R e q u e s t _ e x i t                           */
/******************************************************************************/
  
int bbcp_Protocol::Request_exit(int retc, const char *dRM)
{
    char buff[256];
    int blen;

// Remove any auto-created directory if need bo
//
   if (dRM && fs_obj) fs_obj->RM(dRM);

// Send the exit command (we don't care how it completes)
//
   blen = sprintf(buff, "exit %d\n", retc);
   Remote->Put(buff, (ssize_t)blen);
   Local->Stop();

// Return to caller
//
   return retc;
}
  
/******************************************************************************/
/*                         R e q u e s t _ f l i s t                          */
/******************************************************************************/
  
int bbcp_Protocol::Request_flist(long long &totsz, int &numlinks, bool dotrim)
{
   typedef std::pair<const char *, bbcp_FileSpec *> Pair;
   std::map<char const *, bbcp_FileSpec *, Cmp_Key>::iterator it;
   int retc, noteol, numfiles = 0;
   char *lp, *tfn, *slash;
   int   tdln = strlen(tdir);
   bbcp_FileSpec *fp;
   const char flcmd[] = "flist\n";
   const int  flcsz   = sizeof(flcmd)-1;
   bool orphan;

// Set correct target file name
//
   if (bbcp_Cfg.snkSpec->Info.Otype == 'd') tfn = 0;
      else tfn = bbcp_Cfg.snkSpec->filename;

// Send request
//
   if (Remote->Put(flcmd, flcsz)) return -1;

// Now start getting all of the objects to be recreated here
//
   numlinks = 0;
   while((lp = Remote->GetLine()) && (noteol = strcmp(lp, "eol")))
        {fp = new bbcp_FileSpec(fs_obj, Remote->NodeName());
         if (fp->Decode(lp)) {numfiles = -1; break;}

               if (fp->Compose(tdir_id, tdir, tdln, tfn)
               &&  (retc = fp->Xfr_Done()))
                  {delete fp; if (retc < 0) {numfiles = -1; break;}}
          else if (fp->Info.Otype == 'd')
                  {if (dotrim) {fp->setTrim(); dotrim = false;}
                      else CopySet.insert(Pair(fp->pathname,fp));
                  }
          else {dotrim = false;
                switch(fp->Info.Otype)
                      {case 'f': numfiles++;
                                 totsz += fp->Info.size;
                                 break;
                       case 'l': numlinks++;
                                 break;
                       case 'p': numfiles++;
                                 break;
                       default:  delete fp;
                                 continue;
                      }
                if (!(slash = rindex(fp->pathname, '/'))) orphan = true;
                   else {*slash = 0;
                         it = CopySet.find(fp->pathname);
                         *slash = '/';
                         if (it == CopySet.end()) orphan = true;
                            else {bbcp_FileSpec *gp = it->second;
                                  fp->next = gp->next;
                                  gp->next = fp;
                                  orphan = false;
                                 }
                        }
                if (orphan)
                   {fp->next = bbcp_Cfg.srcSpec;
                    bbcp_Cfg.srcSpec = fp;
                   }
               }
        }

// Flush the input queue if need be
//
   while(lp && strcmp(lp, "eol")) lp = Remote->GetLine();

// Check how we terminated
//
   if (numfiles >= 0 && noteol)
      return bbcp_Fmsg("Request_flist", "Premature end of file list from",
                       Remote->NodeName());

// All done
//
   return numfiles;
}
 
/******************************************************************************/
/*                           R e q u e s t _ g e t                            */
/******************************************************************************/
  
int bbcp_Protocol::Request_get(bbcp_FileSpec *fp)
{
   int blen;
   char *wp, buff[2048];

// Make sure there is enough space for this file
//
   if (!(bbcp_Cfg.Options & bbcp_NOSPCHK) && !(fs_obj->Enough(fp->Info.size,1)))
      {bbcp_Fmsg("Request_get", "Insufficient space to create file",
                       fp->targpath);
       return 28;
      }

// Construct the get command
//
   blen = snprintf(buff, sizeof(buff)-1, "get 0 %d %s %lld\n",
                   fp->seqno, fp->filereqn, fp->targetsz);

// Send off the command
//
   if (Remote->Put(buff, blen)) return 1;

// Get the response
//
   if (Remote->GetLine()        && Remote->GetToken()
   && (wp = Remote->GetToken()) && !strcmp(wp, "getok")) return 0;

// Improper response, we can't create the file
//
   bbcp_Fmsg("Request_get", "get from", Remote->NodeName(),
             "failed to create", fp->targpath);
   return 1;
}
  
/******************************************************************************/
/*                         R e q u e s t _ l o g i n                          */
/******************************************************************************/
  
int bbcp_Protocol::Request_login(bbcp_Link *Net)
{
   const char *CtlLogin = "login ctlr %s wsz: %s%d ver: %s dsz: %d\n";
   const char *DatLogin = "login data %s\n";
   char buff[512], *id, *wp;
   int retc, blen;
   bbcp_Login_Stream loginStream(Net);
   bbcp_Node *np = loginStream.np;

// Determine wether this is a control or data path
//
   id = (Remote ? (char *)DatLogin : (char *)CtlLogin);

// Prepare the login request
//
   blen = sprintf(buff,id,bbcp_Cfg.SecToken,(bbcp_Net.AutoTune() ? "+" : ""),
                          bbcp_Cfg.Wsize, bbcp_Version.VData,
                          bbcp_Cfg.RWBsz);

// Send the request
//
   if ((retc = np->Put(buff, (ssize_t)blen)) < 0)
      return bbcp_Emsg( "Request_Login",-(np->LastError()),
                        "requesting", id, (char *)"path.");

// If this is a data stream, then tell caller to hold on to the net link
//
   if (Remote) {np->Detach(); return 0;}

// For a control connection, read the acknowledgement below
// nnn loginok wsz: <num> [<dsz>]
//
   if (np->GetLine())
      {if (np->GetToken()        && (wp = np->GetToken())
       && !strcmp(wp, "loginok") && (wp = np->GetToken())
       && !strcmp(wp, "wsz:")    && (wp = np->GetToken())
       &&  AdjustWS(wp, np->GetToken(), 1))
          {Remote = np;
           loginStream.np = 0;
           return 1;
          }
      }

// Invalid response
//
   return bbcp_Fmsg("Request_Login", "Invalid login ack sequence.");
}
 
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                              A d j u s t W S                               */
/******************************************************************************/
  
int bbcp_Protocol::AdjustWS(char *wp, char *bp, int Final)
{
   int  xWS, xAT, sWS, sAT, tWS, tAT, xBS = 0;
   int isSRC = bbcp_Cfg.Options & bbcp_SRC;

// New version tell us if they can auto-tune via a leading plus
//
   xAT = (*wp == '+');

// Get window size in binary
//
   if (bbcp_Cfg.a2n("window size", wp, xWS, 1, -1)) return 0;

// New versions also tell us the I/O buffer size as well. Older versions
// do not give a buffer size and it was historically the window size.
//
   if (bp && *bp)
      {if (bbcp_Cfg.a2n("buffer size", bp, xBS, 1, -1)) return 0;}
      else xBS = xWS;

// If this is a login response and the value is the window that must be used.
// The receiver rules and establishes the window and I/O buffer size.
//
   if (Final)
      {if (isSRC)
          {if (xWS < bbcp_Cfg.Wsize)
              {bbcp_Cfg.Wsize = bbcp_Net.setWindow(xWS, 1);
               if (bbcp_Cfg.Options & bbcp_VERBOSE)
                  bbcp_Cfg.WAMsg("Logon","Source window size reduced",xWS);
              }
           if (xBS < bbcp_Cfg.RWBsz) bbcp_Cfg.setRWB(xBS);
          } else
           if (xBS > bbcp_Cfg.RWBsz) bbcp_Cfg.setRWB(xBS);
       return 1;
      }

// This is the initial login so we are being asked if the proposed window
// is acceptable and we must respond with the window that should be used.
// In all cases, we must either set the source buffer to be no greater that the
// target or the traget buffer to be no smaller than the source buffer.
//
   if (isSRC)
      {if (xBS < bbcp_Cfg.RWBsz) bbcp_Cfg.setRWB(xBS);
       sWS = bbcp_Cfg.Wsize; sAT = bbcp_Net.AutoTune();
       tWS = xWS;               tAT = xAT;
      } else {
       if (xBS > bbcp_Cfg.RWBsz) bbcp_Cfg.setRWB(xBS);
       sWS = xWS;               sAT = xAT;
       tWS = bbcp_Cfg.Wsize; tAT = bbcp_Net.AutoTune();
      }

// If source and target autotune then check if the target window >= source (warn
// o/w) and return back what the sender would have expected to keep autotuning.
//
   if (sAT && tAT)
      {if (tWS < sWS && bbcp_Cfg.Options & bbcp_VERBOSE)
          bbcp_Cfg.WAMsg("Login","Target autotuning may be misconfigured; "
                                    "max set", tWS);
       return (isSRC ? tWS : sWS);
      }

// If we are the source, hence we must use a window <= the target window.
//
   if (isSRC)
      {if (sWS > tWS)
          {bbcp_Cfg.Wsize = bbcp_Net.setWindow(tWS, 1);
           if (bbcp_Cfg.Options & bbcp_VERBOSE)
               bbcp_Cfg.WAMsg("Login", "Source window size reduced", tWS);
          }
       return tWS;
      }

// We are the target, request that the source always use the smaller window
//
   xWS = (sWS < tWS ? sWS : tWS);
   return xWS;
}
  
/******************************************************************************/
/*                             C h k 4 R e d i r                              */
/******************************************************************************/

void bbcp_Protocol::Chk4Redir(const char *oldHost, const char *newHost)
{
   bbcp_NetAddr oldAddr;
   bbcp_NetAddr newAddr;
   char buff[128];
   int n;

// Establish addresses and check if they are the same (port doesn't matter)
//
   oldAddr.Set(oldHost,0);
   newAddr.Set(newHost,0);
   if (oldAddr.Same(&newAddr)) return;

// Get information to blab out
//
   buff[0] = '(';
   n = newAddr.Format(buff, sizeof(buff)-8, bbcp_NetAddrInfo::fmtAddr,
                                            bbcp_NetAddrInfo::noPort);
   strcpy(&buff[n+1],").");
   bbcp_Fmsg("Protocol","Host",oldHost,"redirect connection to",newHost,buff);
}
  
/******************************************************************************/
/*                                p u t C S V                                 */
/******************************************************************************/

void bbcp_Protocol::putCSV(char *Host, char *csFn, char *csVal, int csVsz)
{                                //1234567890123
   struct iovec iov[] = {{(char *)"Checksum: ", 10},
                        {bbcp_Cfg.csName,strlen(bbcp_Cfg.csName)},
                        {(char *)" ", 1}, {csVal, static_cast<std::size_t>(csVsz)},
                        {(char *)" ", 1}, {Host, strlen(Host)},
                        {(char *)":", 1}, {csFn, strlen(csFn)},
                        {(char *)"\n",1}};
   int n = sizeof(iov)/sizeof(iov[0]);

// Write the checksum to a special file if it exists
//
   if (bbcp_Cfg.csPath)
      {if (writev(bbcp_Cfg.csFD, iov, n) < 0)
          {bbcp_Emsg("Protocol",errno,"writing checksum to",bbcp_Cfg.csPath);
           close(bbcp_Cfg.csFD); bbcp_Cfg.csFD = -1;
          }
      } else writev(STDERR_FILENO, iov, n);
}
  
/******************************************************************************/
/*                              S e n d A r g s                               */
/******************************************************************************/
  
int bbcp_Protocol::SendArgs(bbcp_Node *Node, bbcp_FileSpec *fsp,
                            char *cbhost, int cbport, char *addOpt)
{
   char buff[512], *apnt[6];
   int alen[6], i = 0;

// The remote program should be running at this point, setup the args
//
   if (bbcp_Cfg.CopyOpts)
      {apnt[i]   = bbcp_Cfg.CopyOpts;
       alen[i++] = strlen(bbcp_Cfg.CopyOpts);
      }
   if (addOpt) {apnt[i] = addOpt; alen[i++] = strlen(addOpt);}
   apnt[i]   = buff;
   alen[i++] = snprintf(buff, sizeof(buff)-1, " -H %s:%d\n", cbhost, cbport);
   apnt[i] = 0; alen[i] = 0;

// Send the argumnets via the stdout/stdin stream for the node
//
   if (Node->Put(apnt, alen) < 0)
      return bbcp_Emsg("Protocol", errno, "sending arguments to",
                        Node->NodeName());

// Send the file arguments now
//
   apnt[1] = (char *)"\n"; alen[1] = 1; apnt[2] = 0; alen[2] = 0;
   while(fsp)
        {apnt[0] = fsp->pathname; alen[0] = strlen(fsp->pathname);
         if (Node->Put(apnt, alen) < 0)
             return bbcp_Emsg("Protocol", errno, "sending file arguments to",
                               Node->NodeName());
         fsp = fsp->next;
        }

// Send eol
//
   apnt[0] = (char *)"\0"; alen[0] = 1; apnt[1] = 0; alen[1] = 0;
   if (Node->Put(apnt, alen) < 0)
      return bbcp_Emsg("Protocol", errno, "sending eol to", Node->NodeName());

// All done
//
   return 0;
}
