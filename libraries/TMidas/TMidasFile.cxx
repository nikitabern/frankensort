//
//  TMidasFile.cxx.
//

#include <stdio.h>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <cstdlib>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "TMidasFile.h"
#include "TMidasEvent.h"

ClassImp(TMidasFile)

////////////////////////////////////////////////////////////////
//                                                            //
// TMidasFile                                                 //
//                                                            //
// This Class is used to read and write MIDAS files in the    //
// root framework. It reads and writes TMidasEvents.          //
//                                                            //
////////////////////////////////////////////////////////////////

TMidasFile::TMidasFile()
{
   //Default Constructor
  uint32_t endian = 0x12345678;

  fFile = -1;
  fGzFile = NULL;
  fPoFile = NULL;
  fLastErrno = 0;
  fCurrentBufferSize = 0;

  fOutFile = -1;
  fOutGzFile = NULL;

  fMaxBufferSize = 1E6;

  fDoByteSwap = *(char*)(&endian) != 0x78;
}

TMidasFile::~TMidasFile()
{
   //Default dtor. It closes the read in midas file as well as the output midas file.
  Close();
  OutClose();
}

static int hasSuffix(const char*name,const char*suffix)
{
   //Checks to see if midas file has suffix.
  const char* s = strstr(name,suffix);
  if (s == NULL)
    return 0;

  return (s-name)+strlen(suffix) == strlen(name);
}

bool TMidasFile::Open(const char *filename)
{
  /// Open a midas .mid file with given file name.
  ///
  /// Remote files can be accessed using these special file names:
  /// - pipein://command - read data produced by given command, see examples below
  /// - ssh://username\@hostname/path/file.mid - read remote file through an ssh pipe
  /// - ssh://username\@hostname/path/file.mid.gz and file.mid.bz2 - same for compressed files
  /// - dccp://path/file.mid (also file.mid.gz and file.mid.bz2) - read data from dcache, requires dccp in the PATH
  ///
  /// Examples:
  /// - ./event_dump.exe /ladd/data9/t2km11/data/run02696.mid.gz - read normal compressed file
  /// - ./event_dump.exe ssh://ladd09//ladd/data9/t2km11/data/run02696.mid.gz - read compressed file through ssh to ladd09 (note double "/")
  /// - ./event_dump.exe pipein://"cat /ladd/data9/t2km11/data/run02696.mid.gz | gzip -dc" - read data piped from a command or script (note quotes)
  /// - ./event_dump.exe pipein://"gzip -dc /ladd/data9/t2km11/data/run02696.mid.gz" - another way to read compressed files
  /// - ./event_dump.exe dccp:///pnfs/triumf.ca/data/t2km11/aug2008/run02837.mid.gz - read file directly from a dcache pool (note triple "/")
  ///
  /// \param [in] filename The file to open.
  /// \returns "true" for succes, "false" for error, use GetLastError() to see why

  if (fFile > 0)
    Close();

  fFilename = filename;

  std::string pipe;

  // Do we need these?
  //signal(SIGPIPE,SIG_IGN); // crash if reading from closed pipe
  //signal(SIGXFSZ,SIG_IGN); // crash if reading from file >2GB without O_LARGEFILE

  if (strncmp(filename, "ssh://", 6) == 0)
    {
      const char* name = filename + 6;
      const char* s = strstr(name, "/");

      if (s == NULL)
        {
          fLastErrno = -1;
          fLastError.assign("TMidasFile::Open: Invalid ssh:// URI. Should be: ssh://user@host/file/path/...");
          return false;
        }

      const char* remoteFile = s + 1;

      std::string remoteHost;
      for (s=name; *s != '/'; s++)
        remoteHost += *s;

      pipe = "ssh -e none -T -x -n ";
      pipe += remoteHost;
      pipe += " dd if=";
      pipe += remoteFile;
      pipe += " bs=1024k";

      if (hasSuffix(remoteFile,".gz"))
        pipe += " | gzip -dc";
      else if (hasSuffix(remoteFile,".bz2"))
        pipe += " | bzip2 -dc";
    }
  else if (strncmp(filename, "dccp://", 7) == 0)
    {
      const char* name = filename + 7;

      pipe = "dccp ";
      pipe += name;
      pipe += " /dev/fd/1";

      if (hasSuffix(filename,".gz"))
        pipe += " | gzip -dc";
      else if (hasSuffix(filename,".bz2"))
        pipe += " | bzip2 -dc";
    }
  else if (strncmp(filename, "pipein://", 9) == 0)
    {
      pipe = filename + 9;
    }
#if 0 // read compressed files using the zlib library
  else if (hasSuffix(filename, ".gz"))
    {
      pipe = "gzip -dc ";
      pipe += filename;
    }
#endif
  else if (hasSuffix(filename, ".bz2"))
    {
      pipe = "bzip2 -dc ";
      pipe += filename;
    }
  else{
     pipe = "cat ";
     pipe+=filename;
  }

  if (pipe.length() > 0)
    {
      fprintf(stderr,"TMidasFile::Open: Reading from pipe: %s\n", pipe.c_str());
      fPoFile = popen(pipe.c_str(), "r");

      if (fPoFile == NULL)
        {
          fLastErrno = errno;
          fLastError.assign(std::strerror(errno));
          return false;
        }

      fFile = fileno((FILE*)fPoFile);
    }
  else
    {
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

      fFile = open(filename, O_RDONLY | O_LARGEFILE);

      if (fFile <= 0)
        {
          fLastErrno = errno;
          fLastError.assign(std::strerror(errno));
          return false;
        }

      if (hasSuffix(filename, ".gz"))
        {
          // this is a compressed file
#ifdef HAVE_ZLIB
          fGzFile = new gzFile;
          (*(gzFile*)fGzFile) = gzdopen(fFile,"rb");
          if ((*(gzFile*)fGzFile) == NULL)
            {
              fLastErrno = -1;
              fLastError.assign("zlib gzdopen() error");
              return false;
            }
#else
          fLastErrno = -1;
          fLastError.assign("Do not know how to read compressed MIDAS files");
          return false;
#endif
        }
    }

  return true;
}

