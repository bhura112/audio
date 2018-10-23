/*******************************************************************************
  Universal Audio decoder demo.

  Company:
    Microchip Technology Inc.

  File Name:
    disk.c

  Summary:
   Contains the functional implementation of this demo application.

  Description:
   This file contains the functional implementation of this demo application.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
Copyright (c) 2012 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED AS IS WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include "app.h"
#include "disk.h"

static APP_DATA * appDataPtr;
static DISK_TASK_DATA diskData;
static uint32_t bytes_read = 0;
static DISK_FILE_NODE rootNode;


// files index table
static DISK_FILE_PATH FilesTable[DISK_MAX_FILES];

#define TRAVERSE_DEPTH DISK_MAX_DIRS

void DISK_Initialize( void )
{
    diskData.state = DISK_STATE_HALT;

    rootNode.fstat.fattrib = SYS_FS_ATTR_DIR;
    strcpy(rootNode.path, SYS_FS_MEDIA_IDX0_MOUNT_NAME_VOLUME_IDX0);
    strcat(rootNode.path, "/");
    appDataPtr = APP_GetAppDataInstance();
}

/*bool DISK_Mount()
{
   return true;
}

bool DISK_Unmount(void)
{
      if(SYS_FS_Unmount("/mnt/myDrive1") != 0)
      {
          // The disk could not be mounted. Try
                 // mounting again untill success. 
          return false;
      }
      else
      {
          // Mount was successful. Try opening the file 
          return true;
      }
}
*/

SYS_FS_RESULT DISK_FS_ReadDirFlat(const char *fname, uint8_t *dir_count, SYS_FS_FSTAT * dir_table, bool isRoot)
{
    SYS_FS_RESULT ret;
    diskData.dirHandle = SYS_FS_DirOpen(fname);
   
    if(diskData.dirHandle == SYS_FS_HANDLE_INVALID)
    {
        return SYS_FS_RES_FAILURE;
    }
    *dir_count = 0;
    
    do
     {
         if(appDataPtr->totalAudioFiles < DISK_MAX_FILES)
         {
             diskData.dirstat.fname[0] = '\0';
             ret = SYS_FS_DirRead(diskData.dirHandle,&diskData.dirstat);

             // End of this directory
             if(diskData.dirstat.fname[0] == '\0')
             {
                 break;
             }
             if(ret!= SYS_FS_RES_FAILURE)
             {

                 if(diskData.dirstat.fattrib != SYS_FS_ATTR_DIR)
                 {
                     
                    if(APP_IsSupportedAudioFile(diskData.dirstat.fname))
                    {
                        strcpy(FilesTable[appDataPtr->totalAudioFiles].path, fname);
                        if(!isRoot)
                        {
                            strcat(FilesTable[appDataPtr->totalAudioFiles].path, "/");
                        }
                        strcat(FilesTable[appDataPtr->totalAudioFiles].path, diskData.dirstat.fname);
                        
                        (appDataPtr->totalAudioFiles)++;
                    }
                     
                 }else if(diskData.dirstat.fattrib == SYS_FS_ATTR_DIR && diskData.dirstat.fname[0] != '.') // Skip ".\" and "..\" directories
                 {
                     if(*dir_count < TRAVERSE_DEPTH)
                     {
                         dir_table[*dir_count]=diskData.dirstat;
                         (*dir_count)++;
                     }
                 }
             }else{
                ret = SYS_FS_RES_FAILURE;
                break;
             }
         }
         else
         {
             ret = SYS_FS_RES_FAILURE;
             break;
         }
     }while(ret==SYS_FS_RES_SUCCESS);
     
     SYS_FS_DirClose(diskData.dirHandle);
     return ret;
}

void DISK_TraverseAllFiles(DISK_FILE_NODE node, bool isRoot,uint8_t depth){
     if(depth > TRAVERSE_DEPTH)
     {
         return;
     }
     int i;
     SYS_FS_RESULT ret;

     uint8_t totalDir = 0;
     SYS_FS_FSTAT dirTable[TRAVERSE_DEPTH];
     
     ret = DISK_FS_ReadDirFlat(node.path, &totalDir, dirTable, isRoot);

     if(ret == SYS_FS_RES_FAILURE)
     {
         return;
     }

     DISK_FILE_NODE child_node;
     
     for(i = 0; i < totalDir; i++)
     {
         
         child_node.fstat = dirTable[i];
         
         strcpy(child_node.path, node.path);
         if(!isRoot)
         {
             strcat(child_node.path, "/");
         }
         strcat(child_node.path, child_node.fstat.fname);
         DISK_TraverseAllFiles(child_node, false, depth+1);
         
     }
     
     return;
 }
 

