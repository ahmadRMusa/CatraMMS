
#include "CMSEngineProcessor.h"
#include "CheckIngestionTimes.h"

CMSEngineProcessor::CMSEngineProcessor(shared_ptr<spdlog::logger> logger)
{
    _logger     = logger;
}

CMSEngineProcessor::~CMSEngineProcessor()
{
    
}

void CMSEngineProcessor::operator ()(shared_ptr<MultiEventsSet> multiEventsSet) 
{
    bool blocking = true;
    chrono::milliseconds milliSecondsToBlock(100);

    // SPDLOG_DEBUG(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    // SPDLOG_TRACE(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    _logger->info("CMSEngineProcessor thread started");

    bool endEvent = false;
    while(!endEvent)
    {
        // cout << "Calling getAndRemoveFirstEvent" << endl;
        shared_ptr<Event> event = multiEventsSet->getAndRemoveFirstEvent(CMSENGINEPROCESSORNAME, blocking, milliSecondsToBlock);
        if (event == nullptr)
        {
            // cout << "No event found or event not yet expired" << endl;

            continue;
        }

        switch(event->getEventKey().first)
        {
            /*
            case GETDATAEVENT_TYPE:
            {                    
                shared_ptr<GetDataEvent>    getDataEvent = dynamic_pointer_cast<GetDataEvent>(event);
                cout << "getAndRemoveFirstEvent: GETDATAEVENT_TYPE (" << event->getEventKey().first << ", " << event->getEventKey().second << "): " << getDataEvent->getDataId() << endl << endl;
                multiEventsSet.getEventsFactory()->releaseEvent<GetDataEvent>(getDataEvent);
            }
            break;
            */
            case CMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION:	// 1
            {
                _logger->info("Received CMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION");

		handleCheckIngestionEvent ();

                multiEventsSet->getEventsFactory()->releaseEvent<Event>(event);

            }
            break;
            default:
                throw invalid_argument(string("Event type identifier not managed")
                        + to_string(event->getEventKey().first));
        }
    }

    _logger->info("CMSEngineProcessor thread terminated");
}

