/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   EnodingsManager.cpp
 * Author: giuliano
 * 
 * Created on February 4, 2018, 7:18 PM
 */

#include <fstream>
#include <sstream>
#include <regex>
#include "catralibraries/ProcessUtility.h"
#include "EncoderVideoAudioProxy.h"


#ifdef __APPLE__
#define _ffmpegPath  string("/Users/multi/GestioneProgetti/Development/catrasoftware/usr_local/bin")
#else
#define _ffmpegPath  string("/app/7/DevelopmentWorkingArea/usr_local/bin")
#endif


#define _charsToBeReadFromFfmpegErrorOutput 1024

EncoderVideoAudioProxy::EncoderVideoAudioProxy()
{
    /*
    _ffmpegPath = configuration["ffmpeg"].get("path", "XXX").asString();
    if (_ffmpegPath.back() == '/')
        _ffmpegPath.pop_back();
     */
}

EncoderVideoAudioProxy::~EncoderVideoAudioProxy() 
{
}

void EncoderVideoAudioProxy::setData(
        mutex* mtEncodingJobs,
        EncodingJobStatus* status,
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        shared_ptr<MMSStorage> mmsStorage,
        shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem,
        #ifdef __FFMPEGLOCALENCODER__
            int* pffmpegEncoderRunning,
        #endif
        shared_ptr<spdlog::logger> logger
)
{
    _logger                 = logger;
    
    _mtEncodingJobs         = mtEncodingJobs;
    _status                 = status;
    
    _mmsEngineDBFacade      = mmsEngineDBFacade;
    _mmsStorage             = mmsStorage;
    _encodingItem           = encodingItem;
    _twoPasses              = false;
    _currentlyAtSecondPass             = false;
    
    _MP4Encoder            = "FFMPEG";
    _mpeg2TSEncoder         = "FFMPEG";
    
    _outputFfmpegPathFileName   = "";
    
    #ifdef __FFMPEGLOCALENCODER__
        _ffmpegMaxCapacity      = 1;
        
        _pffmpegEncoderRunning  = pffmpegEncoderRunning;
        (*_pffmpegEncoderRunning)++;
    #endif
}

void EncoderVideoAudioProxy::operator()()
{
    
    string stagingEncodedAssetPathName;
    try
    {
        #ifdef __FFMPEGLOCALENCODER__
            if (*_pffmpegEncoderRunning > _ffmpegMaxCapacity)
            {
                _logger->info("Max ffmpeg encoder capacity is reached");

                throw MaxConcurrentJobsReached();
            }
        #endif
        
        stagingEncodedAssetPathName = encodeContentVideoAudio();
    }
    catch(MaxConcurrentJobsReached e)
    {
        _logger->warn(__FILEREF__ + "encodeContentVideoAudio: " + e.what());
        
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob MaxCapacityReached"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::MaxCapacityReached, _encodingItem->_ingestionJobKey);
        
        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            #ifdef __FFMPEGLOCALENCODER__
                (*_pffmpegEncoderRunning)--;
            #endif
            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
    catch(EncoderError e)
    {
        _logger->error(__FILEREF__ + "encodeContentVideoAudio: " + e.what());
        
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError, _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            #ifdef __FFMPEGLOCALENCODER__
                (*_pffmpegEncoderRunning)--;
            #endif
            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "encodeContentVideoAudio: " + e.what());
        
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            #ifdef __FFMPEGLOCALENCODER__
                (*_pffmpegEncoderRunning)--;
            #endif
            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "encodeContentVideoAudio: " + e.what());
        
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            #ifdef __FFMPEGLOCALENCODER__
                (*_pffmpegEncoderRunning)--;
            #endif
            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
            
    try
    {
        processEncodedContentVideoAudio(stagingEncodedAssetPathName);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processEncodedContentVideoAudio: " + e.what());
        
        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

        _logger->error(__FILEREF__ + "Remove"
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        // file in case of .3gp content OR directory in case of IPhone content
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
        {
            Boolean_t bRemoveRecursively = true;
            FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
        }
        else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
        {
            FileIO::remove(stagingEncodedAssetPathName);
        }

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            #ifdef __FFMPEGLOCALENCODER__
                (*_pffmpegEncoderRunning)--;
            #endif
            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }

    try
    {
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob NoError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        _mmsEngineDBFacade->updateEncodingJob (
            _encodingItem->_encodingJobKey, 
            MMSEngineDBFacade::EncodingError::NoError, 
            _encodingItem->_ingestionJobKey);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob failed: " + e.what());

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            #ifdef __FFMPEGLOCALENCODER__
                (*_pffmpegEncoderRunning)--;
            #endif
            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
    
    {
        lock_guard<mutex> locker(*_mtEncodingJobs);

        #ifdef __FFMPEGLOCALENCODER__
            (*_pffmpegEncoderRunning)--;
        #endif
        *_status = EncodingJobStatus::Free;
    }        
}

string EncoderVideoAudioProxy::encodeContentVideoAudio()
{
    string stagingEncodedAssetPathName;
    
    _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
        + ", _encodingItem->_encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodingProfileTechnology))
        + ", _MP4Encoder: " + _MP4Encoder
    );

    if (
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MP4 &&
            _MP4Encoder == "FFMPEG") ||
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS &&
            _mpeg2TSEncoder == "FFMPEG") ||
        _encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WEBM ||
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe &&
            _mpeg2TSEncoder == "FFMPEG")
    )
    {
        stagingEncodedAssetPathName = encodeContent_VideoAudio_through_ffmpeg();
    }
    else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    return stagingEncodedAssetPathName;
}