/*bool DISK_ScanTask(void )
{
    return true;
}
*/
void DISK_Tasks()
{
    switch ( diskData.state )
    {
        case DISK_STATE_HALT:
            if(appDataPtr->state == APP_STATE_READY_TO_SCAN)
            {
                diskData.state = DISK_STATE_INIT;
            }
            break;

        case DISK_STATE_INIT:
            diskData.state = DISK_STATE_SCANNING;
            diskData.scanstate = DISK_SCAN_OPEN_DIR;

            appDataPtr->totalAudioFiles = 0;
            appDataPtr->currentSongIdx  = 0;
            appDataPtr->nextSongIdx     = 0;
            appDataPtr->previousSongIdx = 0;
            DISK_ResetPlayedTracks();
            DISK_EventHandler ( DISK_EVENT_SCANNING_STARTED, 0, appDataPtr->fileHandle);
        
            break;

       case DISK_STATE_SCANNING:
            DISK_TraverseAllFiles(rootNode, true, 0);
            
            if(appDataPtr->totalAudioFiles == 0){
                // No Audio File
                diskData.state = DISK_STATE_NO_AUDIO_FILES;
            }else{
                appDataPtr->nextSongIdx = 0;
                diskData.state = DISK_STATE_SCAN_FINISHED;
                DISK_EventHandler ( DISK_EVENT_SCANNING_FINISHED, 0, 0);  //from release
            }
            break;
        
        case DISK_STATE_SCAN_FINISHED:
            if(appDataPtr->playbackDelay)
                break;
            
            appDataPtr->previousSongIdx = appDataPtr->currentSongIdx;
            appDataPtr->currentSongIdx = appDataPtr->nextSongIdx;
            DISK_SetNextTrack();
            strcpy(appDataPtr->fileName, FilesTable[appDataPtr->currentSongIdx].path);
            diskData.state = DISK_STATE_RUNNING;
            //appDataPtr->state = APP_STATE_CODEC_SET_BUFFER_HANDLER;
            appDataPtr->state = APP_STATE_OPEN_FILE;
            break;

        case DISK_STATE_RUNNING:
            if(!appDataPtr->deviceIsConnected) 
            {
                diskData.state = DISK_STATE_HALT;
                SYS_FS_FileClose(appDataPtr->fileHandle);
                break;
            }

            if(appDataPtr->state == APP_STATE_SCANNING)
            {
                appDataPtr->pingPong = 1;    
                diskData.state = DISK_STATE_SCAN_FINISHED;
                FilesTable[appDataPtr->currentSongIdx].trackPlayed = true;
                SYS_FS_FileClose(appDataPtr->fileHandle);
            }
            if(appDataPtr->state == APP_STATE_READY_TO_SCAN)
            {
                diskData.state = DISK_STATE_INIT;
                SYS_FS_FileClose(appDataPtr->fileHandle);
            }
            break;

        case DISK_STATE_NO_AUDIO_FILES:
            appDataPtr->state = APP_STATE_NO_MEDIA;
            diskData.state = DISK_STATE_RUNNING;
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////

bool DISK_NextTrack(void)
{
    if(appDataPtr->currentSongIdx == appDataPtr->totalAudioFiles-1)
    {
        appDataPtr->nextSongIdx = 0;
    }else{
        appDataPtr->nextSongIdx = appDataPtr->currentSongIdx + 1;
    }
    
    appDataPtr->currentSongIdx = appDataPtr->nextSongIdx;

    if (DISK_OpenTrack(FilesTable[appDataPtr->currentSongIdx].path) == true)
    {
        return true;
    }
    else
        return false;
}

bool DISK_PreviousTrack()
{
    appDataPtr->previousSongIdx = (appDataPtr->currentSongIdx ? (appDataPtr->currentSongIdx - 1):(appDataPtr->totalAudioFiles - 1));
    appDataPtr->currentSongIdx = appDataPtr->previousSongIdx;
    if (DISK_OpenTrack(FilesTable[appDataPtr->currentSongIdx].path) == true)
    {
        return true;
    }
    else
        return false;
}

////////////////////////////////////////////////////////////////////////////////

bool DISK_OpenTrack ( const char *fname )
{
    appDataPtr->fileHandle = DISK_OpenFile(fname);

    if ( appDataPtr->fileHandle != SYS_FS_HANDLE_INVALID )
    {
        
        if(DISK_FileStatus(fname) == true)
        {
            appDataPtr->current_filesize = DISK_GetFileSize(appDataPtr->fileHandle);
            DISK_EventHandler ( DISK_EVENT_TRACK_CHANGED, appDataPtr->diskCurrentFile, appDataPtr->fileHandle );
            
            bytes_read = DISK_GetFilePosition(appDataPtr->fileHandle);
            return true;
        }else
        {
              return false;
        }
        
    }
    else
    {
        DISK_EventHandler ( DISK_EVENT_FILE_OPEN_ERROR, appDataPtr->diskCurrentFile,appDataPtr->fileHandle );
        return ( false );
    }
    
    return true;
}

bool DISK_ReopenTrack(void)
{
    if (DISK_OpenTrack(FilesTable[appDataPtr->currentSongIdx].path) == true)
    {
        return true;
    }
    else
        return false;
}


SYS_FS_HANDLE DISK_OpenFile ( const char *fname )
{
    SYS_FS_HANDLE fileHandle;
    bytes_read = 0;
    //playerDiskDataSize=0;

    fileHandle = SYS_FS_FileOpen(fname, (SYS_FS_FILE_OPEN_READ_PLUS));
    return fileHandle;
}

 bool DISK_CloseFile (SYS_FS_HANDLE fileHandle )
{
    SYS_FS_RESULT ret;
    if ( fileHandle != SYS_FS_HANDLE_INVALID )
    {
        ret = SYS_FS_FileClose ( fileHandle );
        if(ret == SYS_FS_RES_SUCCESS)
        {
            return true;
        }
    }
    return false;
}

uint16_t DISK_GetTotalFiles ( void )
{
    return ( appDataPtr->totalAudioFiles );
}

uint16_t DISK_GetCurrentFileNumber ( void )
{
    return ( (uint16_t) appDataPtr->diskCurrentFile );
}

bool DISK_FileNameGet(SYS_FS_HANDLE handle, char* cName)
{
    bool stat = SYS_FS_FileNameGet(handle, (uint8_t*) cName, 255);
    return stat;
}

bool DISK_FileStatus(const char* fname)
{
     if( SYS_FS_FileStat(fname, &(appDataPtr->fileStatus)) == SYS_FS_RES_FAILURE )
     {
        return false;
     }
     
     uint8_t len = 0;
    
     if (appDataPtr->fileStatus.lfname[0]=='\0') // name is shorter than 8 char
     {
        len = strlen(appDataPtr->fileStatus.fname);
        if (len > 0)
        {
            strncpy (appDataPtr->fileStatus.lfname, appDataPtr->fileStatus.fname, len);
            appDataPtr->fileStatus.lfname[len] = '\0'; // null character
        }
     }
     
     return true;
}

uint32_t DISK_GetFileSize(SYS_FS_HANDLE fileHandle)
{
    appDataPtr->fileSize = SYS_FS_FileSize( fileHandle );

    if(appDataPtr->fileSize == -1)
    {
        /* Reading file size was a failure */
        return (uint32_t) SYS_FS_HANDLE_INVALID;
    }
    else
    {
        return (uint32_t)appDataPtr->fileSize;
    }
}

bool DISK_FSeek(SYS_FS_HANDLE fileHandle,int32_t offset)
{
    if(SYS_FS_FileSeek( fileHandle, offset, SYS_FS_SEEK_SET ) == -1)
    {
         return false;
    }
    else
    {
        /* Check for End of file */
         return true;
    }
}

bool DISK_EndOfFile(void)
{
    
    if(SYS_FS_FileEOF( appDataPtr->fileHandle ) == false )
    {
        return false;
    }
    else
    {
        return true;
    }

}
bool DISK_SeekStartLocation(SYS_FS_HANDLE fileHandle,int32_t offset)
{
    /* Move file pointer to begining of file */
    if(SYS_FS_FileSeek( fileHandle, offset, SYS_FS_SEEK_END ) == -1)
    {
            return false;
    }
    else
    {
            return true;
    }
            
}

bool DISK_FillBuffer(uint8_t *ptr)
{
    switch(appDataPtr->currentStreamType)
    {
        case APP_STREAM_WAV:
        {
            if((bytes_read <appDataPtr->current_filesize))
            { 
                if((appDataPtr->readbyte_flag))
                {
                    appDataPtr->nBytesRead = SYS_FS_FileRead( appDataPtr->fileHandle,ptr,appDataPtr->readBytes );
                    if ((appDataPtr->nBytesRead == -1)||(DISK_EndOfFile() == true ))
                    {
                        bytes_read = 0;
                        SYS_FS_FileClose(appDataPtr->fileHandle);
                        return false;
                    }
                    else
                    {
                        bytes_read += appDataPtr->nBytesRead;
                        //playerDiskDataSize += appDataPtr->nBytesRead;
                        appDataPtr->nBytesRead =0;
                    }
                }
            }
            else
            {
                bytes_read = 0;
                return false;
            }
        }
        
        break;
        
        default:
            break;
    }
    return true;
}


int32_t DISK_GetFilePosition (SYS_FS_HANDLE fileHandle )
{
    int32_t tell;

    tell = SYS_FS_FileTell(fileHandle);
    return ( tell );
}

int32_t DISK_GetCurrentFilePosition(){
    return SYS_FS_FileTell(appDataPtr->fileHandle);
}

bool DISK_SetFilePosition (SYS_FS_HANDLE fileHandle, int32_t pos )
{
    int status;
    status = SYS_FS_FileSeek(fileHandle, pos, SYS_FS_SEEK_SET);
    if( status == pos )
    {
        return true;
    }
    return false;
}

bool DISK_SetCurrentFilePosition(int32_t pos)
{
    int status;
    status = SYS_FS_FileSeek(appDataPtr->fileHandle, pos, SYS_FS_SEEK_SET);
    if( status == pos )
    {
        return true;
    }
    return false;
}

bool DISK_SetCurrentFilePositionWithControl(int32_t pos, SYS_FS_FILE_SEEK_CONTROL whence)
{
    int status;
    status = SYS_FS_FileSeek(appDataPtr->fileHandle, pos, whence);
    if( status == pos )
    {
        return true;
    }
    return false;
}


uint32_t DISK_ReadCurrentFile(uint8_t *ptr, size_t readSize){
    uint32_t ret = 0;
    appDataPtr->nBytesRead = SYS_FS_FileRead( appDataPtr->fileHandle,ptr,readSize);
    if ((appDataPtr->nBytesRead != readSize))
    {
        ret = 0;
        bytes_read = 0;
    }
    else
    {
        appDataPtr->nBytesRead =0;
        return readSize;
    }
    
    return ret;
}

uint32_t DISK_GetCurrentFileSize()
{
    appDataPtr->fileSize = SYS_FS_FileSize( appDataPtr->fileHandle );
    if(appDataPtr->fileSize == -1)
    {
        /* Reading file size was a failure */
        return (uint32_t) SYS_FS_HANDLE_INVALID;
    }
    else
    {
        return (uint32_t)appDataPtr->fileSize;
    }
}

bool DISK_AreAllTracksPlayed()
{
    for(int i=0; i<appDataPtr->totalAudioFiles; i++)
    {
        if(FilesTable[i].trackPlayed)
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}

void DISK_ResetPlayedTracks()
{
    for(int i=0; i<appDataPtr->totalAudioFiles; i++)
    {
        FilesTable[i].trackPlayed = false;
    }
}

void DISK_SetNextTrack( void )
{
    switch(appDataPtr->playerBtnMode)
    {
/*        case RANDOM_TRACK_CHANGE:
            // Reset track played indicators if all tracks have been played
            if(DISK_AreAllTracksPlayed())
            {
                DISK_ResetPlayedTracks();
            }
            // Randomly choose an available track
            do
            {
                tempTrack = rand() % appDataPtr->totalAudioFiles;
            } while(FilesTable[tempTrack].trackPlayed);
            appDataPtr->nextSongIdx = tempTrack;
            break;*/
            
        case LINEAR_TRACK_CHANGE:
        default:
            appDataPtr->nextSongIdx++;
            break;
    }
    appDataPtr->nextSongIdx %= appDataPtr->totalAudioFiles;
    appDataPtr->playbackDelay = 1000;
}

