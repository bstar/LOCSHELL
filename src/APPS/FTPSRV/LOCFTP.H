
static int shellVisible=1, shellDrain=0, shellEnabled=1, shellOnline=0;
static int shellProtected(const char*p){
 if(!stricmp(p,"C:\\DOSSHELL"))return 1;
 if(strnicmp(p,"C:\\DOSSHELL\\",12))return 0;
 p+=12;
 return !strnicmp(p,"MEMORY.",7)||!strnicmp(p,"LIBRARY.",8)||!strnicmp(p,"DOSSHELL.",9)||!strnicmp(p,"LEARN.",6)||!strnicmp(p,"NETSTATE.",9)||!strnicmp(p,"RECENT.",7);
}
static uint8_t shuttingDown=0;
static clockTicks_t lastTimeoutSweep,lastKeyboardCheck,lastDnsCheck;
extern "C" int ds_shell_main(int,char **);
extern "C" void ds_shell_boot(const char*);
extern "C" void ds_shell_memory_changed(void);
static void shellContextWritten(const char*);
static void shellImageWritten(FtpClient*);
static void shellTransferFinished(FtpClient*);
static void shellTransferFailed(FtpClient*);
static void shellFileChanged(const char*,const char*,unsigned long,int);

static int shellService(void) {


    PACKET_PROCESS_MULT( PacketsPerPoll );
    Arp::driveArp( );
    Tcp::drivePackets( );

#ifdef LOC_SHELL_TEST
    int previewTrace=daPreviewPending&&!FtpClient::activeClients;
    if(previewTrace){FILE*f=fopen("C:\\DOSSHELL\\PHASE.TXT","w");if(f){fputs("before preview\n",f);fclose(f);}}
#endif
    daPoll(FtpClient::activeClients);
#ifdef LOC_SHELL_TEST
    if(previewTrace){FILE*f=fopen("C:\\DOSSHELL\\PHASE.TXT","a");if(f){fprintf(f,"after preview sleeping=%d\n",daSleeping);fclose(f);}}
#endif



    // Every five minutes check to see if we have to resolve the external IP address.
    // (We don't do this if the external IP address was provided directly as numbers.)
    // If it changes or fails to resolve put a message on the screen.

    if ( Pasv_Name[0] ) {
      if ( Timer_diff( lastDnsCheck, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( 300000ul ) ) {

        lastDnsCheck = TIMER_GET_CURRENT( );

        int rc = resolveAddr( );
        if ( rc == 1 ) {
          addToScreen( 1, "%s now resolves to %d.%d.%d.%d\n", Pasv_Name,
                       Pasv_IpAddr[0], Pasv_IpAddr[1], Pasv_IpAddr[2], Pasv_IpAddr[3] );
        } else if ( rc < 0 ) {
          addToScreen( 1, "Warning: Failed to resolve %s\n", Pasv_Name );
        }

      }
    }


    // Check for client inactivity every 10 seconds

    if ( Timer_diff( lastTimeoutSweep, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( 10000 ) ) {

      lastTimeoutSweep = TIMER_GET_CURRENT( );

      for ( uint16_t i=0; i < FtpClient::activeClients; i++ ) {

        FtpClient *client = FtpClient::activeClientsTable[i];

        if ( client->state != FtpClient::Closed ) {

          // Get the newer of the control socket last activity time and the
          // data socket last activity time, if it is in use.

          clockTicks_t last = client->cs->lastActivity;
          if ( client->ds != NULL && client->ds->lastActivity > last ) last = client->ds->lastActivity;

          clockTicks_t diff = Timer_diff( last, TIMER_GET_CURRENT( ) );

          // End them if latest activity is greater than timeout value.
          if ( diff > FtpSrv_timeoutTicks ) {
            Stat_SessionTimeouts++;
            endSession( client );
          }

        }

      } // end for

    }



    // Things to do once a second:
    //
    // - Check to see if we are finished shutting down
    // - Check for Ctrl-Break or Ctrl-C
    // - Read keyboard input
    // - Redraw the status line

    if (Timer_diff(lastKeyboardCheck,TIMER_GET_CURRENT())>TIMER_MS_TO_TICKS(1000)) {
      lastKeyboardCheck=TIMER_GET_CURRENT();
      if(shuttingDown && FtpClient::activeClients==0)return 1;
      if(ShutdownRequested>1&&!shuttingDown){
        for(uint16_t i=0;i<FtpClient::activeClients;i++)endSession(FtpClient::activeClientsTable[i]);
        shuttingDown=1;
      }
    }

    if ( !shuttingDown && !shellDrain && shellEnabled ) {
      // Check for new connections
      TcpSocket *tmpSocket = TcpSocketMgr::accept( );
      if ( tmpSocket != NULL ) {

        // Not really related, but we might as well update the current year each
        // time we get a new connection.  This way we don't have to worry as much
        // about rolling over if the server has been up a while.
        //
        // This is needed for directory listings.

        DosDate_t currentDate;
        getdate( &currentDate );
        Current_Year = currentDate.year;

        processNewConnection( tmpSocket );
      }
    }



    // Service active FTP clients

    for ( uint16_t i=0; i < FtpClient::activeClients; i++ ) {

      FtpClient *client = FtpClient::activeClientsTable[i];

      // If it is in the active list and it went to Closed, then recycle it.
      if ( client->state == FtpClient::Closed ) {

        addToScreen( 1, "(%lu) Disconnect: %d.%d.%d.%d:%u\n", client->sessionId,
          client->cs->dstHost[0], client->cs->dstHost[1],
          client->cs->dstHost[2], client->cs->dstHost[3],
          client->cs->dstPort );

        client->cleanupSession( );

        // Remove from active list and put back on free list
        FtpClient::removeFromActiveList( client );
        FtpClient::returnFreeClient( client );

        // Break the loop and start from scratch because we changed the
        // order of entries in the Client table
        break;

      } // Did client completely close?

      else {
        // Service the socket
        serviceClient( client );
      }

    } // end service active clients



return 0;
}

#include "SHELLBACK.CPP"