bool TMidasFile::OutOpen(const char *filename)
{
  /// Open a midas .mid file for OUTPUT with given file name.
  ///
  /// Remote files not yet implemented
  ///
  /// \param [in] filename The file to open.
  /// \returns "true" for succes, "false" for error, use GetLastError() to see why

  if (fOutFile > 0)
    OutClose();
  
  fOutFilename = filename;
  
  printf ("Attempting normal open of file %s\n", filename);
  //fOutFile = open(filename, O_CREAT |  O_WRONLY | O_LARGEFILE , S_IRUSR| S_IWUSR | S_IRGRP | S_IROTH );
  //fOutFile = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_LARGEFILE, 0644);
  fOutFile = open (filename, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);
  
  if (fOutFile <= 0)
    {
      fLastErrno = errno;
      fLastError.assign(std::strerror(errno));
      return false;
    }
  
  printf("Opened output file %s ; return fOutFile is %i\n",filename,fOutFile);

  if (hasSuffix(filename, ".gz"))
    // (hasSuffix(filename, ".dummy"))
    {
      // this is a compressed file
#ifdef HAVE_ZLIB
      fOutGzFile = new gzFile;
      *(gzFile*)fOutGzFile = gzdopen(fOutFile,"wb");
      if ((*(gzFile*)fOutGzFile) == NULL)
	{
	  fLastErrno = -1;
	  fLastError.assign("zlib gzdopen() error");
	  return false;
	}
      printf("Opened gz file successfully\n");
      if (1) 
	{
	  if (gzsetparams(*(gzFile*)fOutGzFile, 1, Z_DEFAULT_STRATEGY) != Z_OK) {
	    printf("Cannot set gzparams\n");
	    fLastErrno = -1;
	    fLastError.assign("zlib gzsetparams() error");
	    return false;
	  }
	  printf("setparams for gz file successfully\n");  
	}
#else
      fLastErrno = -1;
      fLastError.assign("Do not know how to write compressed MIDAS files");
      return false;
#endif
    }
  return true;
}


static int readpipe(int fd, char* buf, int length)
{
  int count = 0;
  while (length > 0)
    {
      int rd = read(fd, buf, length);
      if (rd > 0)
        {
          buf += rd;
          length -= rd;
          count += rd;
        }
      else if (rd == 0)
        {
          return count;
        }
      else
        {
          return -1;
        }
    }
  return count;
}