void CMSEngineProcessor::handleCheckIngestionEvent()
{
/*
	CustomersHashMap_t:: iterator		itCustomersBegin;
	CustomersHashMap_t:: iterator		itCustomersEnd;
	CustomersHashMap_t:: iterator		itCustomers;
	Customer_p							pcCustomer;
	FileIO:: Directory_t				dDirectory;
	Error_t								errOpenDir;
	Error_t								errReadDirectory;
	Error_t								errMove;
	Error_t								errFileTime;
	FileIO:: DirectoryEntryType_t		detDirectoryEntryType;
	long long							llIngestionJobKey;
	unsigned long						ulCurrentCustomerIngestionsNumber;
	time_t								tLastModificationTime;
	unsigned long						ulCurrentCustomerIndex;



	if (_pcCustomers -> lockCustomers (&itCustomersBegin,
		&itCustomersEnd) != errNoError)
	{
		Error err = CMSEngineErrors (__FILE__, __LINE__,
			CMS_CUSTOMERS_LOCKCUSTOMERS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	for (itCustomers = itCustomersBegin,
		ulCurrentCustomerIndex = 0;
		itCustomers != itCustomersEnd &&
		ulCurrentCustomerIndex < *_pulPreviousCustomerIndex;
		++itCustomers, ulCurrentCustomerIndex++)
	{
	}

	for (; itCustomers != itCustomersEnd; ++itCustomers)
	{
		pcCustomer	= itCustomers -> second;

		if (_pmtIngestedAssetThread -> lock () != errNoError)
		{
			Error err = PThreadErrors (__FILE__, __LINE__,
				THREADLIB_PMUTEX_LOCK_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			continue;
		}

		if (*_pulIngestedAssetThreadNumber >= _ulMaxIngestedAssetThreadsNumber)
		{
			{
				Message msg = CMSEngineMessages (__FILE__, __LINE__,
					CMS_CMSENGINEPROCESSOR_REACHEDMAXNUMBEROFINGESTIONTHREAD,
					1,
					*_pulIngestedAssetThreadNumber);
				_ptSystemTracer -> trace (Tracer:: TRACER_LWRNG,
					(const char *) msg, __FILE__, __LINE__);
			}

			if (_pmtIngestedAssetThread -> unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				continue;
			}

			break;
		}

		if (_pmtIngestedAssetThread -> unLock () != errNoError)
		{
			Error err = PThreadErrors (__FILE__, __LINE__,
				THREADLIB_PMUTEX_UNLOCK_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			continue;
		}

		(*_pulPreviousCustomerIndex)++;

		if (_bCustomerFTPDirectory. setBuffer (
			_pcrCMSRepository -> getFTPRootRepository ()) !=
			errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_SETBUFFER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);

			continue;
		}

		if (_bCustomerFTPDirectory. append (
			(const char *) (pcCustomer -> _bName)) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_BUFFER_APPEND_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);

			continue;
		}

		{
			Message msg = CMSEngineMessages (__FILE__, __LINE__,
				CMS_CHECKINGESTIONTHREAD_CUSTOMERINGESTING,
				2,
				(const char *) (pcCustomer -> _bName),
				(const char *) _bCustomerFTPDirectory);
			_ptSystemTracer -> trace (Tracer:: TRACER_LDBG5,
				(const char *) msg, __FILE__, __LINE__);
		}

		if ((errOpenDir = FileIO:: openDirectory (
			(const char *) _bCustomerFTPDirectory,
			&dDirectory)) != errNoError)
		{
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) errOpenDir, __FILE__, __LINE__);

			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_OPENDIRECTORY_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);

			continue;
		}

		ulCurrentCustomerIngestionsNumber		= 0;

		while ((errReadDirectory = FileIO:: readDirectory (&dDirectory,
			&_bDirectoryEntry, &detDirectoryEntryType)) == errNoError)
		{
			if (detDirectoryEntryType != FileIO:: TOOLS_FILEIO_REGULARFILE)
			{
				continue;
			}

			// check if the file has ".xml" as extension.
			// We do not accept also the ".xml" file (without name)
			if ((unsigned long) _bDirectoryEntry < 5 ||
				strcmp (((const char *) _bDirectoryEntry) +
					((unsigned long) _bDirectoryEntry) - 4, ".xml"))
				continue;

			#ifdef WIN32
				if (_bSrcPathName. setBuffer (
					(const char *) _bCustomerFTPDirectory) !=
						errNoError ||
					_bSrcPathName. append ("\\") != errNoError ||
					_bSrcPathName. append ((const char *) _bDirectoryEntry) !=
					errNoError)
			#else
				if (_bSrcPathName. setBuffer (
					(const char *) _bCustomerFTPDirectory) !=
						errNoError ||
					_bSrcPathName. append ("/") != errNoError ||
					_bSrcPathName. append ((const char *) _bDirectoryEntry) !=
					errNoError)
			#endif
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_BUFFER_SETBUFFER_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

//				if (FileIO:: closeDirectory (&dDirectory) != errNoError)
//				{
//					Error err = ToolsErrors (__FILE__, __LINE__,
//						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);
//				}
//
//				return err;

				continue;
			}

			if ((errFileTime = FileIO:: getFileTime (
				(const char *) _bSrcPathName,
				&tLastModificationTime)) != errNoError)
			{
				int					iErrno;
				unsigned long		ulUserDataBytes;


				errFileTime. getUserData (&iErrno, &ulUserDataBytes);
				if (iErrno != ENOENT)	// ENOENT: file not found
				{
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) errFileTime, __FILE__, __LINE__);

					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_FILEIO_GETFILETIME_FAILED,
						1, (const char *) _bSrcPathName);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

//				if (FileIO:: closeDirectory (&dDirectory) != errNoError)
//				{
//					Error err = ToolsErrors (__FILE__, __LINE__,
//						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);
//				}
//
//				return err;

				continue;
			}

			if (time (NULL) - tLastModificationTime <
				_ulCheckIngestionPeriodInSeconds)
			{
				// only the XML files having the last modification older
				// at least of _ulCheckIngestionPeriodInSeconds seconds
				// are considered

				continue;
			}

			if (_pcrCMSRepository -> getStagingAssetPathName (
				&_bDestPathName,
				(const char *) (pcCustomer -> _bName),
				"/",
				(const char *) _bDirectoryEntry,
				0, 0,
				false, true) != errNoError)
			{
				Error err = CMSEngineErrors (__FILE__, __LINE__,
					CMS_CMSREPOSITORY_GETSTAGINGASSETPATHNAME_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

//				if (FileIO:: closeDirectory (&dDirectory) != errNoError)
//				{
//					Error err = ToolsErrors (__FILE__, __LINE__,
//						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);
//				}
//
//				return err;

				continue;
			}

			{
				Message msg = CMSEngineMessages (__FILE__, __LINE__,
					CMS_INGESTASSETTHREAD_MOVEFILE,
					3,
					(const char *) (pcCustomer -> _bName),
					(const char *) _bSrcPathName,
					(const char *) _bDestPathName);
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			if ((errMove = FileIO:: moveFile (
				(const char *) _bSrcPathName,
				(const char *) _bDestPathName)) != errNoError)
			{
				// often the move failed because some other cms will do the move
				// before us
				_ptSystemTracer -> trace (Tracer:: TRACER_LWRNG,
					(const char *) errMove, __FILE__, __LINE__);

				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_MOVEFILE_FAILED,
					2, (const char *) _bSrcPathName,
					(const char *) _bDestPathName);
				_ptSystemTracer -> trace (Tracer:: TRACER_LWRNG,
					(const char *) err, __FILE__, __LINE__);

				continue;
			}

			// clean dirty characters at the beginning of the XML (if present)
			{
				if (_bXMLFile. readBufferFromFile (
					(const char *) _bDestPathName) != errNoError)
				{
					Error err = ToolsErrors (__FILE__, __LINE__,
						TOOLS_BUFFER_READBUFFERFROMFILE_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					if (_pcrCMSRepository -> moveContentInRepository (
						(const char *) _bDestPathName,
						CMSRepository:: CMSREP_REPOSITORYTYPE_ERRORS,
						(const char *) (pcCustomer -> _bName),
						true) != errNoError)
					{
						Error err = CMSRepositoryErrors (__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_MOVECONTENTINREPOSITORY_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

//					if (FileIO:: closeDirectory (&dDirectory) != errNoError)
//					{
//						Error err = ToolsErrors (__FILE__, __LINE__,
//							TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//							(const char *) err, __FILE__, __LINE__);
//					}
//
//					return err;

					continue;
				}

				if (strchr ((const char *) _bXMLFile, '<') != (char *) NULL)
				{
					unsigned long		ulDirtyCharactersLength;


					ulDirtyCharactersLength		=
						strchr ((const char *) _bXMLFile, '<') -
						(const char *) _bXMLFile;

					if (ulDirtyCharactersLength > 0)
					{
						if (_bXMLFile. strip (Buffer:: STRIPTYPE_LEADING,
							ulDirtyCharactersLength) != errNoError)
						{
							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_BUFFER_STRIP_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							if (_pcrCMSRepository -> moveContentInRepository (
								(const char *) _bDestPathName,
								CMSRepository:: CMSREP_REPOSITORYTYPE_ERRORS,
								(const char *) (pcCustomer -> _bName),
								true) != errNoError)
							{
								Error err = CMSRepositoryErrors (
									__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_MOVECONTENTINREPOSITORY_FAILED);
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);
							}

//							if (FileIO:: closeDirectory (&dDirectory) !=
//								errNoError)
//							{
//								Error err = ToolsErrors (__FILE__, __LINE__,
//									TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//									(const char *) err, __FILE__, __LINE__);
//							}
//
//							return err;

							continue;
						}

						if (_bXMLFile. writeBufferOnFile (
							(const char *) _bDestPathName) != errNoError)
						{
							Error err = ToolsErrors (__FILE__, __LINE__,
								TOOLS_BUFFER_WRITEBUFFERONFILE_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							if (_pcrCMSRepository -> moveContentInRepository (
								(const char *) _bDestPathName,
								CMSRepository:: CMSREP_REPOSITORYTYPE_ERRORS,
								(const char *) (pcCustomer -> _bName),
								true) != errNoError)
							{
								Error err = CMSRepositoryErrors (
									__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_MOVECONTENTINREPOSITORY_FAILED);
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);
							}

//							if (FileIO:: closeDirectory (&dDirectory) !=
//								errNoError)
//							{
//								Error err = ToolsErrors (__FILE__, __LINE__,
//									TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//									(const char *) err, __FILE__, __LINE__);
//							}
//
//							return err;

							continue;
						}
					}
				}
			}

			if (IngestAssetThread:: insertIngestionJobStatusOnFileAndDB (
				_pcrCMSRepository -> getFTPRootRepository (),
				pcCustomer -> _bName. str(),
				pcCustomer -> _bDirectoryName. str(),
				_bDirectoryEntry. str(),
				&llIngestionJobKey,
				_plbWebServerLoadBalancer,
				_pWebServerLocalIPAddress,
				_ulWebServerTimeoutToWaitAnswerInSeconds,
				_ptSystemTracer) != errNoError)
			{
				Error err = CMSEngineErrors (__FILE__, __LINE__,
			CMS_INGESTASSETTHREAD_INSERTINGESTIONJOBSTATUSONFILEANDDB_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (_pcrCMSRepository -> moveContentInRepository (
					(const char *) _bDestPathName,
					CMSRepository:: CMSREP_REPOSITORYTYPE_ERRORS,
					(const char *) (pcCustomer -> _bName),
					true) != errNoError)
				{
					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
					CMSREP_CMSREPOSITORY_MOVECONTENTINREPOSITORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

//				if (FileIO:: closeDirectory (&dDirectory) != errNoError)
//				{
//					Error err = ToolsErrors (__FILE__, __LINE__,
//						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);
//				}
//
//				return err;

				continue;
			}

			{
				Message msg = CMSEngineMessages (__FILE__, __LINE__,
					CMS_CMSENGINEPROCESSOR_ASSETTOINGEST,
					2,
					(const char *) (pcCustomer -> _bName),
					(const char *) _bDirectoryEntry);
				_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
					(const char *) msg, __FILE__, __LINE__);
			}

			if (sendIngestAssetEvent (
				pcCustomer,
				(const char *) _bDestPathName,
				llIngestionJobKey) != errNoError)
			{
				Error err = CMSEngineErrors (__FILE__, __LINE__,
					CMS_CMSENGINEPROCESSOR_SENDINGESTASSETEVENT_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				if (IngestAssetThread:: updateIngestionJobStatusOnFileAndDB (
					_pcrCMSRepository,
					_pesEventsSet,
					_pcCustomers,
					"8",
					false,
					(const char *) err,
					llIngestionJobKey,
					-1,
					_pcrCMSRepository -> getFTPRootRepository (),
					pcCustomer -> _bName. str(),
					pcCustomer -> _bDirectoryName. str(),
					_plbWebServerLoadBalancer,
					_pWebServerLocalIPAddress,
					_ulWebServerTimeoutToWaitAnswerInSeconds,
					_ptSystemTracer) != errNoError)
				{
					Error err = CMSEngineErrors (__FILE__, __LINE__,
			CMS_INGESTASSETTHREAD_UPDATEINGESTIONJOBSTATUSONFILEANDDB_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				if (_pcrCMSRepository -> moveContentInRepository (
					(const char *) _bDestPathName,
					CMSRepository:: CMSREP_REPOSITORYTYPE_ERRORS,
					(const char *) (pcCustomer -> _bName),
					true) != errNoError)
				{
					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
						CMSREP_CMSREPOSITORY_MOVECONTENTINREPOSITORY_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

//				if (FileIO:: closeDirectory (&dDirectory) != errNoError)
//				{
//					Error err = ToolsErrors (__FILE__, __LINE__,
//						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);
//				}
//
//				return err;

				continue;
			}

			ulCurrentCustomerIngestionsNumber++;

			if (ulCurrentCustomerIngestionsNumber >=
				_ulMaxIngestionsNumberPerCustomerEachIngestionPeriod)
				break;
		}

		if (ulCurrentCustomerIngestionsNumber <
			_ulMaxIngestionsNumberPerCustomerEachIngestionPeriod &&
			(long) errReadDirectory != TOOLS_FILEIO_DIRECTORYFILESFINISHED)
		{
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) errReadDirectory, __FILE__, __LINE__);

			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_READDIRECTORY_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (FileIO:: closeDirectory (&dDirectory) != errNoError)
			{
				Error err = ToolsErrors (__FILE__, __LINE__,
					TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

//			return errReadDirectory;

			continue;
		}

		if (FileIO:: closeDirectory (&dDirectory) != errNoError)
		{
			Error err = ToolsErrors (__FILE__, __LINE__,
				TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

//			return err;

			continue;
		}
	}

	if (itCustomers == itCustomersEnd)
		*_pulPreviousCustomerIndex		= 0;

	if (_pcCustomers -> unLockCustomers () != errNoError)
	{
		Error err = CMSEngineErrors (__FILE__, __LINE__,
			CMS_CUSTOMERS_UNLOCKCUSTOMERS_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

//	if (_pmtIngestedAssetThread -> lock () != errNoError)
//	{
//		Error err = PThreadErrors (__FILE__, __LINE__,
//			THREADLIB_PMUTEX_LOCK_FAILED);
//		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//			(const char *) err, __FILE__, __LINE__);
//
//		return err;
//	}
//
//	{
//		Message msg = CMSEngineMessages (__FILE__, __LINE__,
//			CMS_CMSENGINEPROCESSOR_INGESTASSETTHREADSNUMBER,
//			1,
//			*_pulIngestedAssetThreadNumber);
//		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
//			(const char *) msg, __FILE__, __LINE__);
//	}
//
//	if (_pmtIngestedAssetThread -> unLock () != errNoError)
//	{
//		Error err = PThreadErrors (__FILE__, __LINE__,
//			THREADLIB_PMUTEX_UNLOCK_FAILED);
//		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//			(const char *) err, __FILE__, __LINE__);
//
//		return err;
//	}


	return errNoError;
        */
}


