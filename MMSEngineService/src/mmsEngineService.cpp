
#include <thread>
#include <fstream>
#include <csignal>

#include "catralibraries/System.h"
#include "catralibraries/Service.h"
#include "catralibraries/Scheduler2.h"

#include "MMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "CheckEncodingTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "MainAndBackupRunningHALiveRecordingEvent.h"
#include "MMSEngineDBFacade.h"
#include "ActiveEncodingsManager.h"
#include "MMSStorage.h"
#include "Magick++.h"

Json::Value loadConfigurationFile(string configurationPathName);

void signalHandler(int signal)
{
    auto logger = spdlog::get("mmsEngineService");
    
    logger->error(__FILEREF__ + "Received a signal"
        + ", signal: " + to_string(signal)
    );
}

int main (int iArgc, char *pArgv [])
{
    
    if (iArgc != 2 && iArgc != 3 && iArgc != 4)
    {
        cerr << "Usage: " 
                << pArgv[0] 
                << " --nodaemon | --resetdata | --pidfile <pid path file name>" 
                << "config-path-name" 
                << endl;
        
        return 1;
    }
    
    bool noDaemon = false;
    bool resetdata = false;
    string pidFilePathName;
    string configPathName;
    
    if (iArgc == 2)
    {
		// only cfg file
        pidFilePathName = "/tmp/cmsEngine.pid";
    
        configPathName = pArgv[1];
    }
    else if (iArgc == 3)
    {
		// nodaemon and cfg file
        pidFilePathName = "/tmp/cmsEngine.pid";

        if (!strcmp (pArgv[1], "--nodaemon"))
            noDaemon = true;
		else if (!strcmp (pArgv[1], "--resetdata"))
            resetdata = true;

        configPathName = pArgv[2];
    }
    else if (iArgc == 4)
    {
		// pidfile (2 params) and cfg file
        if (!strcmp (pArgv[1], "--pidfile"))
            pidFilePathName = pArgv[2];
        else
            pidFilePathName = "/tmp/cmsEngine.pid";

        configPathName = pArgv[3];
    }
    
    if (!noDaemon)
        Service::launchUnixDaemon(pidFilePathName);

    Magick::InitializeMagick(*pArgv);
    
    Json::Value configuration = loadConfigurationFile(configPathName);

    string logPathName =  configuration["log"]["mms"].get("pathName", "XXX").asString();
    bool stdout =  configuration["log"]["mms"].get("stdout", "XXX").asBool();
    
    std::vector<spdlog::sink_ptr> sinks;
    auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logPathName.c_str(), 11, 20);
    sinks.push_back(dailySink);
    if (stdout)
    {
        auto stdoutSink = spdlog::sinks::stdout_sink_mt::instance();
        sinks.push_back(stdoutSink);
    }
    auto logger = std::make_shared<spdlog::logger>("mmsEngineService", begin(sinks), end(sinks));
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    spdlog::register_logger(logger);

    // auto logger = spdlog::stdout_logger_mt("mmsEngineService");
    // auto logger = spdlog::daily_logger_mt("mmsEngineService", logPathName.c_str(), 11, 20);
    
    // trigger flush if the log severity is error or higher
    logger->flush_on(spdlog::level::trace);
    
    string logLevel =  configuration["log"]["mms"].get("level", "XXX").asString();
    if (logLevel == "debug")
        spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "info")
        spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "err")
        spdlog::set_level(spdlog::level::err); // trace, debug, info, warn, err, critical, off

    string pattern =  configuration["log"]["mms"].get("pattern", "XXX").asString();
    spdlog::set_pattern(pattern);

    // install a signal handler
    signal(SIGSEGV, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGABRT, signalHandler);
    // signal(SIGBUS, signalHandler);

    size_t dbPoolSize = configuration["database"].get("enginePoolSize", 5).asInt();
    logger->info(__FILEREF__ + "Configuration item"
        + ", database->poolSize: " + to_string(dbPoolSize)
    );
    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, dbPoolSize, logger);
    
	if (resetdata)
	{
		string processorMMS                   = System::getHostName();

        try
        {
            mmsEngineDBFacade->resetProcessingJobsIfNeeded(processorMMS);
        }
        catch(runtime_error e)
        {
            logger->error(__FILEREF__ + "_mmsEngineDBFacade->resetProcessingJobsIfNeeded failed"
                    + ", exception: " + e.what()
            );

            // throw e;
			return 1;
        }
        catch(exception e)
        {
            logger->error(__FILEREF__ + "_mmsEngineDBFacade->resetProcessingJobsIfNeeded failed"
            );

            // throw e;
			return 1;
        }

		return 0;
	}

    logger->info(__FILEREF__ + "Creating MMSStorage"
            );
    shared_ptr<MMSStorage>       mmsStorage = make_shared<MMSStorage>(
            configuration, logger);
    
    logger->info(__FILEREF__ + "Creating MultiEventsSet"
        + ", addDestination: " + MMSENGINEPROCESSORNAME
            );
    shared_ptr<MultiEventsSet>          multiEventsSet = make_shared<MultiEventsSet>();
    multiEventsSet->addDestination(MMSENGINEPROCESSORNAME);

    logger->info(__FILEREF__ + "Creating ActiveEncodingsManager"
            );
    ActiveEncodingsManager      activeEncodingsManager(configuration, multiEventsSet, mmsEngineDBFacade, mmsStorage, logger);