int TMidasFile::Read(TMidasEvent *midasEvent)
{
  /// \param [in] midasEvent Pointer to an empty TMidasEvent 
  /// \returns "true" for success, "false" for failure, see GetLastError() to see why

  ///  EDITED FROM THE ORIGINAL TO RETURN TOTAL SUCESSFULLY BYTES READ INSTEAD OF TRUE/FALSE,  PCB

  midasEvent->Clear();

  int rd = 0;
  int rd_head = 0;
  if (fGzFile)
#ifdef HAVE_ZLIB
    rd_head = gzread(*(gzFile*)fGzFile, (char*)midasEvent->GetEventHeader(), sizeof(TMidas_EVENT_HEADER));
#else
    assert(!"Cannot get here");
#endif
  else
    rd_head = readpipe(fFile, (char*)midasEvent->GetEventHeader(), sizeof(TMidas_EVENT_HEADER));

  if (rd_head == 0)
    {
      fLastErrno = 0;
      fLastError.assign("EOF");
      return 0;
    }
  else if (rd_head != sizeof(TMidas_EVENT_HEADER))
    {
      fLastErrno = errno;
      fLastError.assign(std::strerror(errno));
      return 0;
    }

  if (fDoByteSwap){
    printf("Swapping bytes\n");
    midasEvent->SwapBytesEventHeader();
   }
  if (!midasEvent->IsGoodSize())
    { 
      fLastErrno = -1;
      fLastError.assign("Invalid event size");
      return 0;
    }

  if (fGzFile)
#ifdef HAVE_ZLIB
    rd = gzread(*(gzFile*)fGzFile, midasEvent->GetData(), midasEvent->GetDataSize());
#else
    assert(!"Cannot get here");
#endif
  else
    rd = readpipe(fFile, midasEvent->GetData(), midasEvent->GetDataSize());

  if (rd != (int)midasEvent->GetDataSize())
    {
      fLastErrno = errno;
      fLastError.assign(std::strerror(errno));
      return 0;
    }

  midasEvent->SwapBytes(false);

  return rd + rd_head;
}

void TMidasFile::FillBuffer(TMidasEvent *midasEvent, Option_t *opt){
//Fills A buffer to be written to a midas file.   

   
   //Not the prettiest way to do this but it works.
   //It seems to be filling in the wrong order of bits, but this does it correctly
   //There is a byte swap happening at some point in this process. Might have to put something
   //in here that protects against "Endian-ness"
   fWriteBuffer.push_back((char)(midasEvent->GetEventId()&0xFF));
   fWriteBuffer.push_back((char)(midasEvent->GetEventId() >> 8));

   fWriteBuffer.push_back((char)(midasEvent->GetTriggerMask()&0xFF));
   fWriteBuffer.push_back((char)(midasEvent->GetTriggerMask() >> 8));

   fWriteBuffer.push_back((char)(midasEvent->GetSerialNumber() & 0xFF));
   fWriteBuffer.push_back((char)((midasEvent->GetSerialNumber() >> 8) & 0xFF));
   fWriteBuffer.push_back((char)((midasEvent->GetSerialNumber() >> 16) & 0xFF));
   fWriteBuffer.push_back((char)(midasEvent->GetSerialNumber() >> 24));
  
   fWriteBuffer.push_back((char)(midasEvent->GetTimeStamp() & 0xFF));
   fWriteBuffer.push_back((char)((midasEvent->GetTimeStamp() >> 8) & 0xFF));
   fWriteBuffer.push_back((char)((midasEvent->GetTimeStamp() >> 16) & 0xFF));
   fWriteBuffer.push_back((char)(midasEvent->GetTimeStamp() >> 24));
   
   fWriteBuffer.push_back((char)(midasEvent->GetDataSize() & 0xFF));
   fWriteBuffer.push_back((char)((midasEvent->GetDataSize() >> 8) & 0xFF));
   fWriteBuffer.push_back((char)((midasEvent->GetDataSize() >> 16) & 0xFF));
   fWriteBuffer.push_back((char)(midasEvent->GetDataSize() >> 24));
 
   for(int i=0; i<midasEvent->GetDataSize();i++){
      fWriteBuffer.push_back(midasEvent->GetData()[i]);
   }
  
   fCurrentBufferSize += midasEvent->GetDataSize() + sizeof(TMidas_EVENT_HEADER);

   if(fWriteBuffer.size() >  fMaxBufferSize)
      WriteBuffer();
}

bool TMidasFile::WriteBuffer(){
   //Writes a buffer of TMidasEvents to the output file.
   int wr = -2;
 
   if (fOutGzFile){
      #ifdef HAVE_ZLIB
         wr = gzwrite(*(gzFile*)fOutGzFile, fWriteBuffer.data(), fCurrentBufferSize);
      #else
         assert(!"Cannot get here");
      #endif
   }
   else{
      wr = write(fOutFile, fWriteBuffer.data(), fCurrentBufferSize);
   }
   fCurrentBufferSize = 0;
   fWriteBuffer.clear();

   return wr;
}