/*
Error CheckIngestionThread:: sendIngestAssetEvent (
	Customer_p pcCustomer,
	const char *pMetaFilePathName,
	long long llIngestionJobKey)

{

	IngestAssetEvent_p			pevIngestAssetEvent;
	Event_p						pevEvent;


	if (_pesEventsSet -> getFreeEvent (
		CMSEngineEventsSet:: CMS_EVENTTYPE_INGESTASSETIDENTIFIER,
		&pevEvent) != errNoError)
	{
		Error err = EventsSetErrors (__FILE__, __LINE__,
			EVSET_EVENTSSET_GETFREEEVENT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	pevIngestAssetEvent			= (IngestAssetEvent_p) pevEvent;

	if (pevIngestAssetEvent -> init (
		CMS_CHECKINGESTIONTHREAD_SOURCE,
		(const char *) _bMainProcessor,
		pcCustomer,
		pMetaFilePathName,
		llIngestionJobKey,
		_ptSystemTracer) != errNoError)
	{
		Error err = EventsSetErrors (__FILE__, __LINE__,
			EVSET_EVENT_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (_pesEventsSet -> releaseEvent (
			CMSEngineEventsSet:: CMS_EVENTTYPE_INGESTASSETIDENTIFIER,
			pevIngestAssetEvent) != errNoError)
		{
			Error err = EventsSetErrors (__FILE__, __LINE__,
				EVSET_EVENTSSET_RELEASEEVENT_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (_pesEventsSet -> addEvent (pevIngestAssetEvent) != errNoError)
	{
		Error err = EventsSetErrors (__FILE__, __LINE__,
			EVSET_EVENTSSET_ADDEVENT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (pevIngestAssetEvent -> finish () != errNoError)
		{
			Error err = EventsSetErrors (__FILE__, __LINE__,
				EVSET_EVENT_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		if (_pesEventsSet -> releaseEvent (
			CMSEngineEventsSet:: CMS_EVENTTYPE_INGESTASSETIDENTIFIER,
			pevIngestAssetEvent) != errNoError)
		{
			Error err = EventsSetErrors (__FILE__, __LINE__,
				EVSET_EVENTSSET_RELEASEEVENT_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}


	return errNoError;
}
*/