//    MMSEngineProcessor      mmsEngineProcessor(0, logger, multiEventsSet, 
//            mmsEngineDBFacade, mmsStorage, &activeEncodingsManager, configuration);
    vector<shared_ptr<MMSEngineProcessor>>      mmsEngineProcessors;
    {
        int processorThreads =  configuration["mms"].get("processorThreads", 1).asInt();
        shared_ptr<long> processorsThreadsNumber = make_shared<long>(0);

        for (int processorThreadIndex = 0; processorThreadIndex < processorThreads; processorThreadIndex++)
        {
            logger->info(__FILEREF__ + "Creating MMSEngineProcessor"
                + ", processorThreadIndex: " + to_string(processorThreadIndex)
                    );
            shared_ptr<MMSEngineProcessor>      mmsEngineProcessor = 
                    make_shared<MMSEngineProcessor>(processorThreadIndex, logger, multiEventsSet, 
                        mmsEngineDBFacade, mmsStorage, processorsThreadsNumber,
                    &activeEncodingsManager, configuration);
            mmsEngineProcessors.push_back(mmsEngineProcessor);
        }
    }    
    
    unsigned long           ulThreadSleepInMilliSecs = configuration["scheduler"].get("threadSleepInMilliSecs", 5).asInt();
    logger->info(__FILEREF__ + "Creating Scheduler2"
        + ", ulThreadSleepInMilliSecs: " + to_string(ulThreadSleepInMilliSecs)
            );
    Scheduler2              scheduler(ulThreadSleepInMilliSecs);


    logger->info(__FILEREF__ + "Starting ActiveEncodingsManager"
            );
    thread activeEncodingsManagerThread (ref(activeEncodingsManager));
    
    vector<shared_ptr<thread>> mmsEngineProcessorsThread;
    {
        //    thread mmsEngineProcessorThread (mmsEngineProcessor);
        for (int mmsProcessorIndex = 0; mmsProcessorIndex < mmsEngineProcessors.size(); mmsProcessorIndex++)
        {
            logger->info(__FILEREF__ + "Starting MMSEngineProcessor"
                + ", mmsProcessorIndex: " + to_string(mmsProcessorIndex)
                    );
            mmsEngineProcessorsThread.push_back(make_shared<thread>(&MMSEngineProcessor::operator(), mmsEngineProcessors[mmsProcessorIndex]));
        }
    }    

    logger->info(__FILEREF__ + "Starting Scheduler2"
            );
    thread schedulerThread (ref(scheduler));

    unsigned long           checkIngestionTimesPeriodInMilliSecs = configuration["scheduler"].get("checkIngestionTimesPeriodInMilliSecs", 2000).asInt();
    logger->info(__FILEREF__ + "Creating and Starting CheckIngestionTimes"
        + ", checkIngestionTimesPeriodInMilliSecs: " + to_string(checkIngestionTimesPeriodInMilliSecs)
            );
    shared_ptr<CheckIngestionTimes>     checkIngestionTimes =
            make_shared<CheckIngestionTimes>(checkIngestionTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkIngestionTimes->start();
    scheduler.activeTimes(checkIngestionTimes);

    unsigned long           checkEncodingTimesPeriodInMilliSecs = configuration["scheduler"].get("checkEncodingTimesPeriodInMilliSecs", 10000).asInt();
    logger->info(__FILEREF__ + "Creating and Starting CheckEncodingTimes"
        + ", checkEncodingTimesPeriodInMilliSecs: " + to_string(checkEncodingTimesPeriodInMilliSecs)
            );
    shared_ptr<CheckEncodingTimes>     checkEncodingTimes =
            make_shared<CheckEncodingTimes>(checkEncodingTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkEncodingTimes->start();
    scheduler.activeTimes(checkEncodingTimes);

    string           contentRetentionTimesSchedule = configuration["scheduler"].get("contentRetentionTimesSchedule", "").asString();
    logger->info(__FILEREF__ + "Creating and Starting ContentRetentionTimes"
        + ", contentRetentionTimesSchedule: " + contentRetentionTimesSchedule
            );
    shared_ptr<ContentRetentionTimes>     contentRetentionTimes =
            make_shared<ContentRetentionTimes>(contentRetentionTimesSchedule, multiEventsSet, logger);
    contentRetentionTimes->start();
    scheduler.activeTimes(contentRetentionTimes);

    string           dbDataRetentionTimesSchedule =
		configuration["scheduler"].get("dbDataRetentionTimesSchedule", "").asString();
    logger->info(__FILEREF__ + "Creating and Starting DBDataRetentionTimes"
        + ", dbDataRetentionTimesSchedule: " + dbDataRetentionTimesSchedule
            );
    shared_ptr<DBDataRetentionTimes>     dbDataRetentionTimes =
            make_shared<DBDataRetentionTimes>(dbDataRetentionTimesSchedule, multiEventsSet, logger);
    dbDataRetentionTimes->start();
    scheduler.activeTimes(dbDataRetentionTimes);

    string			checkRefreshPartitionFreeSizeTimesSchedule =
		configuration["scheduler"].get("checkRefreshPartitionFreeSizeTimesSchedule", "").asString();
    logger->info(__FILEREF__ + "Creating and Starting CheckRefreshPartitionFreeSizeTimes"
        + ", checkRefreshPartitionFreeSizeTimesSchedule: "
			+ checkRefreshPartitionFreeSizeTimesSchedule
            );
    shared_ptr<CheckRefreshPartitionFreeSizeTimes>     checkRefreshPartitionFreeSizeTimes =
            make_shared<CheckRefreshPartitionFreeSizeTimes>(checkRefreshPartitionFreeSizeTimesSchedule, multiEventsSet, logger);
    checkRefreshPartitionFreeSizeTimes->start();
    scheduler.activeTimes(checkRefreshPartitionFreeSizeTimes);

    string           mainAndBackupRunningHALiveRecordingTimesSchedule = configuration["scheduler"].get("mainAndBackupRunningHALiveRecordingTimesSchedule", "").asString();
    logger->info(__FILEREF__ + "Creating and Starting MainAndBackupRunningHALiveRecordingEvent"
        + ", mainAndBackupRunningHALiveRecordingTimesSchedule: " + mainAndBackupRunningHALiveRecordingTimesSchedule
            );
    shared_ptr<MainAndBackupRunningHALiveRecordingEvent>     mainAndBackupRunningHALiveRecordingTimes =
            make_shared<MainAndBackupRunningHALiveRecordingEvent>(mainAndBackupRunningHALiveRecordingTimesSchedule, multiEventsSet, logger);
    mainAndBackupRunningHALiveRecordingTimes->start();
    scheduler.activeTimes(mainAndBackupRunningHALiveRecordingTimes);


    logger->info(__FILEREF__ + "Waiting ActiveEncodingsManager"
            );
    activeEncodingsManagerThread.join();
    
    {
        for (int mmsProcessorIndex = 0; mmsProcessorIndex < mmsEngineProcessorsThread.size(); mmsProcessorIndex++)
        {
            logger->info(__FILEREF__ + "Waiting MMSEngineProcessor"
                + ", mmsProcessorIndex: " + to_string(mmsProcessorIndex)
                    );
            mmsEngineProcessorsThread[mmsProcessorIndex]->join();
        }
    }    
        
    logger->info(__FILEREF__ + "Waiting Scheduler2"
            );
    schedulerThread.join();

    logger->info(__FILEREF__ + "Shutdown done"
            );
    
    return 0;
}

Json::Value loadConfigurationFile(string configurationPathName)
{
    Json::Value configurationJson;
    
    try
    {
        ifstream configurationFile(configurationPathName.c_str(), std::ifstream::binary);
        configurationFile >> configurationJson;
    }
    catch(...)
    {
        cerr << string("wrong json configuration format")
                + ", configurationPathName: " + configurationPathName
            << endl;
    }
    
    return configurationJson;
}
