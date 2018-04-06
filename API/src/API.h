/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   API.h
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#ifndef API_h
#define API_h

#include "APICommon.h"

class API: public APICommon {
public:
    struct FileUploadProgressData {
        struct RequestData {
            int64_t     _ingestionJobKey;
            string      _progressId;
            double      _lastPercentageUpdated;
            int         _callFailures;
            bool        _contentRangePresent;
            long long   _contentRangeStart;
            long long   _contentRangeEnd;
            long long   _contentRangeSize;
        };
        
        mutex                       _mutex;
        vector<RequestData>   _filesUploadProgressToBeMonitored;
    };
    
    API(Json::Value configuration, 
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            mutex* fcgiAcceptMutex,
            FileUploadProgressData* fileUploadProgressData,
            shared_ptr<spdlog::logger> logger);
    
    ~API();
    
    virtual void getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength);

    virtual void manageRequestAndResponse(
            FCGX_Request& request,
            string requestURI,
            string requestMethod,
            unordered_map<string, string> queryParameters,
            tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
            unsigned long contentLength,
            string requestBody,
            string xCatraMMSResumeHeader,
            unordered_map<string, string>& requestDetails
    );
    
    void fileUploadProgressCheck();
    void stopUploadFileProgressThread();

private:
    MMSEngineDBFacade::EncodingPriority _encodingPriorityCustomerDefaultValue;
    MMSEngineDBFacade::EncodingPeriod _encodingPeriodCustomerDefaultValue;
    int _maxIngestionsNumberCustomerDefaultValue;
    int _maxStorageInGBCustomerDefaultValue;
    unsigned long       _binaryBufferLength;
    unsigned long       _progressUpdatePeriodInSeconds;
    int                 _webServerPort;
    bool                _fileUploadProgressThreadShutdown;
    int                 _maxProgressCallFailures;
    string              _progressURI;
    FileUploadProgressData*     _fileUploadProgressData;
    

    void registerCustomer(
        FCGX_Request& request,
        string requestBody);
    
    void confirmCustomer(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters);

    void createAPIKey(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters);

    void ingestion(
        FCGX_Request& request,
        shared_ptr<Customer> customer,
        unordered_map<string, string> queryParameters,
        string requestBody);

    int64_t ingestionTask(shared_ptr<MySQLConnection> conn,
        shared_ptr<Customer> customer, int64_t ingestionRootKey,
            Json::Value taskRoot, 
        vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
        string& responseBody);
        
    void ingestionGroupOfTasks(shared_ptr<MySQLConnection> conn,
        shared_ptr<Customer> customer, int64_t ingestionRootKey,
            Json::Value groupOfTasksRoot, 
        vector <int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
        string& responseBody);

    void ingestionEvents(shared_ptr<MySQLConnection> conn,
        shared_ptr<Customer> customer, int64_t ingestionRootKey,
            Json::Value taskOrGroupOfTasksRoot, 
        vector<int64_t> dependOnIngestionJobKeys, string& responseBody);

    void uploadBinary(
        FCGX_Request& request,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool> customerAndFlags,
        // unsigned long contentLength,
            unordered_map<string, string>& requestDetails
    );
    
    void parseContentRange(string contentRange,
        long long& contentRangeStart,
        long long& contentRangeEnd,
        long long& contentRangeSize);
    
};

#endif /* POSTCUSTOMER_H */

