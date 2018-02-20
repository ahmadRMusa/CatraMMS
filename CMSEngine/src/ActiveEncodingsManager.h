/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   EnodingsManager.h
 * Author: giuliano
 *
 * Created on February 4, 2018, 7:18 PM
 */

#ifndef ActiveEncodingsManager_h
#define ActiveEncodingsManager_h

#include <chrono>
#include <vector>
#include <condition_variable>
#include "EncoderVideoAudioProxy.h"
#include "CMSEngineDBFacade.h"
#include "CMSStorage.h"
#include "spdlog/spdlog.h"
#include "Magick++.h"

#define MAXHIGHENCODINGSTOBEMANAGED     30
#define MAXMEDIUMENCODINGSTOBEMANAGED  20
#define MAXLOWENCODINGSTOBEMANAGED      20

struct MaxEncodingsManagerCapacityReached: public exception {    
    char const* what() const throw() 
    {
        return "Max Encoding Manager capacity reached";
    }; 
};

class ActiveEncodingsManager {
public:
    ActiveEncodingsManager(        
            shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<CMSStorage> cmsStorage,
            shared_ptr<spdlog::logger> logger);

    virtual ~ActiveEncodingsManager();
    
    void operator ()();

    unsigned long addEncodingItems (
	vector<shared_ptr<CMSEngineDBFacade::EncodingItem>>& vEncodingItems);
    
    static void encodingImageFormatValidation(string newFormat);
    static Magick::InterlaceType encodingImageInterlaceTypeValidation(string sNewInterlaceType);

private:
    struct EncodingJob
    {
        EncodingJobStatus			_status;
        chrono::system_clock::time_point	_encodingJobStart;

        shared_ptr<CMSEngineDBFacade::EncodingItem>	_encodingItem;
        EncoderVideoAudioProxy                  _encoderVideoAudioProxy;
        
        EncodingJob()
        {
            _status         = EncodingJobStatus::Free;
        }
    };

    shared_ptr<spdlog::logger>                  _logger;
    shared_ptr<CMSEngineDBFacade>               _cmsEngineDBFacade;
    shared_ptr<CMSStorage>                      _cmsStorage;
    
    condition_variable                          _cvAddedEncodingJob;
    mutex                                       _mtEncodingJobs;
    
    EncodingJob     _highPriorityEncodingJobs[MAXHIGHENCODINGSTOBEMANAGED];
    EncodingJob     _mediumPriorityEncodingJobs[MAXMEDIUMENCODINGSTOBEMANAGED];
    EncodingJob     _lowPriorityEncodingJobs[MAXLOWENCODINGSTOBEMANAGED];

    #ifdef __FFMPEGLOCALENCODER__
        int                 _ffmpegEncoderRunning;
    #endif

    void processEncodingJob(mutex* mtEncodingJobs, EncodingJob* encodingJob);
    void addEncodingItem(shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem);
    string encodeContentImage(
        shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem);
    void processEncodedImage(
        shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem, 
        string stagingEncodedAssetPathName);

    void readingImageProfile(
        string profileDetails,
        string& newFormat,
        int& newWidth,
        int& newHeight,
        bool& newAspect,
        string& sNewInterlaceType,
        Magick::InterlaceType& newInterlaceType
    );
};

#endif