int64_t EncoderVideoAudioProxy::getVideoOrAudioDurationInMilliSeconds(
    string mmsAssetPathName)
{
    auto logger = spdlog::get("mmsEngineService");

    size_t fileNameIndex = mmsAssetPathName.find_last_of("/");
    if (fileNameIndex == string::npos)
    {
        string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                + ", mmsAssetPathName: " + mmsAssetPathName;
        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    string sourceFileName = mmsAssetPathName.substr(fileNameIndex + 1);

    string      durationPathFileName =
            string("/tmp/") + sourceFileName + ".duration";
    
    /*
     * ffprobe:
        "-v quiet": Don't output anything else but the desired raw data value
        "-print_format": Use a certain format to print out the data
        "compact=": Use a compact output format
        "print_section=0": Do not print the section name
        ":nokey=1": do not print the key of the key:value pair
        ":escape=csv": escape the value
        "-show_entries format=duration": Get entries of a field named duration inside a section named format
    */
    string ffprobeExecuteCommand = 
            _ffmpegPath + "/ffprobe "
            + "-v quiet -print_format compact=print_section=0:nokey=1:escape=csv -show_entries format=duration "
            + mmsAssetPathName + " "
            + "> " + durationPathFileName 
            + " 2>&1"
            ;

    #ifdef __APPLE__
        ffprobeExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

    logger->info(__FILEREF__ + "Executing ffprobe command"
        + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
    );

    try
    {
        int executeCommandStatus = ProcessUtility:: execute (ffprobeExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "ffprobe command failed"
                    + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
            ;

            logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(exception e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                durationPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffprobe command failed"
                + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
        ;
        logger->error(errorMessage);

        bool exceptionInCaseOfError = false;
        FileIO::remove(durationPathFileName, exceptionInCaseOfError);

        throw e;
    }

    int64_t      videoOrAudioDurationInMilliSeconds;
    {        
        ifstream durationFile(durationPathFileName);
        stringstream buffer;
        buffer << durationFile.rdbuf();
        
        logger->info(__FILEREF__ + "Duration found"
            + ", mmsAssetPathName: " + mmsAssetPathName
            + ", durationInSeconds: " + buffer.str()
        );

        double durationInSeconds = atof(buffer.str().c_str());
        
        videoOrAudioDurationInMilliSeconds  = durationInSeconds * 1000;
        
        bool exceptionInCaseOfError = false;
        FileIO::remove(durationPathFileName, exceptionInCaseOfError);
    }

    
    return videoOrAudioDurationInMilliSeconds;
}

void EncoderVideoAudioProxy::generateScreenshotToIngest(
    string imagePathName,
    double timePositionInSeconds,
    int sourceImageWidth,
    int sourceImageHeight,
    string mmsAssetPathName)
{
    auto logger = spdlog::get("mmsEngineService");

    // ffmpeg -y -i [source.wmv] -f mjpeg -ss [10] -vframes 1 -an -s [176x144] [thumbnail_image.jpg]
    // -y: overwrite output files
    // -i: input file name
    // -f: force format
    // -ss: set the start time offset
    // -vframes: set the number of video frames to record
    // -an: disable audio
    // -s set frame size (WxH or abbreviation)

    size_t extensionIndex = imagePathName.find_last_of("/");
    if (extensionIndex == string::npos)
    {
        string errorMessage = __FILEREF__ + "No extension find in the asset file name"
                + ", imagePathName: " + imagePathName;
        logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    string outputFfmpegPathFileName =
            string("/tmp/")
            + imagePathName.substr(extensionIndex + 1)
            + ".generateScreenshot.log"
            ;
    
    string ffmpegExecuteCommand = 
            _ffmpegPath + "/ffmpeg "
            + "-y -i " + mmsAssetPathName + " "
            + "-f mjpeg -ss " + to_string(timePositionInSeconds) + " "
            + "-vframes 1 -an -s " + to_string(sourceImageWidth) + "x" + to_string(sourceImageHeight) + " "
            + imagePathName + " "
            + "> " + outputFfmpegPathFileName + " "
            + "2>&1"
            ;

    #ifdef __APPLE__
        ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

    logger->info(__FILEREF__ + "Executing ffmpeg command"
        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
    );

    try
    {
        int executeCommandStatus = ProcessUtility::execute (ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "ffmpeg command failed"
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;

            logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(exception e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffmpeg command failed"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
        ;
        logger->error(errorMessage);

        bool exceptionInCaseOfError = false;
        FileIO::remove(outputFfmpegPathFileName, exceptionInCaseOfError);

        throw e;
    }

    bool inCaseOfLinkHasItToBeRead = false;
    unsigned long ulFileSize = FileIO::getFileSizeInBytes (
        imagePathName, inCaseOfLinkHasItToBeRead);

    if (ulFileSize == 0)
    {
        string errorMessage = __FILEREF__ + "ffmpeg command failed, image file size is 0"
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        ;
        logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }    
}

int EncoderVideoAudioProxy::getEncodingProgress()
{
    int encodingPercentage;


    try
    {
        if (!FileIO::isFileExisting(_outputFfmpegPathFileName.c_str()))
        {
            _logger->info(__FILEREF__ + "Encoding status not available"
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
            );

            throw EncodingStatusNotAvailable();
        }

        string ffmpegEncodingStatus;
        try
        {
            int lastCharsToBeRead = 512;
            
            ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeRead);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "Failure reading the encoding status file"
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
            );

            throw EncodingStatusNotAvailable();
        }

        {
            // frame= 2315 fps= 98 q=27.0 q=28.0 size=    6144kB time=00:01:32.35 bitrate= 545.0kbits/s speed=3.93x    
            
            smatch m;   // typedef std:match_result<string>

            regex e("time=([^ ]+)");

            bool match = regex_search(ffmpegEncodingStatus, m, e);

            // m is where the result is saved
            // we will have three results: the entire match, the first submatch, the second submatch
            // giving the following input: <email>user@gmail.com<end>
            // m.prefix(): everything is in front of the matched string (<email> in the previous example)
            // m.suffix(): everything is after the matched string (<end> in the previous example)

            /*
            _logger->info(string("m.size(): ") + to_string(m.size()) + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus);
            for (int n = 0; n < m.size(); n++)
            {
                _logger->info(string("m[") + to_string(n) + "]: str()=" + m[n].str());
            }
            cout << "m.prefix().str(): " << m.prefix().str() << endl;
            cout << "m.suffix().str(): " << m.suffix().str() << endl;
             */

            if (m.size() >= 2)
            {
                string duration = m[1].str();   // 00:01:47.87

                stringstream ss(duration);
                string hours;
                string minutes;
                string seconds;
                string roughMicroSeconds;    // microseconds???
                char delim = ':';

                getline(ss, hours, delim); 
                getline(ss, minutes, delim); 

                delim = '.';
                getline(ss, seconds, delim); 
                getline(ss, roughMicroSeconds, delim); 

                int iHours = atoi(hours.c_str());
                int iMinutes = atoi(minutes.c_str());
                int iSeconds = atoi(seconds.c_str());
                int iRoughMicroSeconds = atoi(roughMicroSeconds.c_str());

                double encodingSeconds = (iHours * 3600) + (iMinutes * 60) + (iSeconds) + (iRoughMicroSeconds / 100);
                double currentTimeInMilliSeconds = (encodingSeconds * 1000) + (_currentlyAtSecondPass ? _encodingItem->_durationInMilliSeconds : 0);
                //  encodingSeconds : _encodingItem->videoOrAudioDurationInMilliSeconds = x : 100
                
                encodingPercentage = 100 * currentTimeInMilliSeconds / (_encodingItem->_durationInMilliSeconds * (_twoPasses ? 2 : 1));

                _logger->info(__FILEREF__ + "Encoding status"
                    + ", duration: " + duration
                    + ", encodingSeconds: " + to_string(encodingSeconds)
                    + ", _twoPasses: " + to_string(_twoPasses)
                    + ", _currentlyAtSecondPass: " + to_string(_currentlyAtSecondPass)
                    + ", currentTimeInMilliSeconds: " + to_string(currentTimeInMilliSeconds)
                    + ", _encodingItem->_durationInMilliSeconds: " + to_string(_encodingItem->_durationInMilliSeconds)
                    + ", encodingPercentage: " + to_string(encodingPercentage)
                );
            }
        }
    }
    catch(...)
    {
        throw EncodingStatusNotAvailable();
    }

    
    return encodingPercentage;
}

string EncoderVideoAudioProxy::encodeContent_VideoAudio_through_ffmpeg()
{
    
    string stagingEncodedAssetPathName;
    string encodedFileName;
    string mmsSourceAssetPathName;
    // stagingEncodedAssetPathName preparation
    {
        mmsSourceAssetPathName = _mmsStorage->getMMSAssetPathName(
            _encodingItem->_mmsPartitionNumber,
            _encodingItem->_customer->_directoryName,
            _encodingItem->_relativePath,
            _encodingItem->_fileName);

        size_t extensionIndex = _encodingItem->_fileName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extension find in the asset file name"
                    + ", _encodingItem->_fileName: " + _encodingItem->_fileName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName =
                _encodingItem->_fileName.substr(0, extensionIndex)
                + "_" 
                + to_string(_encodingItem->_encodingProfileKey);
        if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MP4)
            encodedFileName.append(".mp4");
        else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS ||
                _encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe)
            ;
        else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WEBM)
            encodedFileName.append(".webm");
        else
        {
            string errorMessage = __FILEREF__ + "Unknown technology";
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool removeLinuxPathIfExist = true;
        stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
            _encodingItem->_customer->_directoryName,
            _encodingItem->_relativePath,
            encodedFileName,
            -1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
            -1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
            removeLinuxPathIfExist);

        if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS ||
            _encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe)
        {
            // In this case, the path is a directory where to place the segments

            if (!FileIO::directoryExisting(stagingEncodedAssetPathName)) 
            {
                _logger->info(__FILEREF__ + "Create directory"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                );

                bool noErrorIfExists = true;
                bool recursive = true;
                FileIO::createDirectory(
                        stagingEncodedAssetPathName,
                        S_IRUSR | S_IWUSR | S_IXUSR |
                        S_IRGRP | S_IXGRP |
                        S_IROTH | S_IXOTH, noErrorIfExists, recursive);
            }        
        }
    }

    
    // encoding
    try
    {
        bool segmentFileFormat;    
        string ffmpegFileFormatParameter = "";

        string ffmpegVideoCodecParameter = "";
        string ffmpegVideoProfileParameter = "";
        string ffmpegVideoResolutionParameter = "";
        string ffmpegVideoBitRateParameter = "";
        string ffmpegVideoMaxRateParameter = "";
        string ffmpegVideoBufSizeParameter = "";
        string ffmpegVideoFrameRateParameter = "";
        string ffmpegVideoKeyFramesRateParameter = "";

        string ffmpegAudioCodecParameter = "";
        string ffmpegAudioBitRateParameter = "";

        settingFfmpegPatameters(
            stagingEncodedAssetPathName,

            segmentFileFormat,
            ffmpegFileFormatParameter,

            ffmpegVideoCodecParameter,
            ffmpegVideoProfileParameter,
            ffmpegVideoResolutionParameter,
            ffmpegVideoBitRateParameter,
            _twoPasses,
            ffmpegVideoMaxRateParameter,
            ffmpegVideoBufSizeParameter,
            ffmpegVideoFrameRateParameter,
            ffmpegVideoKeyFramesRateParameter,

            ffmpegAudioCodecParameter,
            ffmpegAudioBitRateParameter
        );

        string ffmpegoutputPathName = string("")
                + to_string(_encodingItem->_physicalPathKey)
                + ".ffmpegoutput";
        _outputFfmpegPathFileName = _mmsStorage->getStagingAssetPathName (
            _encodingItem->_customer->_directoryName,
            _encodingItem->_relativePath,
            ffmpegoutputPathName,
            -1,         // long long llMediaItemKey,
            -1,         // long long llPhysicalPathKey,
            true // removeLinuxPathIfExist
        );

        if (segmentFileFormat)
        {
            string stagingEncodedSegmentAssetPathName =
                    stagingEncodedAssetPathName 
                    + "/"
                    + encodedFileName
                    + "_%04d.ts"
            ;

            string ffmpegExecuteCommand =
                    _ffmpegPath + "/ffmpeg "
                    + "-y -i " + mmsSourceAssetPathName + " "
                    + ffmpegVideoCodecParameter
                    + ffmpegVideoProfileParameter
                    + "-preset slow "
                    + ffmpegVideoBitRateParameter
                    + ffmpegVideoMaxRateParameter
                    + ffmpegVideoBufSizeParameter
                    + ffmpegVideoFrameRateParameter
                    + ffmpegVideoKeyFramesRateParameter
                    + ffmpegVideoResolutionParameter
                    + "-threads 0 "
                    + ffmpegAudioCodecParameter
                    + ffmpegAudioBitRateParameter
                    + ffmpegFileFormatParameter
                    + stagingEncodedSegmentAssetPathName + " "
                    + "> " + _outputFfmpegPathFileName + " "
                    + "2>&1"
            ;

            #ifdef __APPLE__
                ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
            #endif

            _logger->info(__FILEREF__ + "Executing ffmpeg command"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            );

            try
            {
                int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
                if (executeCommandStatus != 0)
                {
                    string errorMessage = __FILEREF__ + "ffmpeg command failed"
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            catch(exception e)
            {
                string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
                string errorMessage = __FILEREF__ + "ffmpeg command failed"
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                ;
                _logger->error(errorMessage);

                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

                throw e;
            }

            bool exceptionInCaseOfError = false;
            FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

            _logger->info(__FILEREF__ + "Encoded file generated"
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // changes to be done to the manifest, see EncoderThread.cpp
        }
        else
        {
            string ffmpegExecuteCommand;
            if (_twoPasses)
            {
                string passLogFileName = string("")
                    + to_string(_encodingItem->_physicalPathKey)
                    + "_"
                    + encodedFileName
                    + ".passlog"
                    ;

                // bool removeLinuxPathIfExist = true;
                string ffmpegPassLogPathFileName = _mmsStorage->getStagingAssetPathName (
                    _encodingItem->_customer->_directoryName,
                    _encodingItem->_relativePath,
                    passLogFileName,
                    -1,         // long long llMediaItemKey,
                    -1,         // long long llPhysicalPathKey,
                    true    // removeLinuxPathIfExist
                );

                ffmpegExecuteCommand =
                        _ffmpegPath + "/ffmpeg "
                        + "-y -i " + mmsSourceAssetPathName + " "
                        + ffmpegVideoCodecParameter
                        + ffmpegVideoProfileParameter
                        + "-preset slow "
                        + ffmpegVideoBitRateParameter
                        + ffmpegVideoMaxRateParameter
                        + ffmpegVideoBufSizeParameter
                        + ffmpegVideoFrameRateParameter
                        + ffmpegVideoKeyFramesRateParameter
                        + ffmpegVideoResolutionParameter
                        + "-threads 0 "
                        + "-pass 1 -passlogfile " + ffmpegPassLogPathFileName + " "
                        + "-an "
                        + ffmpegFileFormatParameter
                        + "/dev/null "
                        + "> " + _outputFfmpegPathFileName + " "
                        + "2>&1"
                ;

                #ifdef __APPLE__
                    ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
                #endif

                _logger->info(__FILEREF__ + "Executing ffmpeg command"
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                );

                try
                {
                    int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "ffmpeg command failed"
                                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                catch(exception e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
                    string errorMessage = __FILEREF__ + "ffmpeg command failed"
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                    ;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
                    FileIO::remove(ffmpegPassLogPathFileName, exceptionInCaseOfError);
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

                    throw e;
                }

                ffmpegExecuteCommand =
                        _ffmpegPath + "/ffmpeg "
                        + "-y -i " + mmsSourceAssetPathName + " "
                        + ffmpegVideoCodecParameter
                        + ffmpegVideoProfileParameter
                        + "-preset slow "
                        + ffmpegVideoBitRateParameter
                        + ffmpegVideoMaxRateParameter
                        + ffmpegVideoBufSizeParameter
                        + ffmpegVideoFrameRateParameter
                        + ffmpegVideoKeyFramesRateParameter
                        + ffmpegVideoResolutionParameter
                        + "-threads 0 "
                        + "-pass 2 -passlogfile " + ffmpegPassLogPathFileName + " "
                        + ffmpegAudioCodecParameter
                        + ffmpegAudioBitRateParameter
                        + ffmpegFileFormatParameter
                        + stagingEncodedAssetPathName + " "
                        + "> " + _outputFfmpegPathFileName 
                        + " 2>&1"
                ;

                #ifdef __APPLE__
                    ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
                #endif

                _logger->info(__FILEREF__ + "Executing ffmpeg command"
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                );

                _currentlyAtSecondPass = true;
                try
                {
                    int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "ffmpeg command failed"
                                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                catch(exception e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
                    string errorMessage = __FILEREF__ + "ffmpeg command failed"
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                    ;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
                    FileIO::remove(ffmpegPassLogPathFileName, exceptionInCaseOfError);
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

                    throw e;
                }

                bool exceptionInCaseOfError = false;
                FileIO::remove(ffmpegPassLogPathFileName, exceptionInCaseOfError);
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }
            else
            {
                ffmpegExecuteCommand =
                        _ffmpegPath + "/ffmpeg "
                        + "-y -i " + mmsSourceAssetPathName + " "
                        + ffmpegVideoCodecParameter
                        + ffmpegVideoProfileParameter
                        + "-preset slow "
                        + ffmpegVideoBitRateParameter
                        + ffmpegVideoMaxRateParameter
                        + ffmpegVideoBufSizeParameter
                        + ffmpegVideoFrameRateParameter
                        + ffmpegVideoKeyFramesRateParameter
                        + ffmpegVideoResolutionParameter
                        + "-threads 0 "
                        + ffmpegAudioCodecParameter
                        + ffmpegAudioBitRateParameter
                        + ffmpegFileFormatParameter
                        + stagingEncodedAssetPathName + " "
                        + "> " + _outputFfmpegPathFileName 
                        + " 2>&1"
                ;

                #ifdef __APPLE__
                    ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
                #endif

                _logger->info(__FILEREF__ + "Executing ffmpeg command"
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                );

                try
                {
                    int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "ffmpeg command failed"
                                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                catch(exception e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
                    string errorMessage = __FILEREF__ + "ffmpeg command failed"
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                    ;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

                    throw e;
                }

                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }

            _logger->info(__FILEREF__ + "Encoded file generated"
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            bool inCaseOfLinkHasItToBeRead = false;
            unsigned long ulFileSize = FileIO::getFileSizeInBytes (
                stagingEncodedAssetPathName, inCaseOfLinkHasItToBeRead);

            if (ulFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg command failed, encoded file size is 0"
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        } 
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg encode failed"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

        _logger->info(__FILEREF__ + "Remove"
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        // file in case of .3gp content OR directory in case of IPhone content
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
        {
            Boolean_t bRemoveRecursively = true;
            FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
        }
        else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
        {
            FileIO::remove(stagingEncodedAssetPathName);
        }
                
        throw e;
    }
    
    return stagingEncodedAssetPathName;
}

string EncoderVideoAudioProxy::getLastPartOfFile(
    string pathFileName, int lastCharsToBeRead)
{
    string lastPartOfFile = "";
    char* buffer = nullptr;

    auto logger = spdlog::get("mmsEngineService");

    try
    {
        ifstream ifPathFileName(pathFileName);
        if (ifPathFileName) 
        {
            int         charsToBeRead;
            
            // get length of file:
            ifPathFileName.seekg (0, ifPathFileName.end);
            int fileSize = ifPathFileName.tellg();
            if (fileSize >= lastCharsToBeRead)
            {
                ifPathFileName.seekg (fileSize - lastCharsToBeRead, ifPathFileName.beg);
                charsToBeRead = lastCharsToBeRead;
            }
            else
            {
                ifPathFileName.seekg (0, ifPathFileName.beg);
                charsToBeRead = fileSize;
            }

            buffer = new char [charsToBeRead];
            ifPathFileName.read (buffer, charsToBeRead);
            if (ifPathFileName)
            {
                // all characters read successfully
                lastPartOfFile.assign(buffer, charsToBeRead);                
            }
            else
            {
                // error: only is.gcount() could be read";
                lastPartOfFile.assign(buffer, ifPathFileName.gcount());                
            }
            ifPathFileName.close();

            delete[] buffer;
        }
    }
    catch(exception e)
    {
        if (buffer != nullptr)
            delete [] buffer;

        logger->error("getLastPartOfFile failed");        
    }

    return lastPartOfFile;
}

void EncoderVideoAudioProxy::settingFfmpegPatameters(
    string stagingEncodedAssetPathName,
        
    bool& segmentFileFormat,
    string& ffmpegFileFormatParameter,

    string& ffmpegVideoCodecParameter,
    string& ffmpegVideoProfileParameter,
    string& ffmpegVideoResolutionParameter,
    string& ffmpegVideoBitRateParameter,
    bool& twoPasses,
    string& ffmpegVideoMaxRateParameter,
    string& ffmpegVideoBufSizeParameter,
    string& ffmpegVideoFrameRateParameter,
    string& ffmpegVideoKeyFramesRateParameter,

    string& ffmpegAudioCodecParameter,
    string& ffmpegAudioBitRateParameter
)
{
    string field;
    Json::Value encodingProfileRoot;
    try
    {
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();
        string errors;

        bool parsingSuccessful = reader->parse(_encodingItem->_details.c_str(),
                _encodingItem->_details.c_str() + _encodingItem->_details.size(), 
                &encodingProfileRoot, &errors);
        delete reader;

        if (!parsingSuccessful)
        {
            string errorMessage = __FILEREF__ + "failed to parse the encoder details"
                    + ", details: " + _encodingItem->_details;
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }
    }
    catch(...)
    {
        throw runtime_error(string("wrong encoding profile json format")
                + ", _encodingItem->_details: " + _encodingItem->_details
                );
    }

    // fileFormat
    string fileFormat;
    {
        field = "fileFormat";
        if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        fileFormat = encodingProfileRoot.get(field, "XXX").asString();

        encodingFileFormatValidation(fileFormat);
        
        if (fileFormat == "segment")
        {
            segmentFileFormat = true;
            
            string stagingManifestAssetPathName =
                    stagingEncodedAssetPathName
                    + "/index.m3u8";
            
            ffmpegFileFormatParameter =
                    "-vbsf h264_mp4toannexb "
                    "-flags "
                    "-global_header "
                    "-map 0 "
                    "-f segment "
                    "-segment_time 10 "
                    "-segment_list " + stagingManifestAssetPathName + " "
            ;
        }
        else
        {
            segmentFileFormat = false;

            ffmpegFileFormatParameter =
                    " -f " + fileFormat + " "
            ;
        }
    }

    if (_encodingItem->_contentType == MMSEngineDBFacade::ContentType::Video)
    {
        field = "video";
        if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value videoRoot = encodingProfileRoot[field]; 

        // codec
        string codec;
        {
            field = "codec";
            if (!_mmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            codec = videoRoot.get(field, "XXX").asString();

            ffmpeg_encodingVideoCodecValidation(codec);

            ffmpegVideoCodecParameter   =
                    "-codec:v " + codec + " "
            ;
        }

        // profile
        {
            field = "profile";
            if (_mmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string profile = videoRoot.get(field, "XXX").asString();

                ffmpeg_encodingVideoProfileValidation(codec, profile);
                if (codec == "libx264")
                {
                    ffmpegVideoProfileParameter =
                            "-profile:v " + profile + " "
                    ;
                }
                else if (codec == "libvpx")
                {
                    ffmpegVideoProfileParameter =
                            "-quality " + profile + " "
                    ;
                }
                else
                {
                    string errorMessage = __FILEREF__ + "codec is wrong"
                            + ", codec: " + codec;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        // resolution
        {
            field = "width";
            if (!_mmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string width = videoRoot.get(field, "XXX").asString();
            if (width == "-1" && codec == "libx264")
                width   = "-2";     // h264 requires always a even width/height
        
            field = "height";
            if (!_mmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string height = videoRoot.get(field, "XXX").asString();
            if (height == "-1" && codec == "libx264")
                height   = "-2";     // h264 requires always a even width/height

            ffmpegVideoResolutionParameter =
                    "-vf scale=" + width + ":" + height + " "
            ;
        }

        // bitRate
        {
            field = "bitRate";
            if (!_mmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string bitRate = videoRoot.get(field, "XXX").asString();

            ffmpegVideoBitRateParameter =
                    "-b:v " + bitRate + " "
            ;
        }

        // bitRate
        {
            field = "twoPasses";
            if (!_mmsEngineDBFacade->isMetadataPresent(videoRoot, field) 
                    && fileFormat != "segment") // twoPasses is used ONLY if it is NOT segment
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            if (fileFormat != "segment")
                twoPasses = videoRoot.get(field, "XXX").asBool();
        }

        // maxRate
        {
            field = "maxRate";
            if (_mmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string maxRate = videoRoot.get(field, "XXX").asString();

                ffmpegVideoMaxRateParameter =
                        "-maxrate " + maxRate + " "
                ;
            }
        }

        // bufSize
        {
            field = "bufSize";
            if (_mmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string bufSize = videoRoot.get(field, "XXX").asString();

                ffmpegVideoBufSizeParameter =
                        "-bufsize " + bufSize + " "
                ;
            }
        }

        /*
        // frameRate
        {
            field = "frameRate";
            if (_mmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string frameRate = videoRoot.get(field, "XXX").asString();

                int iFrameRate = stoi(frameRate);

                ffmpegVideoFrameRateParameter =
                        "-r " + frameRate + " "
                ;

                // keyFrameIntervalInSeconds
                {
                    field = "keyFrameIntervalInSeconds";
                    if (_mmsEngineDBFacade->isMetadataPresent(videoRoot, field))
                    {
                        string keyFrameIntervalInSeconds = videoRoot.get(field, "XXX").asString();

                        int iKeyFrameIntervalInSeconds = stoi(keyFrameIntervalInSeconds);

                        ffmpegVideoKeyFramesRateParameter =
                                "-g " + to_string(iFrameRate * iKeyFrameIntervalInSeconds) + " "
                        ;
                    }
                }
            }
        }
         */
    }
    
    if (_encodingItem->_contentType == MMSEngineDBFacade::ContentType::Video ||
            _encodingItem->_contentType == MMSEngineDBFacade::ContentType::Audio)
    {
        field = "audio";
        if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value audioRoot = encodingProfileRoot[field]; 

        // codec
        {
            field = "codec";
            if (!_mmsEngineDBFacade->isMetadataPresent(audioRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string codec = audioRoot.get(field, "XXX").asString();

            ffmpeg_encodingAudioCodecValidation(codec);

            ffmpegAudioCodecParameter   =
                    "-acodec " + codec + " "
            ;
        }

        // bitRate
        {
            field = "bitRate";
            if (!_mmsEngineDBFacade->isMetadataPresent(audioRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string bitRate = audioRoot.get(field, "XXX").asString();

            ffmpegAudioBitRateParameter =
                    "-b:a " + bitRate + " "
            ;
        }
    }
}

void EncoderVideoAudioProxy::encodingFileFormatValidation(string fileFormat)
{    
    if (fileFormat != "3gp" 
            && fileFormat != "mp4" 
            && fileFormat != "webm" 
            && fileFormat != "segment"
            )
    {
        string errorMessage = __FILEREF__ + "fileFormat is wrong"
                + ", fileFormat: " + fileFormat;

        auto logger = spdlog::get("mmsEngineService");
        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void EncoderVideoAudioProxy::ffmpeg_encodingVideoCodecValidation(string codec)
{    
    if (codec != "libx264" && codec != "libvpx")
    {
        string errorMessage = __FILEREF__ + "Video codec is wrong"
                + ", codec: " + codec;

        auto logger = spdlog::get("mmsEngineService");
        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void EncoderVideoAudioProxy::ffmpeg_encodingVideoProfileValidation(
    string codec, string profile)
{
    auto logger = spdlog::get("mmsEngineService");

    if (codec == "libx264")
    {
        if (profile != "high" && profile != "baseline" && profile != "main")
        {
            string errorMessage = __FILEREF__ + "Profile is wrong"
                    + ", codec: " + codec
                    + ", profile: " + profile;

            logger->error(errorMessage);
        
            throw runtime_error(errorMessage);
        }
    }
    else if (codec == "libvpx")
    {
        if (profile != "best" && profile != "good")
        {
            string errorMessage = __FILEREF__ + "Profile is wrong"
                    + ", codec: " + codec
                    + ", profile: " + profile;

            logger->error(errorMessage);
        
            throw runtime_error(errorMessage);
        }
    }
    else
    {
        string errorMessage = __FILEREF__ + "codec is wrong"
                + ", codec: " + codec;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void EncoderVideoAudioProxy::ffmpeg_encodingAudioCodecValidation(string codec)
{    
    if (codec != "libaacplus" 
            && codec != "libfdk_aac" 
            && codec != "libvo_aacenc" 
            && codec != "libvorbis"
    )
    {
        string errorMessage = __FILEREF__ + "Audio codec is wrong"
                + ", codec: " + codec;

        auto logger = spdlog::get("mmsEngineService");
        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void EncoderVideoAudioProxy::processEncodedContentVideoAudio(string stagingEncodedAssetPathName)
{
    string encodedFileName;
    string mmsAssetPathName;
    unsigned long mmsPartitionIndexUsed;
    try
    {
        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        bool partitionIndexToBeCalculated = true;
        bool deliveryRepositoriesToo = true;

        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            stagingEncodedAssetPathName,
            _encodingItem->_customer->_directoryName,
            encodedFileName,
            _encodingItem->_relativePath,

            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

            deliveryRepositoriesToo,
            _encodingItem->_customer->_territories
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        throw e;
    }

    try
    {
        unsigned long long mmsAssetSizeInBytes;
        {
            FileIO::DirectoryEntryType_t detSourceFileType = 
                    FileIO::getDirectoryEntryType(mmsAssetPathName);

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
                    detSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                string errorMessage = __FILEREF__ + "Wrong directory entry type"
                        + ", mmsAssetPathName: " + mmsAssetPathName
                        ;

                _logger->error(errorMessage);
                throw runtime_error(errorMessage);
            }

            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                mmsAssetSizeInBytes = FileIO::getDirectorySizeInBytes(mmsAssetPathName);   
            }
            else
            {
                bool inCaseOfLinkHasItToBeRead = false;
                mmsAssetSizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                        inCaseOfLinkHasItToBeRead);   
            }
        }


        int64_t encodedPhysicalPathKey = _mmsEngineDBFacade->saveEncodedContentMetadata(
            _encodingItem->_customer->_customerKey,
            _encodingItem->_mediaItemKey,
            encodedFileName,
            _encodingItem->_relativePath,
            mmsPartitionIndexUsed,
            mmsAssetSizeInBytes,
            _encodingItem->_encodingProfileKey);
        
        _logger->info(__FILEREF__ + "Saved the Encoded content"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
            + ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveEncodedContentMetadata failed"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Remove"
            + ", mmsAssetPathName: " + mmsAssetPathName
        );

        // file in case of .3gp content OR directory in case of IPhone content
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
        {
            Boolean_t bRemoveRecursively = true;
            FileIO::removeDirectory(mmsAssetPathName, bRemoveRecursively);
        }
        else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
        {
            FileIO::remove(mmsAssetPathName);
        }

        throw e;
    }
}