bool TMidasFile::Write(TMidasEvent *midasEvent,Option_t *opt)
{
   //Writes an individual TMidasEvent to the output TMidasFile. This will
   //write to a zipped file if the output file is defined as a zipped file.
  int wr = -2;

  if (fOutGzFile)
#ifdef HAVE_ZLIB
    wr = gzwrite(*(gzFile*)fOutGzFile, (char*)midasEvent->GetEventHeader(), sizeof(TMidas_EVENT_HEADER));
#else
    assert(!"Cannot get here");
#endif
  else
    wr = write(fOutFile, (char*)midasEvent->GetEventHeader(), sizeof(TMidas_EVENT_HEADER));

  if(wr !=  sizeof(TMidas_EVENT_HEADER)){
    printf("TMidasFile: error on write event header, return %i, size requested %lu\n",wr,sizeof(TMidas_EVENT_HEADER));
    return false;
  }
 
  if(strncmp(opt,"q",1)!=0)
    printf("Written event header to outfile , return is %i\n",wr);

  if (fOutGzFile)
#ifdef HAVE_ZLIB
    wr = gzwrite(*(gzFile*)fOutGzFile, (char*)midasEvent->GetData(), midasEvent->GetDataSize());
#else
    assert(!"Cannot get here");
#endif
  else
    wr = write(fOutFile, (char*)midasEvent->GetData(), midasEvent->GetDataSize());

  if(strncmp(opt,"q",1)!=0)
    printf("Written event to outfile , return is %d\n",wr);

  return wr;
}

void TMidasFile::SetMaxBufferSize(int maxsize) {
   //Sets the maximum buffer size for the TMidasEvents to be written to
   //an output TMidasFile.
   fMaxBufferSize = maxsize;
}

void TMidasFile::Close()
{
   //Closes the input midas file. Use OutClose() to close the output 
   //Midas File.
  if (fPoFile)
    pclose((FILE*)fPoFile);
  fPoFile = NULL;
#ifdef HAVE_ZLIB
  if (fGzFile)
    gzclose(*(gzFile*)fGzFile);
  fGzFile = NULL;
#endif
  if (fFile > 0)
    close(fFile);
  fFile = -1;
  fFilename = "";
}

void TMidasFile::OutClose()
{
   //Closes the output midas file. Use Close() to close the read-in midas file

   if(fWriteBuffer.size()){
      WriteBuffer();
   }
#ifdef HAVE_ZLIB
  if (fOutGzFile) {
    gzflush(*(gzFile*)fOutGzFile, Z_FULL_FLUSH);
    gzclose(*(gzFile*)fOutGzFile);
  }
  fOutGzFile = NULL;
#endif
  if (fOutFile > 0)
    close(fOutFile);
  fOutFile = -1;
  fOutFilename = "";
}


int TMidasFile::GetRunNumber() {
   //Parse the run number from the current TMidasFile. This assumes a format of
   //run#####_###.mid or run#####.mid.
   if(fFilename.length()==0) {
      return 0;
   }
   std::size_t foundslash = fFilename.rfind('/');
   std::size_t found = fFilename.rfind(".mid");
   if(found == std::string::npos) {
      return 0;
   }
   std::size_t found2 = fFilename.rfind('-');
   if((found2 < foundslash && foundslash != std::string::npos) || found2 == std::string::npos)
      found2 = fFilename.rfind('_');
//   printf("found 2 = %i\n",found2);
   if(found2 < foundslash && foundslash != std::string::npos)
      found2 = std::string::npos;
   std::string temp;
   if(found2 == std::string::npos || fFilename.compare(found2+4,4,".mid") !=0 ) {
      temp = fFilename.substr(found-5,5);
   }
   else {
      temp = fFilename.substr(found-9,5);
   }
   //printf(" %s \t %i \n",temp.c_str(),atoi(temp.c_str()));
   return atoi(temp.c_str());
}


int TMidasFile::GetSubRunNumber()	{
   //Parse the sub run number from the current TMidasFile. This assumes a format of
   //run#####_###.mid or run#####.mid.
   if(fFilename.length()==0)
      return -1;
   std::size_t foundslash = fFilename.rfind('/');
   std::size_t found = fFilename.rfind("-");
   if((found < foundslash && foundslash != std::string::npos) || found == std::string::npos)
      found = fFilename.rfind('_');
   if(found < foundslash && foundslash != std::string::npos)
      found = std::string::npos;
   if(found != std::string::npos) {
      std::string temp = fFilename.substr(found+1,3);
      //printf("%i \n",atoi(temp.c_str()));
      return atoi(temp.c_str());
   }
   return -1;
}










// end
